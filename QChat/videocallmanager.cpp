#include "videocallmanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QTimer>
#include <QPainter>
#include "tcpmgr.h"
#include "usermgr.h"
#include "YangRtcWrapper.h"

VideoCallManager::VideoCallManager()
    : _state(VideoCallState::Idle)
    , _current_call_id("")
    , _current_peer_uid(0)
    , _room_id("")
    , _turn_ws_url("")
    , _signaling_socket(nullptr)
    , _rtcWrapper(nullptr)
    , _incomingCallDialog(nullptr)
{
    // 延迟初始化信号连接，避免初始化时的循环依赖
    initializeConnections();
}

VideoCallManager::~VideoCallManager()
{
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

void VideoCallManager::initializeConnections()
{
    // 连接TcpMgr的视频通话相关信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_call_incoming,
            this, &VideoCallManager::handleCallIncoming);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_accept_call,
            this, &VideoCallManager::handleAcceptCall);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_call_accepted,
            this, &VideoCallManager::handleCallAccept);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_call_rejected,
            this, &VideoCallManager::handleCallReject);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_call_hangup,
            this, &VideoCallManager::handleCallHangup);
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
        qDebug() << "[VideoCallManager]: Signaling server connected";

        QJsonObject joinMsg;
        joinMsg["type"] = "join";
        joinMsg["roomId"] = _room_id;
        joinMsg["uid"] = UserMgr::GetInstance()->GetUid();
        joinMsg["call_id"] = _current_call_id;
        bool isCaller = (_current_peer_uid != 0 && _state == VideoCallState::Connecting && !_current_call_id.isEmpty());
        joinMsg["role"] = isCaller ? "caller" : "callee";
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
                int peers = obj.value("peers").toInt();
                bool isCaller = (_current_peer_uid != 0 && _state == VideoCallState::Connecting && !_current_call_id.isEmpty());
                if (peers == 0) {
                    isInitiator = isCaller; // 房间中只有自己
                } else {
                    isInitiator = isCaller; // 第二个加入的一样根据 isCaller 判断
                }
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
    qDebug() << "[VideoCallManager]: Sent call invite request to user" << callee_uid;
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
    if (type == "joined" || type == "ready") {
        if (isInitiator) {
            QString offerSdp = createOffer();
            if (!offerSdp.isEmpty()) {
                QJsonObject offerMsg;
                offerMsg["type"] = "offer";
                offerMsg["sdp"] = offerSdp;
                sendSignalingMessage(offerMsg);

                setLocalDescription(offerSdp, "offer");
            }
        } else {
            qDebug() << "[VideoCallManager]: Joined room as responder, waiting for offer";
        }
    } else if (type == "offer") {
        handleOfferInternal(sdp);
    } else if (type == "answer") {
        handleAnswerInternal(sdp);
    } else if (type == "ice") {
        handleIceCandidateInternal(candidate, sdpMLineIndex, sdpMid);
    } else if (type == "peer-left") {
        qDebug() << "[VideoCallManager]: Remote peer left room, ending call";
        endCall();
    } else if (type == "error") {
        // TurnServer 的 error 消消息结构为 { type: "error", message: "..." }
        qDebug() << "[VideoCallManager]: Signaling error from server";
        emit sigError("Signaling error: " + sdpMid); // 简化处理
    } else {
        qDebug() << "[VideoCallManager]: Unknown signaling message type:" << type;
    }
}

void VideoCallManager::handleOfferInternal(const QString &sdp)
{
    qDebug() << "[VideoCallManager]: Received offer, handling with metartc";

    if (sdp.isEmpty()) return;

    if (setRemoteDescription(sdp, "offer") && _rtcWrapper) {
        QString answerSdp = _rtcWrapper->createAnswer();
        if (!answerSdp.isEmpty()) {
            QJsonObject answerMsg;
            answerMsg["type"] = "answer";
            answerMsg["sdp"] = answerSdp;
            sendSignalingMessage(answerMsg);

            setLocalDescription(answerSdp, "answer");
        } else {
            qDebug() << "[VideoCallManager]: Failed to create answer";
        }
    }
}

void VideoCallManager::handleAnswerInternal(const QString &sdp)
{
    qDebug() << "[VideoCallManager]: Received answer, handling with metartc";

    if (sdp.isEmpty()) return;

    if (setRemoteDescription(sdp, "answer")) {
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
    if (_current_call_id == call_id && !_room_id.isEmpty() && _state == VideoCallState::Connecting) {
        qDebug() << "[VideoCallManager]: handleAcceptCall called twice, ignore";
        return;
    }

    _current_call_id = call_id;
    _room_id = room_id;
    _turn_ws_url = turn_ws_url;
    _ice_servers = ice_servers;

    initMetartc();
    initWebSocket();

    setState(VideoCallState::Connecting);

    if (_rtcWrapper) {
        _rtcWrapper->initRtcConnection(_turn_ws_url);
        _rtcWrapper->configureIceServers(_ice_servers);
        _rtcWrapper->addAudioTrack();
        _rtcWrapper->addVideoTrack();
    }

    if (_signaling_socket && _signaling_socket->state() == QAbstractSocket::UnconnectedState && !_turn_ws_url.isEmpty()) {
        _signaling_socket->open(QUrl(_turn_ws_url));
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
    if (_current_call_id == call_id && !_room_id.isEmpty() && _state == VideoCallState::Connecting) {
        qDebug() << "[VideoCallManager]: handleCallAccept called twice, ignore";
        return;
    }

    _current_call_id = call_id;
    _room_id = room_id;
    _turn_ws_url = turn_ws_url;
    _ice_servers = ice_servers;

    initMetartc();
    initWebSocket();

    setState(VideoCallState::Connecting);

    if (_rtcWrapper) {
        _rtcWrapper->initRtcConnection(_turn_ws_url);
        _rtcWrapper->configureIceServers(_ice_servers);
        _rtcWrapper->addAudioTrack();
        _rtcWrapper->addVideoTrack();
    }

    if (_signaling_socket && _signaling_socket->state() == QAbstractSocket::UnconnectedState && !_turn_ws_url.isEmpty()) {
        _signaling_socket->open(QUrl(_turn_ws_url));
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
