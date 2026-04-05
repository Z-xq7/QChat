#include "videocallmanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QTimer>
#include <QPainter>
#include <QImage>
#include <QDateTime>
#include "tcpmgr.h"
#include "usermgr.h"
#include "YangRtcWrapper.h"

VideoCallManager::VideoCallManager()
    : _state(VideoCallState::Idle)
    , _current_call_id("")
    , _current_peer_uid(0)
    , _isCaller(false)
    , _room_id("")
    , _turn_ws_url("")
    , _signaling_socket(nullptr)
    , _rtcWrapper(nullptr)
    , _camera(nullptr)
    , _captureSession(nullptr)
    , _videoSink(nullptr)
    , _incomingCallDialog(nullptr)
{
    // 延迟初始化信号连接，避免初始化时的循环依赖
    initializeConnections();
}

VideoCallManager::~VideoCallManager()
{
    stopLocalVideo();
    if (_signaling_socket) {
        _signaling_socket->close();
        delete _signaling_socket;
        _signaling_socket = nullptr;
    }

    if (_rtcWrapper) {
        delete _rtcWrapper;
        _rtcWrapper = nullptr;
    }
}

void VideoCallManager::startLocalVideo()
{
    if (_camera) return;
    _camera = new QCamera(this);
    _captureSession = new QMediaCaptureSession(this);
    _videoSink = new QVideoSink(this);
    _captureSession->setCamera(_camera);
    _captureSession->setVideoSink(_videoSink);
    connect(_videoSink, &QVideoSink::videoFrameChanged, this, &VideoCallManager::onLocalVideoFrameCaptured);
    _camera->start();
}

void VideoCallManager::stopLocalVideo()
{
    if (_camera) {
        _camera->stop();
    }
    if (_videoSink) {
        disconnect(_videoSink, nullptr, this, nullptr);
    }
    delete _videoSink;
    delete _captureSession;
    delete _camera;
    _videoSink = nullptr;
    _captureSession = nullptr;
    _camera = nullptr;
}

void VideoCallManager::onLocalVideoFrameCaptured(const QVideoFrame &frame)
{
    QImage image = frame.toImage();
    if (image.isNull()) return;

    if (_state != VideoCallState::Idle) {
        QImage previewImage = image.convertToFormat(QImage::Format_ARGB32);
        if (!previewImage.isNull()) {
            const int bytes = previewImage.bytesPerLine() * previewImage.height();
            QByteArray buffer(reinterpret_cast<const char *>(previewImage.constBits()), bytes);
            emit sigLocalPreviewFrame(buffer, previewImage.width(), previewImage.height(), 2);
        }
    }

    processVideoImage(image);
}

void VideoCallManager::processVideoFrame(const QVideoFrame &frame)
{
    processVideoImage(frame.toImage());
}

void VideoCallManager::processVideoImage(QImage image)
{
    if (!_rtcWrapper) return;
    YangRtcState state = _rtcWrapper->getState();
    if (state != YangRtcState::Connected) return;
    if (image.isNull()) return;
    image = image.convertToFormat(QImage::Format_RGB32);
    int targetWidth = 640;
    int targetHeight = 480;
    if (image.width() != targetWidth || image.height() != targetHeight) {
        image = image.scaled(targetWidth, targetHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    int ySize = targetWidth * targetHeight;
    int uvSize = ySize / 4;
    QByteArray buffer;
    buffer.resize(ySize + uvSize * 2);
    uint8_t *yPlane = reinterpret_cast<uint8_t *>(buffer.data());
    uint8_t *uPlane = yPlane + ySize;
    uint8_t *vPlane = uPlane + uvSize;
    const uchar *src = image.constBits();
    int srcStride = image.bytesPerLine();
    for (int y = 0; y < targetHeight; ++y) {
        const uint8_t *row = src + y * srcStride;
        for (int x = 0; x < targetWidth; ++x) {
            const uint8_t *pixel = row + x * 4;
            int b = pixel[0];
            int g = pixel[1];
            int r = pixel[2];
            int Y = static_cast<int>(0.257 * r + 0.504 * g + 0.098 * b + 16);
            if (Y < 0) Y = 0;
            if (Y > 255) Y = 255;
            yPlane[y * targetWidth + x] = static_cast<uint8_t>(Y);
        }
    }
    for (int y = 0; y < targetHeight; y += 2) {
        const uint8_t *row0 = src + y * srcStride;
        const uint8_t *row1 = src + (y + 1) * srcStride;
        for (int x = 0; x < targetWidth; x += 2) {
            const uint8_t *p0 = row0 + x * 4;
            const uint8_t *p1 = row0 + (x + 1) * 4;
            const uint8_t *p2 = row1 + x * 4;
            const uint8_t *p3 = row1 + (x + 1) * 4;
            int b0 = p0[0];
            int g0 = p0[1];
            int r0 = p0[2];
            int b1 = p1[0];
            int g1 = p1[1];
            int r1 = p1[2];
            int b2 = p2[0];
            int g2 = p2[1];
            int r2 = p2[2];
            int b3 = p3[0];
            int g3 = p3[1];
            int r3 = p3[2];
            int r = (r0 + r1 + r2 + r3) / 4;
            int g = (g0 + g1 + g2 + g3) / 4;
            int b = (b0 + b1 + b2 + b3) / 4;
            int U = static_cast<int>(-0.148 * r - 0.291 * g + 0.439 * b + 128);
            int V = static_cast<int>(0.439 * r - 0.368 * g - 0.071 * b + 128);
            if (U < 0) U = 0;
            if (U > 255) U = 255;
            if (V < 0) V = 0;
            if (V > 255) V = 255;
            int index = (y / 2) * (targetWidth / 2) + (x / 2);
            uPlane[index] = static_cast<uint8_t>(U);
            vPlane[index] = static_cast<uint8_t>(V);
        }
    }
    int64_t ts = QDateTime::currentMSecsSinceEpoch();
    _rtcWrapper->sendVideoI420(reinterpret_cast<uint8_t *>(buffer.data()), buffer.size(), ts);
}

void VideoCallManager::initializeConnections()
{
    // 连接TcpMgr的视频通话相关信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_call_incoming,
            this, &VideoCallManager::handleCallIncoming, Qt::UniqueConnection);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_accept_call,
            this, &VideoCallManager::handleAcceptCall, Qt::UniqueConnection);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_call_accepted,
            this, &VideoCallManager::handleCallAccept, Qt::UniqueConnection);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_call_rejected,
            this, &VideoCallManager::handleCallReject, Qt::UniqueConnection);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_call_hangup,
            this, &VideoCallManager::handleCallHangup, Qt::UniqueConnection);
}

void VideoCallManager::initMetartc()
{
    if (_rtcWrapper) return;

    _rtcWrapper = new YangRtcWrapper(this);

    connect(_rtcWrapper, &YangRtcWrapper::sigLocalStreamReady,
            this, &VideoCallManager::sigLocalStreamReady);
    connect(_rtcWrapper, &YangRtcWrapper::sigRemoteStreamReady,
            this, &VideoCallManager::sigRemoteStreamReady);
    connect(_rtcWrapper, &YangRtcWrapper::sigConnectionStateChanged,
            this, [this](YangRtcState newState) {
        switch (newState) {
        case YangRtcState::Connected:
            // ICE/DTLS 握手完成，进入通话状态
            // 注意：startLocalVideo() 已在 handleAcceptCall/handleCallAccept 里调用过，
            // 这里不再重复调用，避免摄像头二次初始化（第二次 new QCamera 会导致设备冲突崩溃）
            setState(VideoCallState::InCall);
            break;
        case YangRtcState::Disconnected:
        case YangRtcState::Failed:
            endCall();
            break;
        default:
            break;
        }
    });
    connect(_rtcWrapper, &YangRtcWrapper::sigError,
            this, &VideoCallManager::sigError);
}

QString VideoCallManager::createOffer()
{
    if (_rtcWrapper) return _rtcWrapper->createOffer();
    return QString();
}

bool VideoCallManager::setLocalDescription(const QString &sdp, const QString &type)
{
    return _rtcWrapper ? _rtcWrapper->setLocalDescription(sdp, type) : false;
}

bool VideoCallManager::setRemoteDescription(const QString &sdp, const QString &type)
{
    return _rtcWrapper ? _rtcWrapper->setRemoteDescription(sdp, type) : false;
}

bool VideoCallManager::addIceCandidate(const QString &candidate, int sdpMLineIndex, const QString &sdpMid)
{
    return _rtcWrapper ? _rtcWrapper->addIceCandidate(candidate, sdpMLineIndex, sdpMid) : false;
}

void VideoCallManager::initWebSocket()
{
    if (_signaling_socket) return;

    _signaling_socket = new QWebSocket();

    connect(_signaling_socket, &QWebSocket::connected, this, [this]() {
        qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                           << "]: Signaling server connected";

        QJsonObject joinMsg;
        joinMsg["type"] = "join";
        joinMsg["roomId"] = _room_id;
        joinMsg["uid"] = UserMgr::GetInstance()->GetUid();
        joinMsg["call_id"] = _current_call_id;
        joinMsg["role"] = _isCaller ? "caller" : "callee";
        sendSignalingMessage(joinMsg);
    });

    connect(_signaling_socket, &QWebSocket::disconnected, this, [this]() {
        qDebug() << "[VideoCallManager]: Signaling server disconnected";
        endCall();
    });

    connect(_signaling_socket, &QWebSocket::textMessageReceived, this, [this](const QString &message) {
        qDebug() << "[VideoCallManager]: Received signaling message:" << message;
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            const QString type = obj.value("type").toString();
            const QString sdp = obj.value("sdp").toString();
            const QString candidate = obj.value("candidate").toString();
            const int sdpMLineIndex = obj.value("sdpMLineIndex").toInt();
            const QString sdpMid = obj.value("sdpMid").toString();

            bool isInitiator = false;
            if (obj.contains("isInitiator")) {
                isInitiator = obj.value("isInitiator").toBool();
            } else if (type == "joined") {
                // Bug#6 修复：joined 消息中 isInitiator 由服务端在 ready 消息下发，
                // 此处的 peers 计数逻辑只是备用推断（先加入者 peers==0 时不是 initiator）。
                // 原代码两个分支赋值完全相同是死代码，此处修正逻辑以备万一服务端不发 isInitiator 时使用。
                int peers = obj.value("peers").toInt();
                isInitiator = (peers > 0); // 后加入者（房间里已有人）才是 initiator（发 offer）
            }

            handleSignalingMessage(type, sdp, candidate, sdpMLineIndex, sdpMid, isInitiator);
        }
    });

    connect(_signaling_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, [this](QAbstractSocket::SocketError error) {
        qDebug() << "[VideoCallManager]: WebSocket error:" << error;
        emit sigError("WebSocket error: " + QString::number(error));
        endCall();
    });
}

void VideoCallManager::initSignalingHandlers()
{
}

void VideoCallManager::startCall(int callee_uid)
{
    if (_state != VideoCallState::Idle) {
        emit sigError("对方正在通话中...");
        return;
    }

    QJsonObject obj;
    obj["callee_uid"] = callee_uid;
    obj["caller_uid"] = UserMgr::GetInstance()->GetUid();
    obj["caller_nick"] = UserMgr::GetInstance()->GetNick();

    QJsonDocument doc(obj);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    TcpMgr::GetInstance()->SendData(ReqId::ID_CALL_INVITE_REQ, data);

    setState(VideoCallState::Connecting);
    _current_peer_uid = callee_uid;
    _isCaller = true;
    startLocalVideo();
    qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                       << "]: Sent call invite request to user" << callee_uid;
}

void VideoCallManager::sendAcceptCallRequest(const QString &call_id, int caller_uid)
{
    if (_state != VideoCallState::Ringing) {
        emit sigError("没有待接听的通话...");
        return;
    }

    QJsonObject obj;
    obj["call_id"] = call_id;
    obj["caller_uid"] = caller_uid;

    QJsonDocument doc(obj);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    TcpMgr::GetInstance()->SendData(ReqId::ID_CALL_ACCEPT_REQ, data);

    qDebug() << "[VideoCallManager]: Sent CALL_ACCEPT_REQ to server, call_id:" << call_id;
}

void VideoCallManager::rejectCall(const QString &call_id, int caller_uid, const QString &reason)
{
    if (_state != VideoCallState::Ringing) {
        emit sigError("没有待拒绝的通话...");
        return;
    }

    QJsonObject obj;
    obj["call_id"] = call_id;
    obj["caller_uid"] = caller_uid;
    obj["reason"] = reason;

    QJsonDocument doc(obj);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    TcpMgr::GetInstance()->SendData(ReqId::ID_CALL_REJECT_REQ, data);

    if (_incomingCallDialog) {
        _incomingCallDialog->close();
        delete _incomingCallDialog;
        _incomingCallDialog = nullptr;
    }

    endCall();

    qDebug() << "[VideoCallManager]: Rejected call";
}

void VideoCallManager::hangupCall(const QString &call_id)
{
    if (_state == VideoCallState::Idle) return;

    const QString target_call_id = call_id.isEmpty() ? _current_call_id : call_id;

    QJsonObject obj;
    obj["call_id"] = target_call_id;

    QJsonDocument doc(obj);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    TcpMgr::GetInstance()->SendData(ReqId::ID_CALL_HANGUP_REQ, data);

    endCall();

    qDebug() << "[VideoCallManager]: Hangup call";
}

void VideoCallManager::endCall()
{
    if (_state == VideoCallState::Idle) return;

    stopLocalVideo();

    if (_signaling_socket && _signaling_socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject leaveMsg;
        leaveMsg["type"] = "leave";
        leaveMsg["roomId"] = _room_id;
        leaveMsg["uid"] = UserMgr::GetInstance()->GetUid();
        sendSignalingMessage(leaveMsg);

        _signaling_socket->close();
    }

    if (_incomingCallDialog) {
        _incomingCallDialog->close();
        delete _incomingCallDialog;
        _incomingCallDialog = nullptr;
    }

    if (_rtcWrapper) {
        _rtcWrapper->closeConnection();
    }

    setState(VideoCallState::Idle);
    _current_call_id.clear();
    _current_peer_uid = 0;
    _room_id.clear();
    _turn_ws_url.clear();
    _ice_servers = QJsonArray();

    qDebug() << "[VideoCallManager]: Call ended";
}

void VideoCallManager::sendSignalingMessage(const QJsonObject &message)
{
    if (_signaling_socket && _signaling_socket->state() == QAbstractSocket::ConnectedState) {
        QJsonDocument doc(message);
        const QString jsonStr = doc.toJson(QJsonDocument::Compact);
        _signaling_socket->sendTextMessage(jsonStr);
    } else {
        qDebug() << "[VideoCallManager]: Cannot send signaling message - not connected";
    }
}

void VideoCallManager::handleSignalingMessage(const QString &type,
                                              const QString &sdp,
                                              const QString &candidate,
                                              int sdpMLineIndex,
                                              const QString &sdpMid,
                                              bool isInitiator)
{
    if (type == "joined") {
        qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                           << "]: Joined room, waiting for ready";
        return;
    }

    if (type == "ready") {
        // 适配当前 server.js 的约定：后加入者 isInitiator=true。
        // 你这边希望先加入者(=isInitiator=false)负责发 Offer。
        const bool isOfferer = !isInitiator;

        qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                           << "]: Received ready, isInitiator=" << isInitiator
                           << " isOfferer=" << isOfferer
                           << " role=" << (_isCaller ? "caller" : "callee")
                           << " state=" << static_cast<int>(_state);

        if (_rtcWrapper) {
            _rtcWrapper->setDirection(YangSendrecv);
            // WebRTC 标准：Offerer = ICE controlling (isControlled=false)
            //               Answerer = ICE controlled  (isControlled=true)
            // 原来写反了！这里修正。
            _rtcWrapper->setControlled(isOfferer ? false : true);
        } else {
            qDebug() << "[VideoCallManager]: WARNING - _rtcWrapper is NULL when ready received!";
        }

        if (isOfferer) {
            qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                               << "]: Acting as offerer, calling createOffer...";
            fflush(stdout);
            QString offerSdp = createOffer();
            fflush(stdout);
            qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                               << "]: createOffer returned, len=" << offerSdp.size();
            if (offerSdp.isEmpty()) {
                emit sigError("生成 Offer SDP 失败");
                endCall();
                return;
            }

            qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                               << "]: Setting local description (offer)...";
            if (!setLocalDescription(offerSdp, "offer")) {
                emit sigError("设置本地 Offer SDP 失败");
                endCall();
                return;
            }

            qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                               << "]: Sending offer via signaling...";
            QJsonObject offerMsg;
            offerMsg["type"] = "offer";
            offerMsg["sdp"] = offerSdp;
            sendSignalingMessage(offerMsg);
            qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                               << "]: Offer sent successfully";
        } else {
            qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                               << "]: Ready as answerer, waiting for offer";
        }
        return;
    }

    if (type == "offer") {
        handleOfferInternal(sdp);
        return;
    }

    if (type == "answer") {
        handleAnswerInternal(sdp);
        return;
    }

    if (type == "ice") {
        handleIceCandidateInternal(candidate, sdpMLineIndex, sdpMid);
        return;
    }

    if (type == "peer-left" || type == "left") {
        qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                           << "]: Remote peer left room, ending call (type=" << type << ")";
        endCall();
        return;
    }

    if (type == "error") {
        qDebug() << "[VideoCallManager]: Signaling error from server";
        emit sigError("Signaling error: " + sdpMid);
        return;
    }

    qDebug() << "[VideoCallManager]: Unknown signaling message type:" << type;
}

static QString normalizeSdpForMetartc(QString sdp)
{
    // 某些信令链路会把 SDP 的换行变成两个字符“\n”，导致 metartc 解析失败。
    // 这里把字面量转义序列还原成真正换行，并规范为 CRLF。
    if (sdp.contains("\\n") || sdp.contains("\\r")) {
        sdp.replace("\\r\\n", "\n");
        sdp.replace("\\n", "\n");
        sdp.replace("\\r", "\n");
    }
    // 规范为 CRLF
    sdp.replace("\r\n", "\n");
    sdp.replace("\r", "\n");
    sdp.replace("\n", "\r\n");
    return sdp;
}

static bool looksLikeTruncatedSdp(const QString& sdp)
{
    // 必须包含至少这些字段
    if (!sdp.contains("v=0")) return true;
    if (!sdp.contains("o=")) return true;
    if (!sdp.contains("t=0 0")) return true;

    // 如果有 fingerprint 行，则 sha-256 指纹应为 32 字节 => 64 hex + 31 ':' = 95 字符（不含前缀）。
    // 这里做一个宽松校验：至少应包含 20 个 ':'，否则大概率被截断。
    const QRegularExpression reFp("a=fingerprint:sha-256\\s+([^\\r\\n]+)");
    QRegularExpressionMatch m = reFp.match(sdp);
    if (m.hasMatch()) {
        const QString fp = m.captured(1).trimmed();
        if (fp.count(':') < 20) {
            return true;
        }
    }

    return false;
}

void VideoCallManager::handleOfferInternal(const QString &sdp)
{
    qDebug() << "[VideoCallManager]: Received offer, handling with metartc";

    QString fixedSdp = normalizeSdpForMetartc(sdp);

    qDebug() << "[VideoCallManager]: offer sdp len=" << sdp.size()
             << " fixed len=" << fixedSdp.size()
             << " contains\\n=" << sdp.contains("\\n")
             << " contains\\\\n=" << sdp.contains("\\\\n");

    if (fixedSdp.isEmpty() || looksLikeTruncatedSdp(fixedSdp)) {
        emit sigError("对端 Offer SDP 异常/被截断（可能信令传输截断或转义处理错误）");
        endCall();
        return;
    }

    if (fixedSdp.isEmpty()) {
        emit sigError("对端 Offer SDP 为空");
        endCall();
        return;
    }
    if (!_rtcWrapper) {
        emit sigError("RTC 未初始化，无法处理 Offer");
        endCall();
        return;
    }

    YangRtcState state = _rtcWrapper->getState();
    if (state != YangRtcState::Connecting) {
        qDebug() << "[VideoCallManager]: Unexpected RTC state when handling offer:" << static_cast<int>(state);
        emit sigError("当前状态无法处理对端 Offer");
        endCall();
        return;
    }

    bool ok = setRemoteDescription(fixedSdp, "offer");
    if (!ok) {
        qDebug() << "[VideoCallManager]: setRemoteDescription for offer failed";
        emit sigError("解析对端 SDP 失败");
        endCall();
        return;
    }

    QString answerSdp = _rtcWrapper->createAnswer();
    if (answerSdp.isEmpty()) {
        qDebug() << "[VideoCallManager]: Failed to create answer SDP";
        emit sigError("生成 Answer SDP 失败");
        endCall();
        return;
    }

    // 关键：先 setLocalDescription(answer)，再发送 answer。
    bool localOk = setLocalDescription(answerSdp, "answer");
    if (!localOk) {
        qDebug() << "[VideoCallManager]: setLocalDescription for answer failed";
        emit sigError("设置本地 Answer SDP 失败");
        endCall();
        return;
    }

    QJsonObject answerMsg;
    answerMsg["type"] = "answer";
    answerMsg["sdp"] = answerSdp;
    sendSignalingMessage(answerMsg);
}

void VideoCallManager::handleAnswerInternal(const QString &sdp)
{
    qDebug() << "[VideoCallManager]: Received answer, handling with metartc";

    QString fixedSdp = normalizeSdpForMetartc(sdp);

    qDebug() << "[VideoCallManager]: answer sdp len=" << sdp.size()
             << " fixed len=" << fixedSdp.size()
             << " contains\\n=" << sdp.contains("\\n")
             << " contains\\\\n=" << sdp.contains("\\\\n");

    if (fixedSdp.isEmpty() || looksLikeTruncatedSdp(fixedSdp)) {
        emit sigError("对端 Answer SDP 异常/被截断（可能信令传输截断或转义处理错误）");
        endCall();
        return;
    }

    if (setRemoteDescription(fixedSdp, "answer")) {
        qDebug() << "[VideoCallManager]: Set remote answer description";
    }
}

void VideoCallManager::handleIceCandidateInternal(const QString &candidate, int sdpMLineIndex, const QString &sdpMid)
{
    qDebug() << "[VideoCallManager]: Received ICE candidate, handling with metartc";

    if (!candidate.isEmpty()) {
        addIceCandidate(candidate, sdpMLineIndex, sdpMid);
    }
}

void VideoCallManager::handleCallIncoming(int caller_uid, const QString &call_id, const QString &caller_name)
{
    setState(VideoCallState::Ringing);
    _current_call_id = call_id;
    _current_peer_uid = caller_uid;
    _isCaller = false;

    if (_incomingCallDialog) {
        delete _incomingCallDialog;
        _incomingCallDialog = nullptr;
    }

    std::shared_ptr<UserInfo> caller_info = UserMgr::GetInstance()->GetFriendById(caller_uid);
    const QString caller_icon = caller_info ? caller_info->_icon : QString();

    _incomingCallDialog = new IncomingCallDialog();
    _incomingCallDialog->setCallerInfo(caller_name, caller_icon, caller_uid);
    _incomingCallDialog->startTimeout(30);

    connect(_incomingCallDialog, &IncomingCallDialog::accepted, this, [this, call_id, caller_uid]() {
        qDebug() << "[VideoCallManager]: Call accepted via dialog";
        sendAcceptCallRequest(call_id, caller_uid);
    });

    connect(_incomingCallDialog, &IncomingCallDialog::rejected, this, [this, call_id, caller_uid]() {
        qDebug() << "[VideoCallManager]: Call rejected via dialog";
        rejectCall(call_id, caller_uid, "rejected");
    });

    _incomingCallDialog->show();

    emit sigIncomingCall(caller_uid, call_id, caller_name);
    qDebug() << "[VideoCallManager]: Incoming call from user" << caller_uid;
}

void VideoCallManager::handleAcceptCall(const QString &call_id,
                                        const QString &room_id,
                                        const QString &turn_ws_url,
                                        const QJsonArray &ice_servers)
{
    // 被叫方：服务器回包 CALL_ACCEPT_RSP
    qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                       << "]: handleAcceptCall (callee) called, call_id=" << call_id
                       << " room_id=" << room_id;
    if (_current_call_id == call_id && !_room_id.isEmpty() && _state == VideoCallState::Connecting) {
        qDebug() << "[VideoCallManager]: handleAcceptCall called twice, ignore";
        return;
    }

    _current_call_id = call_id;
    _room_id = room_id;
    _turn_ws_url = turn_ws_url;
    _ice_servers = ice_servers;

    // 立即切换状态（避免 initMetartc 内部的 Connected 回调误触发 endCall）
    setState(VideoCallState::Connecting);

    // 创建 RTC wrapper 和 WebSocket（先建对象，暂不连接）
    initMetartc();
    initWebSocket();

    qDebug() << "[VideoCallManager]: handleAcceptCall -> initMetartc done, _rtcWrapper =" << (void*)_rtcWrapper;

    if (_rtcWrapper) {
        // 先配置 ICE 服务器（coturn TURN relay），再 init 连接参数
        qDebug() << "[VideoCallManager]: Calling configureIceServers...";
        _rtcWrapper->configureIceServers(_ice_servers);
        qDebug() << "[VideoCallManager]: configureIceServers returned, calling initPeerConnection...";
        if (!_rtcWrapper->initPeerConnection()) {
            qDebug() << "[VideoCallManager]: initPeerConnection FAILED";
            emit sigError("初始化 PeerConnection 失败");
            endCall();
            return;
        }
        qDebug() << "[VideoCallManager]: initPeerConnection returned, calling initRtcConnection...";
        _rtcWrapper->initRtcConnection(_turn_ws_url);
        qDebug() << "[VideoCallManager]: initRtcConnection returned, calling addAudioTrack...";
        // addTrack 必须在 WebSocket 连接之前完成（SDP 协商需要 track 信息）
        if (!_rtcWrapper->addAudioTrack()) {
            qDebug() << "[VideoCallManager]: addAudioTrack FAILED";
            emit sigError("初始化音频轨道失败");
            endCall();
            return;
        }
        qDebug() << "[VideoCallManager]: addAudioTrack OK, calling addVideoTrack...";
        if (!_rtcWrapper->addVideoTrack()) {
            qDebug() << "[VideoCallManager]: addVideoTrack FAILED";
            emit sigError("初始化视频轨道失败");
            endCall();
            return;
        }
        qDebug() << "[VideoCallManager]: addVideoTrack OK";
        if (!_rtcWrapper->addTransceiver()) {
            qDebug() << "[VideoCallManager]: addTransceiver FAILED";
            emit sigError("初始化 transceiver 失败");
            endCall();
            return;
        }
        qDebug() << "[VideoCallManager]: addTransceiver OK";
    }

    // 开启本地摄像头预览（仅调用一次，不会在 Connected 回调里重复调用）
    qDebug() << "[VideoCallManager]: Calling startLocalVideo...";
    startLocalVideo();
    qDebug() << "[VideoCallManager]: startLocalVideo returned";

    // 所有初始化完毕后，才打开 WebSocket 连接信令服务器
    if (_signaling_socket && _signaling_socket->state() == QAbstractSocket::UnconnectedState && !_turn_ws_url.isEmpty()) {
        qDebug() << "[VideoCallManager]: Connecting to signaling server:" << _turn_ws_url;
        _signaling_socket->open(QUrl(_turn_ws_url));
    } else {
        qDebug() << "[VideoCallManager]: WebSocket skip - socket=" << (void*)_signaling_socket
                 << " state=" << (_signaling_socket ? _signaling_socket->state() : -1)
                 << " url=" << _turn_ws_url;
    }

    if (_incomingCallDialog) {
        _incomingCallDialog->close();
        delete _incomingCallDialog;
        _incomingCallDialog = nullptr;
    }

    emit sigCallAccepted(call_id, room_id, turn_ws_url, ice_servers);

    qDebug() << "[VideoCallManager]: Accepted call (callee), connecting to signaling server, room:" << room_id;
}

void VideoCallManager::handleCallAccept(const QString &call_id,
                                        const QString &room_id,
                                        const QString &turn_ws_url,
                                        const QJsonArray &ice_servers)
{
    // 主叫方：服务器通知 CALL_ACCEPT_NOTIFY
    qDebug().noquote() << "[VideoCallManager][uid=" << UserMgr::GetInstance()->GetUid()
                       << "]: handleCallAccept (caller) called, call_id=" << call_id
                       << " room_id=" << room_id
                       << " _current_call_id=" << _current_call_id
                       << " _room_id=" << _room_id
                       << " state=" << static_cast<int>(_state);

    if (_current_call_id == call_id && !_room_id.isEmpty() && _state == VideoCallState::Connecting) {
        qDebug() << "[VideoCallManager]: handleCallAccept called twice, ignore";
        return;
    }

    _current_call_id = call_id;
    _room_id = room_id;
    _turn_ws_url = turn_ws_url;
    _ice_servers = ice_servers;

    // 立即切换状态
    setState(VideoCallState::Connecting);

    qDebug() << "[VideoCallManager]: handleCallAccept -> calling initMetartc...";
    initMetartc();
    qDebug() << "[VideoCallManager]: handleCallAccept -> initMetartc done, _rtcWrapper =" << (void*)_rtcWrapper;

    qDebug() << "[VideoCallManager]: handleCallAccept -> calling initWebSocket...";
    initWebSocket();
    qDebug() << "[VideoCallManager]: handleCallAccept -> initWebSocket done";

    if (_rtcWrapper) {
        qDebug() << "[VideoCallManager]: handleCallAccept (caller) -> Calling configureIceServers...";
        _rtcWrapper->configureIceServers(_ice_servers);
        qDebug() << "[VideoCallManager]: configureIceServers returned, calling initPeerConnection...";
        if (!_rtcWrapper->initPeerConnection()) {
            qDebug() << "[VideoCallManager]: initPeerConnection FAILED";
            emit sigError("初始化 PeerConnection 失败");
            endCall();
            return;
        }
        qDebug() << "[VideoCallManager]: initPeerConnection returned, calling initRtcConnection...";
        _rtcWrapper->initRtcConnection(_turn_ws_url);
        qDebug() << "[VideoCallManager]: initRtcConnection returned, calling addTracks...";
        if (!_rtcWrapper->addAudioTrack()) {
            qDebug() << "[VideoCallManager]: addAudioTrack FAILED";
            emit sigError("初始化音频轨道失败");
            endCall();
            return;
        }
        qDebug() << "[VideoCallManager]: addAudioTrack OK, calling addVideoTrack...";
        if (!_rtcWrapper->addVideoTrack()) {
            qDebug() << "[VideoCallManager]: addVideoTrack FAILED";
            emit sigError("初始化视频轨道失败");
            endCall();
            return;
        }
        qDebug() << "[VideoCallManager]: addVideoTrack OK, calling addTransceiver...";
        if (!_rtcWrapper->addTransceiver()) {
            qDebug() << "[VideoCallManager]: addTransceiver FAILED";
            emit sigError("初始化 transceiver 失败");
            endCall();
            return;
        }
        qDebug() << "[VideoCallManager]: addTransceiver OK";
    } else {
        qDebug() << "[VideoCallManager]: handleCallAccept (caller) -> _rtcWrapper is NULL!";
    }

    // 开启本地摄像头预览（主叫方在 startCall 时已启动，这里的 startLocalVideo 内部有 if (_camera) return 保护）
    qDebug() << "[VideoCallManager]: handleCallAccept (caller) -> Calling startLocalVideo...";
    startLocalVideo();
    qDebug() << "[VideoCallManager]: startLocalVideo returned";

    // 所有初始化完毕后，才打开 WebSocket 连接信令服务器
    if (_signaling_socket && _signaling_socket->state() == QAbstractSocket::UnconnectedState && !_turn_ws_url.isEmpty()) {
        qDebug() << "[VideoCallManager]: Connecting to signaling server:" << _turn_ws_url;
        _signaling_socket->open(QUrl(_turn_ws_url));
    } else {
        qDebug() << "[VideoCallManager]: WebSocket skip - socket=" << (void*)_signaling_socket
                 << " state=" << (_signaling_socket ? _signaling_socket->state() : -1)
                 << " url=" << _turn_ws_url;
    }

    emit sigCallAccepted(call_id, room_id, turn_ws_url, ice_servers);

    qDebug() << "[VideoCallManager]: Call accepted (caller), connecting to signaling server, room:" << room_id;
}

void VideoCallManager::handleCallReject(const QString &call_id, const QString &reason)
{
    endCall();
    emit sigCallRejected(call_id, reason);

    qDebug() << "[VideoCallManager]: Call rejected, reason:" << reason;
}

void VideoCallManager::handleCallHangup(const QString &call_id)
{
    Q_UNUSED(call_id);
    endCall();
    emit sigCallHangup(call_id);

    qDebug() << "[VideoCallManager]: Call hangup received";
}

void VideoCallManager::setState(VideoCallState state)
{
    _state = state;
    emit sigCallStateChanged(state);
}
