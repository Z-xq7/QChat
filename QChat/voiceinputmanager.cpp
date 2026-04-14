#include "voiceinputmanager.h"
#include <QAudioDevice>
#include <QMediaDevices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QUrlQuery>
#include <QDebug>
#include <QIODevice>
#include <QtMath>

// ============================================================
// HMAC-SHA256 实现（兼容所有 Qt 版本）
// ============================================================
static QByteArray hmacSha256(const QByteArray& key, const QByteArray& message)
{
    QByteArray keyPad(64, 0x00);
    if (key.size() > 64) {
        keyPad = QCryptographicHash::hash(key, QCryptographicHash::Sha256);
    } else {
        memcpy(keyPad.data(), key.data(), key.size());
    }

    QByteArray innerPad(64, 0x36);
    QByteArray outerPad(64, 0x5c);

    for (int i = 0; i < 64; ++i) {
        keyPad[i] = static_cast<char>(keyPad[i] ^ innerPad[i]);
    }
    QByteArray innerData = keyPad + message;
    QByteArray innerHash = QCryptographicHash::hash(innerData, QCryptographicHash::Sha256);

    for (int i = 0; i < 64; ++i) {
        keyPad[i] = static_cast<char>((keyPad[i] ^ innerPad[i]) ^ outerPad[i]);
    }
    QByteArray outerData = keyPad + innerHash;
    return QCryptographicHash::hash(outerData, QCryptographicHash::Sha256);
}

VoiceInputManager::VoiceInputManager(QObject *parent)
    : QObject(parent)
    , _lastSentPos(0)
{
    _silenceTimer = new QTimer(this);
    _silenceTimer->setInterval(500);
    connect(_silenceTimer, &QTimer::timeout, this, &VoiceInputManager::checkSilence);

    _volumeTimer = new QTimer(this);
    _volumeTimer->setInterval(40);  // 40ms 发送一帧音频
    connect(_volumeTimer, &QTimer::timeout, this, &VoiceInputManager::onAudioDataReady);

    _webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(_webSocket, &QWebSocket::connected, this, &VoiceInputManager::onWebSocketConnected);
    connect(_webSocket, &QWebSocket::disconnected, this, &VoiceInputManager::onWebSocketDisconnected);
    connect(_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &VoiceInputManager::onWebSocketError);
    connect(_webSocket, &QWebSocket::textMessageReceived, this, &VoiceInputManager::onWebSocketMessage);
}

VoiceInputManager::~VoiceInputManager()
{
    stopRecording();
    delete _webSocket;
}

void VoiceInputManager::prepare()
{
    // 预连接 WebSocket，但不开始录音
    // 如果正在连接中，不要重复调用
    if (_isConnecting) {
        return;
    }
    // 如果已连接，直接返回
    if (_isConnected) {
        return;
    }
    
    QString authUrl = generateAuthUrl();
    _webSocket->open(QUrl(authUrl));
    _isConnecting = true;
    _lastSentPos = 0;  // 重置发送位置
    qDebug() << "[VoiceInput] Preparing WebSocket connection...";
}

bool VoiceInputManager::isReady() const
{
    return _isConnected;
}

void VoiceInputManager::startRecording()
{
    if (_isRecording) return;

    // 检查 WebSocket 是否已连接
    if (!_isConnected) {
        qWarning() << "[VoiceInput] WebSocket not connected, cannot start recording";
        emit recognitionError("语音识别服务未连接");
        return;
    }

    _audioBuffer = new QBuffer(this);
    _audioBuffer->open(QIODevice::WriteOnly);

    QAudioFormat format;
    format.setSampleRate(SAMPLE_RATE);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (!inputDevice.isFormatSupported(format)) {
        qWarning() << "[VoiceInput] Audio format not supported";
        emit recognitionError("音频格式不支持");
        delete _audioBuffer;
        _audioBuffer = nullptr;
        return;
    }

    // 重置状态
    _lastSentPos = 0;
    _silenceCount = 0;
    qDebug() << "[VoiceInput] Starting recording, lastSentPos reset to 0";

    // 开始录音
    _audioSource = new QAudioSource(inputDevice, format, this);
    _audioSource->start(_audioBuffer);

    _isRecording = true;
    _volumeTimer->start();
    _silenceTimer->start();

    emit recordingStarted();
    qDebug() << "[VoiceInput] Recording started";
}

void VoiceInputManager::stopRecording()
{
    if (!_isRecording) return;

    _isRecording = false;
    _volumeTimer->stop();
    _silenceTimer->stop();

    // 停止录音
    if (_audioSource) {
        _audioSource->stop();
        delete _audioSource;
        _audioSource = nullptr;
    }

    // 获取剩余音频数据并发送
    if (_audioBuffer) {
        _audioBuffer->close();
        QByteArray remainingData = _audioBuffer->buffer().mid(_lastSentPos);
        delete _audioBuffer;
        _audioBuffer = nullptr;

        // 发送剩余数据（最后一帧，status=2）
        if (_isConnected && !remainingData.isEmpty()) {
            sendAudioChunk(remainingData, true);
        } else if (_isConnected) {
            sendEndFrame();
        }
    }

    // 延迟关闭 WebSocket，等待服务器返回最终结果
    // 使用 singleshot timer 在 500ms 后关闭
    QTimer::singleShot(500, this, [this]() {
        if (_webSocket->state() == QAbstractSocket::ConnectedState) {
            _webSocket->close();
        }
        _isConnected = false;
        _isConnecting = false;
        qDebug() << "[VoiceInput] WebSocket closed after delay";
    });

    emit recordingStopped();
    qDebug() << "[VoiceInput] Recording stopped, waiting for final result...";
}

void VoiceInputManager::cancelRecording()
{
    if (!_isRecording) return;

    _isRecording = false;
    _volumeTimer->stop();
    _silenceTimer->stop();

    if (_audioSource) {
        _audioSource->stop();
        delete _audioSource;
        _audioSource = nullptr;
    }

    if (_audioBuffer) {
        _audioBuffer->close();
        delete _audioBuffer;
        _audioBuffer = nullptr;
    }

    // 取消录音时也关闭 WebSocket
    if (_webSocket->state() == QAbstractSocket::ConnectedState) {
        _webSocket->close();
    }
    _isConnected = false;
    _isConnecting = false;

    emit recordingCancelled();
    qDebug() << "[VoiceInput] Recording cancelled";
}

bool VoiceInputManager::isRecording() const
{
    return _isRecording;
}

// ============================================================
// 定时发送音频数据（流式传输）
// ============================================================
void VoiceInputManager::onAudioDataReady()
{
    if (!_isRecording || !_audioBuffer || !_isConnected) {
        qDebug() << "[VoiceInput] onAudioDataReady skipped: recording=" << _isRecording 
                 << "buffer=" << (_audioBuffer != nullptr) << "connected=" << _isConnected;
        return;
    }

    QByteArray allData = _audioBuffer->buffer();
    int available = allData.size() - _lastSentPos;
    
    qDebug() << "[VoiceInput] onAudioDataReady: total=" << allData.size() 
             << "lastSent=" << _lastSentPos << "available=" << available;

    // 第一帧：发送首帧（包含参数）+ 第一块音频数据
    if (_lastSentPos == 0 && available > 0) {
        QByteArray chunk = allData.mid(0, qMin(available, CHUNK_SIZE));
        _lastSentPos += chunk.size();

        // 发送首帧（包含 common/business + 第一块音频）
        sendFirstFrame(chunk);

        // 计算音量
        int level = calculateVolumeLevel(allData);
        emit volumeChanged(level);
        return;
    }

    // 发送所有可用数据（每次一帧 1280 字节 = 40ms）
    int chunksSent = 0;
    while (available >= CHUNK_SIZE) {
        QByteArray chunk = allData.mid(_lastSentPos, CHUNK_SIZE);
        _lastSentPos += CHUNK_SIZE;
        available -= CHUNK_SIZE;

        // 中间帧 status = 1
        sendAudioChunk(chunk, false);
        chunksSent++;
    }
    
    if (chunksSent > 0) {
        qDebug() << "[VoiceInput] Sent" << chunksSent << "intermediate chunks";
    }

    // 计算音量
    if (!allData.isEmpty()) {
        int level = calculateVolumeLevel(allData);
        emit volumeChanged(level);
    }
}

int VoiceInputManager::calculateVolumeLevel(const QByteArray& data)
{
    if (data.size() < 2) return 0;

    const qint16* samples = reinterpret_cast<const qint16*>(data.constData());
    int sampleCount = data.size() / sizeof(qint16);

    // 只计算最近 500ms 的数据（16000Hz * 0.5s = 8000 样本）
    // 避免累积数据过多导致音量被平均
    int recentSamples = qMin(sampleCount, 8000);
    
    double sum = 0;
    for (int i = sampleCount - recentSamples; i < sampleCount; ++i) {
        if (i >= 0) {
            sum += static_cast<double>(samples[i]) * samples[i];
        }
    }
    double rms = qSqrt(sum / recentSamples);
    int level = static_cast<int>((rms / 32768.0) * 100);
    return qBound(0, level, 100);
}

void VoiceInputManager::checkSilence()
{
    if (!_isRecording || !_audioBuffer) return;

    QByteArray data = _audioBuffer->buffer();
    int level = calculateVolumeLevel(data);
    
    // 每2秒输出一次音量日志（避免日志过多）
    static int logCounter = 0;
    if (++logCounter >= 4) {
        qDebug() << "[VoiceInput] Volume level:" << level << "silenceCount:" << _silenceCount;
        logCounter = 0;
    }

    if (level < 3) {  // 降低静音阈值，避免误判
        _silenceCount++;
        if (_silenceCount >= SILENCE_THRESHOLD) {
            qDebug() << "[VoiceInput] Auto stop due to silence, level:" << level;
            stopRecording();
        }
    } else {
        if (_silenceCount > 0) {
            qDebug() << "[VoiceInput] Sound detected, level:" << level << "reset silence count";
        }
        _silenceCount = 0;
    }
}

// ============================================================
// 讯飞 API 鉴权
// ============================================================
QString VoiceInputManager::generateAuthUrl()
{
    // 讯飞语音听写 API 地址
    QString host = "iat-api.xfyun.cn";
    QString path = "/v2/iat";
    QString url = "wss://" + host + path;

    QLocale cLocale(QLocale::C);
    QString date = cLocale.toString(QDateTime::currentDateTimeUtc(),
                                    "ddd, dd MMM yyyy HH:mm:ss") + " GMT";

    QString signatureOrigin = QString("host: %1\ndate: %2\nGET %3 HTTP/1.1")
        .arg(host).arg(date).arg(path);

    QByteArray signature = hmacSha256(_apiSecret.toUtf8(), signatureOrigin.toUtf8()).toBase64();

    QString authOrigin = QString("api_key=\"%1\", algorithm=\"hmac-sha256\", "
                                 "headers=\"host date request-line\", signature=\"%2\"")
        .arg(_apiKey).arg(QString::fromLatin1(signature));
    QByteArray authorization = authOrigin.toUtf8().toBase64();

    // 按照文档顺序：host, date, authorization
    QUrlQuery query;
    query.addQueryItem("host", host);
    query.addQueryItem("date", date);
    query.addQueryItem("authorization", QString::fromLatin1(authorization));

    return url + "?" + query.toString();
}

// ============================================================
// WebSocket 回调
// ============================================================
void VoiceInputManager::onWebSocketConnected()
{
    qDebug() << "[VoiceInput] WebSocket connected";
    _isConnected = true;
    _isConnecting = false;

    emit readyChanged(true);
}

void VoiceInputManager::onWebSocketDisconnected()
{
    qDebug() << "[VoiceInput] WebSocket disconnected";
    _isConnected = false;
    _isConnecting = false;
    emit readyChanged(false);
    emit recognitionComplete();
}

void VoiceInputManager::onWebSocketError(QAbstractSocket::SocketError)
{
    qWarning() << "[VoiceInput] WebSocket error";
    emit recognitionError("网络连接错误");
}

void VoiceInputManager::onWebSocketMessage(const QString& message)
{
    qDebug() << "[VoiceInput] WebSocket message received:" << message.left(200);
    
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "[VoiceInput] Invalid JSON response";
        return;
    }

    QJsonObject obj = doc.object();
    int code = obj["code"].toInt();
    QString sid = obj["sid"].toString();

    if (code != 0) {
        QString errorMsg = obj["message"].toString();
        qWarning() << "[VoiceInput] Recognition error:" << code << errorMsg << "sid:" << sid;
        emit recognitionError(QString("识别错误: %1").arg(errorMsg));
        return;
    }

    QJsonObject data = obj["data"].toObject();
    int status = data["status"].toInt();
    QJsonObject result = data["result"].toObject();
    QJsonArray wsArray = result["ws"].toArray();

    QString text;
    for (const QJsonValue& ws : wsArray) {
        QJsonArray cwArray = ws.toObject()["cw"].toArray();
        for (const QJsonValue& cw : cwArray) {
            text += cw.toObject()["w"].toString();
        }
    }

    bool isFinal = (status == 2);
    qDebug() << "[VoiceInput] Recognized text:" << text << "isFinal:" << isFinal << "status:" << status;
    
    if (!text.isEmpty()) {
        emit recognitionResult(text, isFinal);
    }

    if (isFinal) {
        qDebug() << "[VoiceInput] Recognition complete, closing WebSocket";
        _webSocket->close();
    }
}

// ============================================================
// 发送音频数据
// ============================================================
QString VoiceInputManager::buildRequestJson()
{
    QJsonObject common{{"app_id", _appId}};
    QJsonObject business{
        {"language", "zh_cn"},
        {"domain", "iat"},
        {"accent", "mandarin"},
        {"vad_eos", 3000},
        {"dwa", "wpgs"}
    };
    QJsonObject data{
        {"status", 0},  // 第一帧
        {"format", "audio/L16;rate=16000"},
        {"encoding", "raw"},
        {"audio", ""}
    };
    QJsonObject root{
        {"common", common},
        {"business", business},
        {"data", data}
    };
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void VoiceInputManager::sendFirstFrame(const QByteArray& chunk)
{
    if (!_isConnected) return;

    // app_id 必须是字符串
    QJsonObject common{{"app_id", _appId}};
    QJsonObject business{
        {"language", "zh_cn"},
        {"domain", "iat"},
        {"accent", "mandarin"},
        {"vad_eos", 3000},
        {"dwa", "wpgs"},
        {"ptt", 1}
    };
    QJsonObject data{
        {"status", 0},  // 第一帧
        {"format", "audio/L16;rate=16000"},
        {"encoding", "raw"},
        {"audio", QString::fromLatin1(chunk.toBase64())}
    };
    QJsonObject root{
        {"common", common},
        {"business", business},
        {"data", data}
    };
    
    QString jsonStr = QJsonDocument(root).toJson(QJsonDocument::Compact);
    qDebug() << "[VoiceInput] Sending first frame:" << jsonStr.left(300) << "...";
    _webSocket->sendTextMessage(jsonStr);
    qDebug() << "[VoiceInput] First frame sent, audio size:" << chunk.size();
}

void VoiceInputManager::sendAudioChunk(const QByteArray& chunk, bool isLast)
{
    if (!_isConnected) return;

    int status = isLast ? 2 : 1;  // 1=中间帧, 2=最后一帧

    QJsonObject data{
        {"status", status},
        {"format", "audio/L16;rate=16000"},
        {"encoding", "raw"},
        {"audio", QString::fromLatin1(chunk.toBase64())}
    };
    QJsonObject root{{"data", data}};

    QString jsonStr = QJsonDocument(root).toJson(QJsonDocument::Compact);
    _webSocket->sendTextMessage(jsonStr);
    qDebug() << "[VoiceInput] Sent chunk, status=" << status << "size=" << chunk.size();
}

void VoiceInputManager::sendEndFrame()
{
    if (!_isConnected) return;

    QJsonObject data{
        {"status", 2},
        {"format", "audio/L16;rate=16000"},
        {"encoding", "raw"},
        {"audio", ""}
    };
    QJsonObject root{{"data", data}};
    _webSocket->sendTextMessage(QJsonDocument(root).toJson(QJsonDocument::Compact));
}
