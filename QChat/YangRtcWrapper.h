#ifndef YANGRTCWRAPPER_H
#define YANGRTCWRAPPER_H

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QByteArray>
#include <QMetaType>
#include <memory>

// metartc库头文件
extern "C" {
#include <yangrtc/YangPeerConnection.h>
#include <yangutil/yangavinfotype.h>
#include <yangutil/yangavtype.h>
#include <yangutil/yangavctype.h>
}

// 定义RTC连接状态枚举
enum class YangRtcState {
    Idle,
    Connecting,
    Connected,
    Disconnected,
    Failed
};
Q_DECLARE_METATYPE(YangRtcState)

// 用于封装metartc的类
class YangRtcWrapper : public QObject
{
    Q_OBJECT

public:
    explicit YangRtcWrapper(QObject *parent = nullptr);
    ~YangRtcWrapper();

    // 初始化RTC连接
    bool initRtcConnection(const QString& serverUrl, int localPort = 0);
    
    // 配置ICE服务器（TURN/STUN）
    bool configureIceServers(const QJsonArray& iceServers);

    // 初始化 PeerConnection（必须在 configureIceServers 之后调用！）
    // 因为 yang_create_peerConnection 内部会复制 avinfo->rtc.iceCandidateType，
    // 如果在设置 ICE 类型之前调用，ICE 会始终走 Host 模式。
    bool initPeerConnection();
    
    // 创建offer
    QString createOffer();
    
    // 创建answer
    QString createAnswer();
    
    // 设置本地描述
    bool setLocalDescription(const QString& sdp, const QString& type);
    
    // 设置远程描述
    bool setRemoteDescription(const QString& sdp, const QString& type);
    
    // 添加ICE候选
    bool addIceCandidate(const QString& candidate, int sdpMLineIndex, const QString& sdpMid);
    
    // 添加音频轨道
    bool addAudioTrack();
    
    // 添加视频轨道
    bool addVideoTrack();
    
    // 添加 transceiver（设置流方向，demo 中在 addTrack 之后调用）
    bool addTransceiver(YangRtcDirection direction = YangSendrecv);
    
    // 向远端发送已经编码好的 I420 视频帧
    void sendVideoI420(uint8_t* data, int len, int64_t timestamp);
    
    // 连接服务器
    bool connectToServer();
    
    // 关闭连接
    void closeConnection();
    
    // 获取当前连接状态
    YangRtcState getState() const { return m_state; }
    
    // 设置本地视频回调（本地预览）
    void setLocalVideoCallback(std::function<void(const QByteArray&, int, int, int)> callback) {
        m_localVideoCallback = callback;
    }
    
    // 设置远程视频回调
    void setRemoteVideoCallback(std::function<void(const QByteArray&, int, int, int)> callback) {
        m_remoteVideoCallback = callback;
    }

    // 设置当前流方向（sendrecv / recvonly / sendonly）
    void setDirection(YangRtcDirection dir);

    // 设置当前端是否为 controlling 端（P2P ICE）
    void setControlled(bool controlled);

signals:
    void sigLocalStreamReady();
    void sigRemoteStreamReady();
    void sigIceCandidateReady(const QString& candidate, int sdpMLineIndex, const QString& sdpMid);
    void sigConnectionStateChanged(YangRtcState newState);
    void sigError(const QString& error);

private:
    void setState(YangRtcState state);
    
    // metartc回调函数 - 用于PeerConnection级别的事件
    static int32_t onAudioFrame(YangPeer* peer, YangFrame *audioFrame);
    static int32_t onVideoFrame(YangPeer* peer, YangFrame *videoFrame);
    static int32_t onMessageFrame(YangPeer* peer, YangFrame *msgFrame);
    
    // metartc回调函数 - 用于接收音视频帧（注意返回类型是void，符合YangReceiveCallback定义）
    static void onRecvAudioFrame(void* context, YangFrame *audioFrame);
    static void onRecvVideoFrame(void* context, YangFrame *videoFrame);
    static void onRecvMessageFrame(void* context, YangFrame *msgFrame);
    
private:
    YangPeerConnection m_peerConnection;
    YangAVInfo m_avInfo;
    
    YangRtcState m_state;
    QString m_serverUrl;
    
    // 视频回调函数：使用 QByteArray 携带一帧数据
    std::function<void(const QByteArray&, int, int, int)> m_localVideoCallback;
    std::function<void(const QByteArray&, int, int, int)> m_remoteVideoCallback;
    
    // 用于模拟连接状态的计时器
    QTimer* m_connectTimer;

    QByteArray m_lastLocalSdp;
    QByteArray m_lastRemoteSdp;
    
    // 用于在回调中访问实例的静态映射
    static QHash<void*, YangRtcWrapper*> s_wrapperMap;
};

#endif // YANGRTCWRAPPER_H
