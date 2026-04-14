#ifndef VOICEINPUTMANAGER_H
#define VOICEINPUTMANAGER_H

#include <QObject>
#include <QAudioSource>
#include <QAudioFormat>
#include <QBuffer>
#include <QByteArray>
#include <QWebSocket>
#include <QTimer>
#include <QDateTime>

// ============================================================
// VoiceInputManager - 语音输入管理器
// 
// 功能：
// 1. 音频录制（Qt Multimedia - QAudioSource）
// 2. 讯飞语音识别 API 调用（WebSocket 流式）
// 3. 音量检测和自动停止
// ============================================================
class VoiceInputManager : public QObject
{
    Q_OBJECT
public:
    explicit VoiceInputManager(QObject *parent = nullptr);
    ~VoiceInputManager();

    // 控制接口
    void prepare();             // 预连接 WebSocket（在按钮按下前调用）
    void startRecording();      // 开始录音
    void stopRecording();       // 停止录音并发送识别
    void cancelRecording();     // 取消录音
    bool isRecording() const;
    bool isReady() const;       // WebSocket 是否已连接就绪

    // 配置
    void setAppId(const QString& appId) { _appId = appId; }
    void setApiKey(const QString& apiKey) { _apiKey = apiKey; }
    void setApiSecret(const QString& apiSecret) { _apiSecret = apiSecret; }

signals:
    void recordingStarted();                    // 开始录音
    void recordingStopped();                    // 停止录音
    void recordingCancelled();                  // 取消录音
    void readyChanged(bool ready);              // WebSocket 就绪状态变化
    void volumeChanged(int level);              // 音量变化 (0-100)
    void recognitionResult(const QString& text, bool isFinal);  // 识别结果
    void recognitionError(const QString& error);                // 识别错误
    void recognitionComplete();                 // 识别完成

private slots:
    void onAudioDataReady();                    // 音频数据就绪（定时读取并流式发送）
    void onWebSocketConnected();                // WebSocket 连接成功
    void onWebSocketDisconnected();             // WebSocket 断开
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketMessage(const QString& message);
    void checkSilence();                        // 检测静音自动停止

private:
    // 讯飞 API 相关
    QString generateAuthUrl();                  // 生成鉴权 URL
    void sendFirstFrame(const QByteArray& chunk);  // 发送首帧（含参数+音频）
    void sendAudioChunk(const QByteArray& chunk, bool isLast);  // 发送音频分片
    void sendEndFrame();                        // 发送结束帧
    QString buildRequestJson();                 // 构建请求 JSON（已废弃，用sendFirstFrame）

    // 音频处理
    int calculateVolumeLevel(const QByteArray& data);  // 计算音量

    // 成员变量
    QAudioSource* _audioSource = nullptr;       // Qt 6 使用 QAudioSource 而非 QAudioInput
    QBuffer* _audioBuffer = nullptr;
    QWebSocket* _webSocket = nullptr;
    QTimer* _silenceTimer = nullptr;
    QTimer* _volumeTimer = nullptr;

    // 配置
    QString _appId = "62fa7aec";
    QString _apiKey = "afd43ab01c46e8dd73299e1488cfbb59";
    QString _apiSecret = "N2ZiYzA1MTBiODU3ZTNmY2QwMzk1NzE5";

    // 状态
    bool _isRecording = false;
    bool _isConnected = false;
    bool _isConnecting = false;     // 是否正在连接中
    int _lastSentPos = 0;           // 上次发送音频数据的位置（流式发送用）
    int _silenceCount = 0;          // 静音计数
    static constexpr int SILENCE_THRESHOLD = 10;  // 连续10次静音停止（5秒）
    static constexpr int SAMPLE_RATE = 16000;    // 采样率 16kHz
    static constexpr int CHUNK_SIZE = 1280;      // 每帧音频大小（40ms）
};

#endif // VOICEINPUTMANAGER_H
