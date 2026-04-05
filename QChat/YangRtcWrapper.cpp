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

    // ===================================================================
    // 正确的 YangPeerConnection 初始化顺序（参考 YangPeerConnection2
    // 构造函数和 YangRtcReceive::init 的 demo 实现）：
    //
    // 1. memset 清零结构体
    // 2. 设置 peer.avinfo 和 peer.streamconfig
    // 3. 配置 ICE 服务器（configureIceServers）—— 更新 avinfo.rtc 的 ICE 参数
    // 4. 调用 initPeerConnection() —— 内部调 yang_create_peerConnection，
    //    此时 avinfo.rtc.iceCandidateType 已正确设置（Turn/Stun/Host），
    //    yang_create_ice 会正确复制该值
    // 5. 之后不要再整体 memset，也不要手动调 init(peer)
    // ===================================================================

    // 步骤1：清零
    memset(&m_peerConnection, 0, sizeof(YangPeerConnection));

    // 步骤2：设置 avinfo 和 streamconfig（必须在 yang_create_peerConnection 之前！）
    YangPeer* peer = &m_peerConnection.peer;
    peer->avinfo = &m_avInfo;

    peer->streamconfig.direction   = YangSendrecv;
    peer->streamconfig.isControlled = yangfalse;

    // 接收远端音视频帧的回调
    peer->streamconfig.recvCallback.context      = this;
    peer->streamconfig.recvCallback.receiveAudio = YangRtcWrapper::onRecvAudioFrame;
    peer->streamconfig.recvCallback.receiveVideo = YangRtcWrapper::onRecvVideoFrame;
    peer->streamconfig.recvCallback.receiveMsg   = YangRtcWrapper::onRecvMessageFrame;

    // ICE/连接状态回调
    peer->streamconfig.iceCallback.context                  = this;
    peer->streamconfig.iceCallback.onConnectionStateChange  = nullptr;
    peer->streamconfig.iceCallback.onIceStateChange         = nullptr;

    // 注意：yang_create_peerConnection 延迟到 initPeerConnection() 中调用！
    // 必须在 configureIceServers() 之后调用，否则 ICE session 的 candidateType
    // 会一直是初始值 YangIceHost（因为 yang_create_ice 是复制而非引用 avinfo）。

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
    // 只有在 yang_create_peerConnection 被调用过（peer.conn 非 NULL）时才销毁
    if (m_peerConnection.peer.conn) {
        yang_destroy_peerConnection(&m_peerConnection);
        s_wrapperMap.remove(&m_peerConnection.peer);
    }
}

bool YangRtcWrapper::initPeerConnection()
{
    if (m_state != YangRtcState::Idle) {
        qDebug() << "[YangRtcWrapper] initPeerConnection called while not Idle, state=" << static_cast<int>(m_state);
        return false;
    }

    // 检查 avinfo 是否已绑定
    if (m_peerConnection.peer.avinfo == nullptr) {
        qDebug() << "[YangRtcWrapper] initPeerConnection: avinfo is NULL, cannot create peer connection";
        return false;
    }

    // 调用 yang_create_peerConnection —— 内部自动调 yang_pc_init，
    // 读取 peer->avinfo（包括已更新的 iceCandidateType）和 peer->streamconfig
    yang_create_peerConnection(&m_peerConnection);

    // 将当前实例注册到静态映射
    s_wrapperMap.insert(&m_peerConnection.peer, this);

    return true;
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

    if (localPort <= 0) {
        localPort = 20000 + QRandomGenerator::global()->bounded(20000);
    }

    m_avInfo.rtc.rtcLocalPort = localPort;
    m_peerConnection.peer.streamconfig.localPort = localPort;

    // P2P Host 模式：remoteIp/remotePort 不在此处填充。
    // setRemoteDescription 解析对端 SDP 的 candidate 行时会自动覆盖写入 remoteIp/remotePort，
    // 所以这里预填 TURN/STUN 服务器地址只对非 Host 模式有意义，Host 模式直接跳过。
    if (m_avInfo.rtc.iceCandidateType != YangIceHost &&
        m_avInfo.rtc.iceServerIP[0] != '\0' && m_avInfo.rtc.iceServerPort > 0) {
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

    // 注意：不要再调用 m_peerConnection.init(peer)！
    // yang_create_peerConnection 内部已经自动调用了 yang_pc_init，
    // 而 yang_pc_init 有 conn!=NULL 保护，重复调用会被跳过。
    // 正确的做法是在构造函数中先设好 avinfo/streamconfig 再调 yang_create_peerConnection。

    setState(YangRtcState::Connecting);
    return true;
}

bool YangRtcWrapper::addAudioTrack()
{
    if (!m_peerConnection.addAudioTrack) {
        qDebug() << "[YangRtcWrapper] addAudioTrack: function pointer is NULL!";
        return false;
    }
    int32_t ret = m_peerConnection.addAudioTrack(&m_peerConnection.peer, Yang_AED_OPUS);
    if (ret != Yang_Ok) {
        qDebug() << "[YangRtcWrapper] addAudioTrack failed, ret=" << ret;
        return false;
    }
    return true;
}

bool YangRtcWrapper::addVideoTrack()
{
    if (!m_peerConnection.addVideoTrack) {
        qDebug() << "[YangRtcWrapper] addVideoTrack: function pointer is NULL!";
        return false;
    }
    int32_t ret = m_peerConnection.addVideoTrack(&m_peerConnection.peer, Yang_VED_H264);
    if (ret != Yang_Ok) {
        qDebug() << "[YangRtcWrapper] addVideoTrack failed, ret=" << ret;
        return false;
    }
    return true;
}

bool YangRtcWrapper::addTransceiver(YangRtcDirection direction)
{
    if (!m_peerConnection.addTransceiver) {
        qDebug() << "[YangRtcWrapper] addTransceiver: function pointer is NULL!";
        return false;
    }
    int32_t ret = m_peerConnection.addTransceiver(&m_peerConnection.peer, direction);
    if (ret != Yang_Ok) {
        qDebug() << "[YangRtcWrapper] addTransceiver failed, ret=" << ret;
        return false;
    }
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

    // 通过 metartc 内部发送管道将 I420 帧送入编码器，再打包 RTP 发送到远端。
    // on_video 是库在 yang_create_peerConnection 时设置的原生发送入口（已不再被覆盖）。
    if (m_peerConnection.on_video) {
        YangFrame frame;
        memset(&frame, 0, sizeof(YangFrame));
        frame.payload = data;
        frame.nb = len;
        frame.pts = timestamp;
        frame.mediaType = YangFrameTypeVideo;
        // frametype: 0=P帧, 1=I帧, 9=SPS/PPS；此处传原始 I420，由库内部编码器决定帧类型
        frame.frametype = YANG_Frametype_P;

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

    // P2P 模式：SDP 双方交换完成后 metartc 内部自动触发 ICE/DTLS，
    // 无需调用 connectSfuServer（它在 P2P 模式下是 NULL）。
    // Answerer 路径：setLocalDescription(answer) 完成，双端 SDP 都就位，
    // 开始轮询 isConnected 等待 ICE 握手完成。
    if (!m_lastRemoteSdp.isEmpty()) {
        qDebug() << "[YangRtcWrapper] Both SDPs set (answerer path), starting connection poll";
        m_connectTimer->start(500);
    }

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

    // P2P 模式：metartc 在 setRemoteDescription 解析到对端 ICE 候选信息后
    // 会自动开始 ICE/DTLS 握手，外部无需调用 connectSfuServer（P2P 下为 NULL）。
    //
    // Offerer  路径：setRemoteDescription(answer) → 双端 SDP 都已就位，开始轮询
    // Answerer 路径：setRemoteDescription(offer)  → 还需等 setLocalDescription(answer)，
    //               轮询会在 setLocalDescription 成功后启动
    if (!m_lastLocalSdp.isEmpty()) {
        qDebug() << "[YangRtcWrapper] Both SDPs set (offerer path), starting connection poll";
        m_connectTimer->start(500);
    } else {
        qDebug() << "[YangRtcWrapper] setRemoteDescription done (answerer path), poll will start after setLocalDescription";
    }

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
    // P2P 模式（Yang_Server_P2p）说明：
    //
    // connectSfuServer 仅供 SRS / ZLM 服务端模式使用（注释 //srs zlm），
    // 在 P2P 模式下 yang_create_peerConnection 不会填充该函数指针（NULL）。
    //
    // P2P 模式下，ICE/DTLS 握手由 metartc 内部在 setRemoteDescription 解析到
    // 对端 ICE 候选地址（remoteIp/remotePort）之后自动启动，外部无需显式调用
    // 任何 connect 函数。
    //
    // 连接状态通过 isConnected(peer) 轮询感知（connectTimer 每 500ms 轮询一次）。
    qDebug() << "[YangRtcWrapper] connectToServer called (P2P mode: ICE starts automatically after SDP exchange)";
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
