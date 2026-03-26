#include "YangRtcWrapper.h"
#include <QDebug>
#include <QTimer>
#include <QThread>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRandomGenerator>

#include <memory>
#include <new>

static QString yang_normalizeSdp(QString sdp)
{
    if (sdp.contains('`')) {
        sdp.replace('`', "");
    }
    if (sdp.contains("\\n") || sdp.contains("\\r")) {
        sdp.replace("\\r\\n", "\n");
        sdp.replace("\\n", "\n");
        sdp.replace("\\r", "\n");
    }
    sdp.replace("\r\n", "\n");
    sdp.replace("\r", "\n");
    sdp.replace("\n", "\r\n");
    QString filtered;
    filtered.reserve(sdp.size());
    for (const QChar& ch : sdp) {
        const ushort u = ch.unicode();
        if (u == '\r' || u == '\n') {
            filtered.push_back(ch);
            continue;
        }
        if (u >= 0x20 && u <= 0x7E) {
            filtered.push_back(ch);
        }
    }
    sdp = filtered;
    QStringList lines = sdp.split("\r\n");
    QStringList cleaned;
    cleaned.reserve(lines.size());
    for (const QString& line : lines) {
        const QString t = line.trimmed();
        if (t.isEmpty()) continue;
        cleaned.push_back(t);
    }
    sdp = cleaned.join("\r\n");
    if (!sdp.endsWith("\r\n")) {
        sdp.append("\r\n");
    }
    return sdp;
}

static bool yang_isAsciiSdpSafe(const QString& sdp, int* badIndex, ushort* badCode)
{
    const int n = sdp.size();
    for (int i = 0; i < n; ++i) {
        const ushort u = sdp.at(i).unicode();
        if (u == '\r' || u == '\n') continue;
        if (u < 0x20 || u > 0x7E) {
            if (badIndex) *badIndex = i;
            if (badCode) *badCode = u;
            return false;
        }
    }
    return true;
}

static QString yang_minimalizeSdp(const QString& sdp)
{
    const QStringList lines = sdp.split("\r\n");
    QStringList out;
    out.reserve(lines.size());

    bool inMedia = false;
    QString currentMedia;

    auto keepSessionLine = [&](const QString& t) -> bool {
        if (t.startsWith("v=")) return true;
        if (t.startsWith("o=")) return true;
        if (t.startsWith("s=")) return true;
        if (t.startsWith("t=")) return true;
        if (t.startsWith("a=group:BUNDLE")) return true;
        if (t.startsWith("a=msid-semantic:")) return true;
        return false;
    };

    auto keepMediaLine = [&](const QString& t) -> bool {
        if (t.startsWith("m=")) return true;
        if (t.startsWith("c=")) return true;
        if (t.startsWith("a=mid:")) return true;
        if (t.startsWith("a=ice-ufrag:")) return true;
        if (t.startsWith("a=ice-pwd:")) return true;
        if (t.startsWith("a=fingerprint:")) return true;
        if (t.startsWith("a=setup:")) return true;
        if (t == "a=sendrecv" || t == "a=sendonly" || t == "a=recvonly" || t == "a=inactive") return true;
        if (t == "a=rtcp-mux") return true;
        if (t.startsWith("a=rtpmap:")) return true;
        if (t.startsWith("a=fmtp:")) return true;
        return false;
    };

    for (const QString& line : lines) {
        const QString t = line.trimmed();
        if (t.isEmpty()) continue;
        if (t.startsWith("m=")) {
            inMedia = true;
            currentMedia = t;
            out.push_back(t);
            continue;
        }
        if (!inMedia) {
            if (keepSessionLine(t)) out.push_back(t);
            continue;
        }
        if (!keepMediaLine(t)) continue;
        if (t.startsWith("a=candidate:")) continue;
        if (t.startsWith("a=end-of-candidates")) continue;
        if (t.startsWith("a=ssrc:")) continue;
        if (t.startsWith("a=rtcp-fb:")) continue;
        out.push_back(t);
    }

    QString minimal = out.join("\r\n");
    if (!minimal.endsWith("\r\n")) minimal.append("\r\n");
    return minimal;
}

QHash<void*, YangRtcWrapper*> YangRtcWrapper::s_wrapperMap;

YangRtcWrapper::YangRtcWrapper(QObject *parent)
    : QObject(parent)
    , m_state(YangRtcState::Idle)
    , m_localVideoCallback(nullptr)
    , m_remoteVideoCallback(nullptr)
    , m_connectTimer(new QTimer(this))
{
    yang_init_avinfo(&m_avInfo);
    m_avInfo.sys.mediaServer = Yang_Server_P2p;

    // 显式指定 ICE candidate 类型（Full ICE），避免走 ICE-Lite 生成 `a=ice-lite` SDP。
    // 默认先用 Host；如果后续配置了 TURN/STUN，可在 configureIceServers() 中再切换。
    m_avInfo.rtc.iceCandidateType = YangIceHost;

    m_avInfo.rtc.iceUsingLocalIp = yangfalse;
    m_avInfo.rtc.enableAudioBuffer = yangtrue;
    m_avInfo.rtc.iceUsingLocalIp = yangtrue;
    m_avInfo.rtc.enableHttpServerSdp = yangfalse;
    m_avInfo.rtc.sessionTimeout = 30;
    m_avInfo.rtc.rtcSocketProtocol = Yang_Socket_Protocol_Udp;
    m_avInfo.rtc.turnSocketProtocol = Yang_Socket_Protocol_Udp;
    m_avInfo.rtc.rtcPort = 0;
    m_avInfo.rtc.rtcLocalPort = 0;
    m_avInfo.audio.sample  = 48000;
    m_avInfo.audio.channel = 2;
    m_avInfo.audio.bitrate = 64000;
    m_avInfo.video.width  = 640;
    m_avInfo.video.height = 480;
    m_avInfo.video.frame  = 25;
    m_avInfo.video.rate   = 800;
    m_avInfo.video.videoCaptureFormat = YangI420;
    m_avInfo.video.videoEncoderFormat = YangI420;
    m_avInfo.video.videoDecoderFormat = YangI420;

    memset(&m_peerConnection, 0, sizeof(YangPeerConnection));
    yang_create_peerConnection(&m_peerConnection);
    YangPeer* peer = &m_peerConnection.peer;
    memset(peer, 0, sizeof(YangPeer));
    peer->conn = nullptr;
    peer->avinfo = &m_avInfo;
    memset(&peer->streamconfig, 0, sizeof(YangStreamConfig));

    // 默认双向发送接收，后续在 ready 信令中根据 isInitiator 调整
    peer->streamconfig.direction = YangSendrecv;
    peer->streamconfig.isControlled = yangtrue;

    // 将音视频接收回调指向静态函数，通过 streamconfig.recvCallback.context 传递 this
    peer->streamconfig.recvCallback.context = this;
    peer->streamconfig.recvCallback.receiveAudio = YangRtcWrapper::onRecvAudioFrame;
    peer->streamconfig.recvCallback.receiveVideo = YangRtcWrapper::onRecvVideoFrame;
    peer->streamconfig.recvCallback.receiveMsg   = YangRtcWrapper::onRecvMessageFrame;

    // 可选：配置 ICE 状态/连接状态回调，方便以后调试
    peer->streamconfig.iceCallback.context = this;
    peer->streamconfig.iceCallback.onConnectionStateChange = nullptr;
    peer->streamconfig.iceCallback.onIceStateChange = nullptr;

    // 初始化 peerConnection 本身（内部可能会用到 m_peer.avinfo 等）
    if (m_peerConnection.init) {
        m_peerConnection.init(peer);
    }

    // 绑定音视频/消息回调（这些回调函数用于PeerConnection级别的事件）
    m_peerConnection.on_audio   = YangRtcWrapper::onAudioFrame;
    m_peerConnection.on_video   = YangRtcWrapper::onVideoFrame;
    m_peerConnection.on_message = YangRtcWrapper::onMessageFrame;
    // 将当前实例添加到静态映射中
    s_wrapperMap.insert(peer, this);

    // 连接状态计时器：周期轮询，超时则关闭，避免 DTLS 死循环导致卡死
    m_connectTimer->setSingleShot(false);
    connect(m_connectTimer, &QTimer::timeout, this, [this]() {
        // 注意：不能用 static，否则多个 YangRtcWrapper 实例会共享重试计数，容易导致误判超时甚至异常流程
        // 这里用 QObject 动态属性保存每个实例自己的计数（避免修改头文件）。
        int retryCount = this->property("_yang_rtc_retryCount").toInt();
        const int kMaxRetry = 10; // 10 * 500ms = 5 秒最大等待时间

        if (m_state == YangRtcState::Connecting && m_peerConnection.isConnected) {
            // isConnected 的参数应使用当前 peer 实例
            if (m_peerConnection.isConnected(&m_peerConnection.peer)) {
                qDebug() << "[YangRtcWrapper] Peer connection established";
                this->setProperty("_yang_rtc_retryCount", 0);
                m_connectTimer->stop();
                setState(YangRtcState::Connected);
                return;
            }
        }

        // 仍未连上，增加重试计数
        retryCount++;
        this->setProperty("_yang_rtc_retryCount", retryCount);
        if (retryCount >= kMaxRetry) {
            qDebug() << "[YangRtcWrapper] Connect timeout, closing connection";
            this->setProperty("_yang_rtc_retryCount", 0);
            m_connectTimer->stop();
            emit sigError("RTC DTLS/ICE handshake timeout");
            closeConnection();
        }
    });
}

YangRtcWrapper::~YangRtcWrapper()
{
    closeConnection();
    yang_destroy_peerConnection(&m_peerConnection);

    // 从静态映射中移除
    s_wrapperMap.remove(&m_peerConnection.peer);
}

bool YangRtcWrapper::configureIceServers(const QJsonArray& iceServers)
{
    if (iceServers.isEmpty()) {
        qDebug() << "[YangRtcWrapper] No ICE servers provided";
        // 没有 STUN/TURN 时保持 Host 模式
        m_avInfo.rtc.iceCandidateType = YangIceHost;
        return true;
    }

    // 配置第一个ICE服务器（通常是TURN服务器）
    QJsonObject iceServerObj = iceServers[0].toObject();

    QString urlsStr;
    if (iceServerObj["urls"].isArray()) {
        QJsonArray urlsArr = iceServerObj["urls"].toArray();
        if (!urlsArr.isEmpty()) {
            urlsStr = urlsArr.at(0).toString();
        }
    } else {
        urlsStr = iceServerObj["urls"].toString();
    }

    QString username = iceServerObj["username"].toString();
    QString credential = iceServerObj["credential"].toString();

    if (urlsStr.isEmpty()) {
        qDebug() << "[YangRtcWrapper] ICE server urls empty";
        return false;
    }

    // 解析URL并提取服务器信息
    if (urlsStr.startsWith("turn:")) {
        QString serverUrl = urlsStr.mid(5); // 移除"turn:"前缀
        QStringList parts = serverUrl.split(":");
        if (parts.size() >= 2) {
            QString ip = parts[0];
            QString portStr = parts[1].split("?")[0];
            int port = portStr.toInt();

            strncpy(m_avInfo.rtc.iceServerIP, ip.toUtf8().constData(), sizeof(m_avInfo.rtc.iceServerIP) - 1);
            m_avInfo.rtc.iceServerPort = port;
            strncpy(m_avInfo.rtc.iceUserName, username.toUtf8().constData(), sizeof(m_avInfo.rtc.iceUserName) - 1);
            strncpy(m_avInfo.rtc.icePassword, credential.toUtf8().constData(), sizeof(m_avInfo.rtc.icePassword) - 1);

            m_avInfo.rtc.iceCandidateType = YangIceTurn;
            qDebug() << "[YangRtcWrapper] Configured TURN server:" << ip << ":" << port;
            return true;
        }
    } else if (urlsStr.startsWith("stun:")) {
        QString serverUrl = urlsStr.mid(5); // 移除"stun:"前缀
        QStringList parts = serverUrl.split(":");
        if (parts.size() >= 2) {
            QString ip = parts[0];
            QString portStr = parts[1].split("?")[0];
            int port = portStr.toInt();

            strncpy(m_avInfo.rtc.iceServerIP, ip.toUtf8().constData(), sizeof(m_avInfo.rtc.iceServerIP) - 1);
            m_avInfo.rtc.iceServerPort = port;

            m_avInfo.rtc.iceCandidateType = YangIceStun;
            qDebug() << "[YangRtcWrapper] Configured STUN server:" << ip << ":" << port;
            return true;
        }
    }

    qDebug() << "[YangRtcWrapper] Unsupported ICE url:" << urlsStr;
    return false;
}

bool YangRtcWrapper::initRtcConnection(const QString &serverUrl, int localPort)
{
    if (m_state != YangRtcState::Idle && m_state != YangRtcState::Disconnected && m_state != YangRtcState::Failed) {
        qDebug() << "[YangRtcWrapper] initRtcConnection called while in state" << static_cast<int>(m_state);
        closeConnection();
    }

    m_serverUrl = serverUrl;

    // 配置 RTC 相关信息
    if (localPort <= 0) {
        localPort = 20000 + QRandomGenerator::global()->bounded(20000);
    }

    m_avInfo.rtc.rtcLocalPort = localPort;
    m_peerConnection.peer.streamconfig.localPort = localPort;

    // P2P 模式下 remoteIp/remotePort 指向 TURN 服务器（如果已配置）
    if (m_avInfo.rtc.iceServerIP[0] != '\0' && m_avInfo.rtc.iceServerPort > 0) {
        strncpy(m_peerConnection.peer.streamconfig.remoteIp,
                m_avInfo.rtc.iceServerIP,
                sizeof(m_peerConnection.peer.streamconfig.remoteIp) - 1);
        m_peerConnection.peer.streamconfig.remotePort = m_avInfo.rtc.iceServerPort;
    }

    // 基本视频参数（已在构造函数设置，这里确保覆盖）
    m_avInfo.video.width  = 640;
    m_avInfo.video.height = 480;
    m_avInfo.video.frame  = 25;
    m_avInfo.video.rate   = 800; // kbps
    m_avInfo.video.videoCaptureFormat = YangI420;
    m_avInfo.video.videoEncoderFormat = YangI420;
    m_avInfo.video.videoDecoderFormat = YangI420;

    // 基本音频参数
    m_avInfo.audio.sample  = 48000;
    m_avInfo.audio.channel = 2;
    m_avInfo.audio.bitrate = 64000;

    setState(YangRtcState::Connecting);
    return true;
}

bool YangRtcWrapper::addAudioTrack()
{
    if (!m_peerConnection.addAudioTrack) return false;
    int32_t ret = m_peerConnection.addAudioTrack(&m_peerConnection.peer, Yang_AED_OPUS);
    if (ret != Yang_Ok) {
        qDebug() << "[YangRtcWrapper] addAudioTrack failed, ret=" << ret;
        return false;
    }
    qDebug() << "[YangRtcWrapper] addAudioTrack success";
    return true;
}

bool YangRtcWrapper::addVideoTrack()
{
    if (!m_peerConnection.addVideoTrack) return false;
    int32_t ret = m_peerConnection.addVideoTrack(&m_peerConnection.peer, Yang_VED_H264);
    if (ret != Yang_Ok) {
        qDebug() << "[YangRtcWrapper] addVideoTrack failed, ret=" << ret;
        return false;
    }
    qDebug() << "[YangRtcWrapper] addVideoTrack success";
    return true;
}

QString YangRtcWrapper::createOffer()
{
    if (m_state != YangRtcState::Connecting) {
        emit sigError("RTC connection not in connecting state");
        return {};
    }
    if (!m_peerConnection.createOffer) {
        emit sigError("createOffer not implemented in metartc");
        return {};
    }

    char *sdp = nullptr;
    int32_t ret = m_peerConnection.createOffer(&m_peerConnection.peer, &sdp);
    if (ret != Yang_Ok || !sdp) {
        emit sigError("Failed to create offer");
        return {};
    }
    QString offerSdp = QString::fromLatin1(sdp);
    offerSdp = yang_normalizeSdp(offerSdp);
    int badIndex = -1;
    ushort badCode = 0;
    if (!yang_isAsciiSdpSafe(offerSdp, &badIndex, &badCode)) {
        qDebug() << "[YangRtcWrapper] createOffer got non-ascii sdp, badIndex=" << badIndex
                 << " badCode=" << badCode
                 << " len=" << offerSdp.size();
        emit sigError("Created offer SDP contains invalid characters");
        return {};
    }
    qDebug() << "[YangRtcWrapper] createOffer success, len=" << offerSdp.size();
    return offerSdp;
}

QString YangRtcWrapper::createAnswer()
{
    if (m_state != YangRtcState::Connecting) {
        emit sigError("RTC connection not in connecting state");
        return {};
    }
    if (!m_peerConnection.createAnswer) {
        emit sigError("createAnswer not implemented in metartc");
        return {};
    }

    // 注意：原实现使用 4096 的固定栈缓冲，容易导致 SDP 被截断（尤其是 fingerprint/ice 参数较长时）。
    // metartc 的 createAnswer 签名为 (YangPeer*, char* answer)，需要调用方提供足够大的缓冲区。
    // 这里改为堆分配更大的缓冲，避免截断。
    constexpr int kMaxAnswerSdpSize = 64 * 1024; // 64KB
    std::unique_ptr<char[]> answer(new (std::nothrow) char[kMaxAnswerSdpSize]);
    if (!answer) {
        emit sigError("Out of memory when allocating answer SDP buffer");
        return {};
    }
    memset(answer.get(), 0, kMaxAnswerSdpSize);

    int32_t ret = m_peerConnection.createAnswer(&m_peerConnection.peer, answer.get());
    if (ret != Yang_Ok) {
        qDebug() << "[YangRtcWrapper] createAnswer failed, ret=" << ret;
        emit sigError("Failed to create answer");
        return {};
    }

    // 保险起见：确保以 '\0' 终止
    answer[kMaxAnswerSdpSize - 1] = '\0';

    QString sdp = QString::fromLatin1(answer.get());
    sdp = yang_normalizeSdp(sdp);
    int badIndex = -1;
    ushort badCode = 0;
    if (!yang_isAsciiSdpSafe(sdp, &badIndex, &badCode)) {
        qDebug() << "[YangRtcWrapper] createAnswer got non-ascii sdp, badIndex=" << badIndex
                 << " badCode=" << badCode
                 << " len=" << sdp.size();
        emit sigError("Created answer SDP contains invalid characters");
        return {};
    }
    qDebug() << "[YangRtcWrapper] createAnswer success, len=" << sdp.size();
    return sdp;
}

void YangRtcWrapper::sendVideoI420(uint8_t* data, int len, int64_t timestamp)
{
    if (!data || len <= 0) {
        return;
    }

    // 本地预览：直接把 I420 帧通过本地回调抛给 UI（如果上层需要）
    if (m_localVideoCallback) {
        QByteArray frameCopy(reinterpret_cast<const char*>(data), len);
        m_localVideoCallback(frameCopy, m_avInfo.video.width, m_avInfo.video.height,
                             (m_avInfo.video.videoEncoderFormat == YangI420) ? 1 : 0);
    }

    // 通过 metartc 发送到远端
    if (m_peerConnection.on_video) {
        YangFrame frame;
        memset(&frame, 0, sizeof(YangFrame));
        frame.payload = data;
        frame.nb = len;
        frame.pts = timestamp;
        frame.frametype = YangFrameTypeVideo; // 简单标记为视频帧

        int32_t ret = m_peerConnection.on_video(&m_peerConnection.peer, &frame);
        if (ret != Yang_Ok) {
            qDebug() << "[YangRtcWrapper] sendVideoI420 on_video failed, ret=" << ret;
        }
    }
}

bool YangRtcWrapper::setLocalDescription(const QString &sdp, const QString &type)
{
    Q_UNUSED(type);
    if (m_state != YangRtcState::Connecting) {
        emit sigError("RTC connection not in connecting state");
        return false;
    }
    if (sdp.isEmpty() || !m_peerConnection.setLocalDescription) {
        emit sigError("setLocalDescription not available or SDP empty");
        return false;
    }

    QString fixed = yang_normalizeSdp(sdp);
    m_lastLocalSdp = fixed.toLatin1();
    if (m_lastLocalSdp.isEmpty() || m_lastLocalSdp.at(m_lastLocalSdp.size() - 1) != '\0') {
        m_lastLocalSdp.append('\0');
    }
    char *sdpStr = m_lastLocalSdp.data();
    int32_t ret = m_peerConnection.setLocalDescription(&m_peerConnection.peer, sdpStr);
    if (ret != Yang_Ok) {
        qDebug() << "[YangRtcWrapper] setLocalDescription failed, ret=" << ret;
        emit sigError("Failed to set local description");
        return false;
    }
    qDebug() << "[YangRtcWrapper] setLocalDescription success";
    return true;
}

bool YangRtcWrapper::setRemoteDescription(const QString &sdp, const QString &type)
{
    Q_UNUSED(type);
    if (m_state != YangRtcState::Connecting) {
        emit sigError("RTC connection not in connecting state");
        return false;
    }
    if (sdp.isEmpty() || !m_peerConnection.setRemoteDescription) {
        emit sigError("setRemoteDescription not available or SDP empty");
        return false;
    }

    QString fixed = yang_normalizeSdp(sdp);
    const bool hadIceLite = fixed.contains("a=ice-lite");
    int badIndex = -1;
    ushort badCode = 0;
    if (!yang_isAsciiSdpSafe(fixed, &badIndex, &badCode)) {
        qDebug() << "[YangRtcWrapper] setRemoteDescription rejected non-ascii sdp, badIndex=" << badIndex
                 << " badCode=" << badCode
                 << " len=" << fixed.size();
        emit sigError("Remote SDP contains invalid characters");
        return false;
    }

    const int sdpLen = fixed.size();
    const QString firstLine = fixed.section("\r\n", 0, 0);
    qDebug() << "[YangRtcWrapper] setRemoteDescription input: len=" << sdpLen
             << " firstLine=" << firstLine
             << " iceLitePresentInitially=" << hadIceLite;

    m_lastRemoteSdp = fixed.toLatin1();
    if (m_lastRemoteSdp.isEmpty() || m_lastRemoteSdp.at(m_lastRemoteSdp.size() - 1) != '\0') {
        m_lastRemoteSdp.append('\0');
    }
    char *sdpStr = m_lastRemoteSdp.data();
    int32_t ret = m_peerConnection.setRemoteDescription(&m_peerConnection.peer, sdpStr);
    qDebug() << "[YangRtcWrapper] ret =" << ret;
    if (ret != Yang_Ok) {
        const int previewLen = qMin(256, m_lastRemoteSdp.size());
        QByteArray preview = m_lastRemoteSdp.left(previewLen);
        qDebug() << "[YangRtcWrapper] setRemoteDescription failed, ret=" << ret
                 << " len=" << sdpLen
                 << " iceCandidateType=" << m_avInfo.rtc.iceCandidateType
                 << " sdpPreview=" << QString::fromUtf8(preview);
        emit sigError("Failed to set remote description");
        return false;
    }

    qDebug() << "[YangRtcWrapper] setRemoteDescription success";
    m_connectTimer->start(500);
    return true;
}

bool YangRtcWrapper::addIceCandidate(const QString &candidate, int sdpMLineIndex, const QString &sdpMid)
{
    Q_UNUSED(sdpMLineIndex);
    Q_UNUSED(sdpMid);
    // 当前使用的 YangPeerConnection C API 不提供 addIceCandidate 接口，
    // ICE/DTLS 由 metartc 内部和 TURN 服务器处理，这里的 candidate 对底层无实际作用。
    qDebug() << "[YangRtcWrapper] addIceCandidate (ignored for current metartc API), len="
             << candidate.size();
    return true;
}

bool YangRtcWrapper::connectToServer()
{
    return true;
}

void YangRtcWrapper::closeConnection()
{
    // 关闭时重置重试计数，避免下次连接继承旧值
    this->setProperty("_yang_rtc_retryCount", 0);

    if (m_peerConnection.close) {
        m_peerConnection.close(&m_peerConnection.peer);
    }
    if (m_connectTimer) {
        m_connectTimer->stop();
    }
    if (m_state != YangRtcState::Idle) {
        setState(YangRtcState::Disconnected);
    }
}

void YangRtcWrapper::setState(YangRtcState state)
{
    if (m_state != state) {
        m_state = state;
        emit sigConnectionStateChanged(m_state);
    }
}

void YangRtcWrapper::setDirection(YangRtcDirection dir)
{
    m_peerConnection.peer.streamconfig.direction = dir;
}

void YangRtcWrapper::setControlled(bool controlled)
{
    m_peerConnection.peer.streamconfig.isControlled = controlled ? yangtrue : yangfalse;
}

// === 静态回调 ===
int32_t YangRtcWrapper::onAudioFrame(YangPeer* peer, YangFrame *audioFrame)
{
    Q_UNUSED(peer);
    Q_UNUSED(audioFrame);
    return Yang_Ok;
}

int32_t YangRtcWrapper::onVideoFrame(YangPeer* peer, YangFrame *videoFrame)
{
    Q_UNUSED(peer);
    Q_UNUSED(videoFrame);
    return Yang_Ok;
}

int32_t YangRtcWrapper::onMessageFrame(YangPeer* peer, YangFrame *msgFrame)
{
    Q_UNUSED(peer);
    Q_UNUSED(msgFrame);
    return Yang_Ok;
}

// === 接收回调 ===
void YangRtcWrapper::onRecvAudioFrame(void* context, YangFrame *audioFrame)
{
    if (!context || !audioFrame || !audioFrame->payload || audioFrame->nb <= 0) return;

    auto *wrapper = static_cast<YangRtcWrapper*>(context);
    if (!wrapper) return;

    // 将远端音频帧传给上层，由上层决定如何播放
    // 当前项目暂未集成本地音频播放，可以在这里添加信号/回调
    Q_UNUSED(wrapper);
}

void YangRtcWrapper::onRecvVideoFrame(void* context, YangFrame *videoFrame)
{
    qDebug() << "[YangRtcWrapper] onRecvVideoFrame, nb=" << (videoFrame ? videoFrame->nb : 0);
    if (!context || !videoFrame || !videoFrame->payload || videoFrame->nb <= 0) return;

    auto *wrapper = static_cast<YangRtcWrapper*>(context);
    if (!wrapper) return;

    int width  = wrapper->m_avInfo.video.width;
    int height = wrapper->m_avInfo.video.height;

    int format = (wrapper->m_avInfo.video.videoDecoderFormat == YangI420) ? 1 : 0;

    // 直接把 YUV420 帧数据复制到 QByteArray 中，由上层在 QWidget 中用 OpenGL 显示
    QByteArray frameCopy(reinterpret_cast<const char*>(videoFrame->payload),
                         videoFrame->nb);

    if (wrapper->m_remoteVideoCallback) {
        wrapper->m_remoteVideoCallback(frameCopy, width, height, format);
    }
}

void YangRtcWrapper::onRecvMessageFrame(void* context, YangFrame *msgFrame)
{
    Q_UNUSED(context);
    Q_UNUSED(msgFrame);
}
