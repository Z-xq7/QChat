#ifndef VIDEOCALLMANAGER_H
#define VIDEOCALLMANAGER_H

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QTimer>
#include <memory>
#include "singleton.h"
#include "global.h"
#include "userdata.h"
#include "incomingcalldialog.h"

// 添加metartc封装类的前向声明
class YangRtcWrapper;

// 视频通话状态
enum class VideoCallState {
    Idle,           // 空闲
    Ringing,        // 响铃中
    InCall,         // 通话中
    Connecting      // 连接中
};
Q_DECLARE_METATYPE(VideoCallState)

// 视频通话事件
struct VideoCallEvent {
    QString call_id;
    QString room_id;
    QString turn_ws_url;
    QJsonArray ice_servers;
    int caller_uid;
    int callee_uid;
    QString reason;
};

class VideoCallManager : public QObject, public Singleton<VideoCallManager>
{
    Q_OBJECT
    friend class Singleton<VideoCallManager>;

public:
    ~VideoCallManager();

    // 启动视频通话
    void startCall(int callee_uid);
    // 接受视频通话（由来电弹窗点击接受触发，只负责给服务器发 ACCEPT_REQ）
    void sendAcceptCallRequest(const QString& call_id, int caller_uid);
    // 拒绝视频通话
    void rejectCall(const QString& call_id, int caller_uid, const QString& reason = "rejected");
    // 挂断视频通话
    void hangupCall(const QString& call_id = "");
    // 结束通话（内部清理状态）
    void endCall();

    // 获取当前通话状态
    VideoCallState getState() const { return _state; }
    // 获取当前通话ID
    QString getCurrentCallId() const { return _current_call_id; }
    // 获取当前通话的对端用户ID
    int getCurrentPeerUid() const { return _current_peer_uid; }
    // 设置好友信息（用于发起通话时）
    void setFriendInfo(std::shared_ptr<UserInfo> user_info) { _friend_info = user_info; }
    // 通过信令服务器发送消息
    void sendSignalingMessage(const QJsonObject& message);

    // 公开rtcWrapper实例供外部访问（如VideoCallWindow）
    YangRtcWrapper* getRtcWrapper() const { return _rtcWrapper; }

// 处理服务器消息的槽函数（由TcpMgr调用）
public slots:
    // 来电通知（由 TcpMgr 转发 CALL_INCOMING_NOTIFY）
    void handleCallIncoming(int caller_uid, const QString& call_id, const QString& caller_name);
    // 服务器给本端的 CALL_ACCEPT_RSP（来电方被我接受后，服务器回包给我）
    void handleAcceptCall(const QString& call_id,
                          const QString& room_id,
                          const QString& turn_ws_url,
                          const QJsonArray& ice_servers);
    // 服务器给对端的 CALL_ACCEPT_NOTIFY（对方接受了我发起的通话）
    void handleCallAccept(const QString& call_id,
                          const QString& room_id,
                          const QString& turn_ws_url,
                          const QJsonArray& ice_servers);
    // 通话被拒绝
    void handleCallReject(const QString& call_id, const QString& reason);
    // 通话被挂断
    void handleCallHangup(const QString& call_id);

    // WebSocket 信令消息处理（由 QWebSocket::textMessageReceived 调用）
    void handleSignalingMessage(const QString& type,
                                const QString& sdp,
                                const QString& candidate,
                                int sdpMLineIndex,
                                const QString& sdpMid,
                                bool isInitiator);

private:
    VideoCallManager();

    // 初始化WebSocket连接
    void initWebSocket();
    // 初始化信令处理器（目前为空，占位）
    void initSignalingHandlers();
    // 初始化信号连接（避免初始化时的循环依赖）
    void initializeConnections();
    // 设置状态
    void setState(VideoCallState state);

    // 初始化metartc相关
    void initMetartc();
    // 创建offer
    QString createOffer();
    // 设置本地描述
    bool setLocalDescription(const QString& sdp, const QString& type);
    // 设置远程描述
    bool setRemoteDescription(const QString& sdp, const QString& type);
    // 添加ICE候选
    bool addIceCandidate(const QString& candidate, int sdpMLineIndex, const QString& sdpMid);

    // 处理 WebSocket 信令子类型
    void handleOfferInternal(const QString& sdp);
    void handleAnswerInternal(const QString& sdp);
    void handleIceCandidateInternal(const QString& candidate, int sdpMLineIndex, const QString& sdpMid);

private:
    VideoCallState _state;
    QString _current_call_id;
    int _current_peer_uid;
    QString _room_id;
    QString _turn_ws_url;
    QJsonArray _ice_servers;

    QWebSocket* _signaling_socket;
    std::shared_ptr<UserInfo> _friend_info;

    // metartc封装类实例
    YangRtcWrapper* _rtcWrapper;

public:
    // 来电弹窗
    IncomingCallDialog* _incomingCallDialog;

signals:
    // 通话事件信号
    void sigIncomingCall(int caller_uid, const QString& call_id, const QString& caller_name);
    void sigCallAccepted(const QString& call_id, const QString& room_id, const QString& turn_ws_url, const QJsonArray& ice_servers);
    void sigCallRejected(const QString& call_id, const QString& reason);
    void sigCallHangup(const QString& call_id);
    void sigCallStateChanged(VideoCallState new_state);
    void sigError(const QString& error_msg);

    // WebRTC相关信号
    void sigLocalStreamReady();
    void sigRemoteStreamReady();
};

#endif // VIDEOCALLMANAGER_H
