#include "YangRtcWrapper.h"
#include <QDebug>
#include <QTimer>
#include <QThread>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

QHash<void*, YangRtcWrapper*> YangRtcWrapper::s_wrapperMap;

YangRtcWrapper::YangRtcWrapper(QObject *parent)
    : QObject(parent)
    , m_state(YangRtcState::Idle)
    , m_localVideoCallback(nullptr)
    , m_remoteVideoCallback(nullptr)
    , m_connectTimer(new QTimer(this))
    , m_ctx(nullptr)
    , m_pushMsgHandle(nullptr)
    , m_pushHandle(nullptr)
    , m_rtcPublish(nullptr)
    , m_localStreamNotified(false)
    , m_remoteStreamNotified(false)
{
    // 初始化 AV 信息
    yang_init_avinfo(&m_avInfo);

    // 使用 metartc 默认配置初始化 context
    m_ctx = new YangContext();
    m_ctx->avinfo = m_avInfo;

    memset(&m_peerConnection, 0, sizeof(YangPeerConnection));
    yang_create_peerConnection(&m_peerConnection);

    memset(&m_peer, 0, sizeof(YangPeer));
    m_peer.conn = nullptr;
    m_peer.avinfo = &m_avInfo;
    memset(&m_peer.streamconfig, 0, sizeof(YangStreamConfig));

    m_peer.streamconfig.direction = YangSendrecv;

    m_peer.streamconfig.recvCallback.context = this;
    m_peer.streamconfig.recvCallback.receiveAudio = YangRtcWrapper::onRecvAudioFrame;
    m_peer.streamconfig.recvCallback.receiveVideo = YangRtcWrapper::onRecvVideoFrame;
    m_peer.streamconfig.recvCallback.receiveMsg   = YangRtcWrapper::onRecvMessageFrame;

    if (m_peerConnection.init) {
        m_peerConnection.init(&m_peer);
    }

    m_peerConnection.on_audio   = YangRtcWrapper::onAudioFrame;
    m_peerConnection.on_video   = YangRtcWrapper::onVideoFrame;
    m_peerConnection.on_message = YangRtcWrapper::onMessageFrame;

    s_wrapperMap.insert(&m_peer, this);

    m_connectTimer->setSingleShot(true);
    connect(m_connectTimer, &QTimer::timeout, this, [this]() {
        if (m_state == YangRtcState::Connecting && m_peerConnection.isConnected) {
            if (m_peerConnection.isConnected(&m_peer)) {
                setState(YangRtcState::Connected);
            }
        }
    });
}

YangRtcWrapper::~YangRtcWrapper()
{
    closeConnection();
    yang_destroy_peerConnection(&m_peerConnection);
    s_wrapperMap.remove(&m_peer);

    stopCaptureAndPublish();
    delete m_ctx;
    m_ctx = nullptr;
}

bool YangRtcWrapper::configureIceServers(const QJsonArray& iceServers)
{
    if (iceServers.isEmpty()) {
        qDebug() << "[YangRtcWrapper] No ICE servers provided";
        return true;
    }

    QJsonObject iceServerObj = iceServers[0].toObject();
    QString urls = iceServerObj["urls"].toString();
    QString username = iceServerObj["username"].toString();
    QString credential = iceServerObj["credential"].toString();

    if (urls.startsWith("turn:")) {
        QString serverUrl = urls.mid(5);
        QStringList parts = serverUrl.split(":");
        if (parts.size() >= 2) {
            QString ip = parts[0];
            QString portStr = parts[1].split("?")[0];
            int port = portStr.toInt();

            strncpy(m_avInfo.rtc.iceServerIP, ip.toUtf8().constData(), sizeof(m_avInfo.rtc.iceServerIP) - 1);
            m_avInfo.rtc.iceServerPort = port;
            strncpy(m_avInfo.rtc.iceUserName, username.toUtf8().constData(), sizeof(m_avInfo.rtc.iceUserName) - 1);
            strncpy(m_avInfo.rtc.icePassword, credential.toUtf8().constData(), sizeof(m_avInfo.rtc.icePassword) - 1);

            qDebug() << "[YangRtcWrapper] Configured TURN server:" << ip << ":" << port;
            return true;
        }
    }

    return false;
}

bool YangRtcWrapper::initRtcConnection(const QString &serverUrl, int localPort)
{
    if (m_state != YangRtcState::Idle && m_state != YangRtcState::Disconnected && m_state != YangRtcState::Failed) {
        qDebug() << "[YangRtcWrapper] initRtcConnection called while in state" << static_cast<int>(m_state);
        closeConnection();
    }

    memset(&m_peer, 0, sizeof(YangPeer));
    m_peer.conn = nullptr;
    m_peer.avinfo = &m_avInfo;
    memset(&m_peer.streamconfig, 0, sizeof(YangStreamConfig));
    m_peer.streamconfig.direction = YangSendrecv;
    m_peer.streamconfig.recvCallback.context = this;
    m_peer.streamconfig.recvCallback.receiveAudio = YangRtcWrapper::onRecvAudioFrame;
    m_peer.streamconfig.recvCallback.receiveVideo = YangRtcWrapper::onRecvVideoFrame;
    m_peer.streamconfig.recvCallback.receiveMsg   = YangRtcWrapper::onRecvMessageFrame;

    if (m_peerConnection.init) {
        m_peerConnection.init(&m_peer);
    }

    m_serverUrl = serverUrl;
    m_localStreamNotified = false;
    m_remoteStreamNotified = false;

    if (localPort > 0) {
        m_avInfo.rtc.rtcLocalPort = localPort;
    }
    if (!serverUrl.isEmpty()) {
        QByteArray ipBytes = serverUrl.toLocal8Bit();
        qstrncpy(m_avInfo.rtc.rtcServerIP, ipBytes.constData(), sizeof(m_avInfo.rtc.rtcServerIP));
    }

    m_avInfo.video.width  = 640;
    m_avInfo.video.height = 480;
    m_avInfo.video.frame  = 25;
    m_avInfo.video.rate   = 800;
    m_avInfo.video.videoCaptureFormat = YangI420;
    m_avInfo.video.videoEncoderFormat = YangI420;
    m_avInfo.video.videoDecoderFormat = YangI420;

    m_avInfo.audio.sample  = 48000;
    m_avInfo.audio.channel = 2;
    m_avInfo.audio.bitrate = 64000;

    // 同步到 context
    if (m_ctx) {
        m_ctx->avinfo = m_avInfo;
    }

    setState(YangRtcState::Connecting);

    // 启动推流/采集管线
    initCaptureAndPublish();

    return true;
}

bool YangRtcWrapper::addAudioTrack()
{
    if (!m_peerConnection.addAudioTrack) return false;
    int32_t ret = m_peerConnection.addAudioTrack(&m_peer, Yang_AED_OPUS);
    return ret == Yang_Ok;
}

bool YangRtcWrapper::addVideoTrack()
{
    if (!m_peerConnection.addVideoTrack) return false;
    int32_t ret = m_peerConnection.addVideoTrack(&m_peer, Yang_VED_H264);
    return ret == Yang_Ok;
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
    int32_t ret = m_peerConnection.createOffer(&m_peer, &sdp);
    if (ret != Yang_Ok || !sdp) {
        emit sigError("Failed to create offer");
        return {};
    }
    return QString::fromUtf8(sdp);
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

    char answer[4096] = {0};
    int32_t ret = m_peerConnection.createAnswer(&m_peer, answer);
    if (ret != Yang_Ok) {
        emit sigError("Failed to create answer");
        return {};
    }
    return QString::fromUtf8(answer);
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

    QByteArray sdpBytes = sdp.toUtf8();
    char *sdpStr = sdpBytes.data();
    int32_t ret = m_peerConnection.setLocalDescription(&m_peer, sdpStr);
    if (ret != Yang_Ok) {
        emit sigError("Failed to set local description");
        return false;
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

    QByteArray sdpBytes = sdp.toUtf8();
    char *sdpStr = sdpBytes.data();
    int32_t ret = m_peerConnection.setRemoteDescription(&m_peer, sdpStr);
    if (ret != Yang_Ok) {
        emit sigError("Failed to set remote description");
        return false;
    }

    m_connectTimer->start(1000);
    return true;
}

bool YangRtcWrapper::addIceCandidate(const QString &candidate, int sdpMLineIndex, const QString &sdpMid)
{
    Q_UNUSED(candidate);
    Q_UNUSED(sdpMLineIndex);
    Q_UNUSED(sdpMid);
    return true;
}

bool YangRtcWrapper::connectToServer()
{
    return true;
}

void YangRtcWrapper::closeConnection()
{
    if (m_peerConnection.close) {
        m_peerConnection.close(&m_peer);
    }
    if (m_connectTimer) {
        m_connectTimer->stop();
    }
    if (m_state != YangRtcState::Idle) {
        setState(YangRtcState::Disconnected);
    }

    stopCaptureAndPublish();
}

void YangRtcWrapper::setState(YangRtcState state)
{
    if (m_state != state) {
        m_state = state;
        emit sigConnectionStateChanged(m_state);
    }
}

int32_t YangRtcWrapper::onAudioFrame(YangPeer* peer, YangFrame *audioFrame)
{
    Q_UNUSED(peer);
    Q_UNUSED(audioFrame);
    return Yang_Ok;
}

int32_t YangRtcWrapper::onVideoFrame(YangPeer* peer, YangFrame *videoFrame)
{
    if (!peer || !videoFrame || !videoFrame->payload || videoFrame->nb <= 0) {
        return Yang_Ok;
    }

    YangRtcWrapper* wrapper = s_wrapperMap.value(peer, nullptr);
    if (!wrapper) {
        return Yang_Ok;
    }

    const int width = wrapper->m_avInfo.video.width;
    const int height = wrapper->m_avInfo.video.height;
    const int format = (wrapper->m_avInfo.video.videoEncoderFormat == YangI420) ? 1 : 0;
    const QByteArray frameCopy(reinterpret_cast<const char*>(videoFrame->payload), videoFrame->nb);

    if (wrapper->m_localVideoCallback) {
        QMetaObject::invokeMethod(wrapper, [wrapper, frameCopy, width, height, format]() {
            if (wrapper->m_localVideoCallback) {
                wrapper->m_localVideoCallback(frameCopy, width, height, format);
            }
        }, Qt::QueuedConnection);
    }
    if (!wrapper->m_localStreamNotified) {
        wrapper->m_localStreamNotified = true;
        QMetaObject::invokeMethod(wrapper, [wrapper]() {
            emit wrapper->sigLocalStreamReady();
        }, Qt::QueuedConnection);
    }

    return Yang_Ok;
}

int32_t YangRtcWrapper::onMessageFrame(YangPeer* peer, YangFrame *msgFrame)
{
    Q_UNUSED(peer);
    Q_UNUSED(msgFrame);
    return Yang_Ok;
}

void YangRtcWrapper::onRecvAudioFrame(void* context, YangFrame *audioFrame)
{
    Q_UNUSED(context);
    Q_UNUSED(audioFrame);
}

void YangRtcWrapper::onRecvVideoFrame(void* context, YangFrame *videoFrame)
{
    if (!context || !videoFrame || !videoFrame->payload || videoFrame->nb <= 0) return;

    auto *wrapper = static_cast<YangRtcWrapper*>(context);
    if (!wrapper) {
        qDebug() << "[YangRtcWrapper] onRecvVideoFrame: wrapper null";
        return;
    }

    int width  = wrapper->m_avInfo.video.width;
    int height = wrapper->m_avInfo.video.height;

    int format = (wrapper->m_avInfo.video.videoDecoderFormat == YangI420) ? 1 : 0;

    QByteArray frameCopy(reinterpret_cast<const char*>(videoFrame->payload),
                         videoFrame->nb);

    if (wrapper->m_remoteVideoCallback) {
        QMetaObject::invokeMethod(wrapper, [wrapper, frameCopy, width, height, format]() {
            if (wrapper->m_remoteVideoCallback) {
                wrapper->m_remoteVideoCallback(frameCopy, width, height, format);
            }
        }, Qt::QueuedConnection);
    }
    if (!wrapper->m_remoteStreamNotified) {
        wrapper->m_remoteStreamNotified = true;
        QMetaObject::invokeMethod(wrapper, [wrapper]() {
            emit wrapper->sigRemoteStreamReady();
        }, Qt::QueuedConnection);
    }
}

void YangRtcWrapper::onRecvMessageFrame(void* context, YangFrame *msgFrame)
{
    Q_UNUSED(context);
    Q_UNUSED(msgFrame);
}

void YangRtcWrapper::initCaptureAndPublish()
{
    if (!m_ctx) return;

    if (!m_pushMsgHandle) {
        m_pushMsgHandle = new YangPushMessageHandle(true, Yang_VideoSrc_Camera,
                                                    &m_ctx->avinfo.video, &m_ctx->avinfo.video,
                                                    m_ctx, nullptr, nullptr);
    }

    if (!m_pushHandle) {
        m_pushHandle = m_pushMsgHandle->m_push;
    }

    if (!m_rtcPublish) {
        m_rtcPublish = new YangRtcPublish(m_ctx);
        if (m_rtcPublish && !m_rtcPublish->m_isStart) {
            m_rtcPublish->start();
            m_rtcPublish->m_isStart = 1;
        }
    }

    // 使用现有 push demo 的发布逻辑：这里简化为本地发布到当前 PeerConnection
    // 在你的环境中，如需 SFU/WHIP，请在此调用 publish() / init(url)
}

void YangRtcWrapper::stopCaptureAndPublish()
{
    if (m_rtcPublish) {
        m_rtcPublish->stop();
        m_rtcPublish->disConnectMediaServer();
        delete m_rtcPublish;
        m_rtcPublish = nullptr;
    }

    if (m_pushMsgHandle) {
        delete m_pushMsgHandle;
        m_pushMsgHandle = nullptr;
        m_pushHandle = nullptr;
    }
}
