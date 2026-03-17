#ifndef YANGRTCWRAPPER_H
#define YANGRTCWRAPPER_H

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QByteArray>
#include <memory>

// metartc库头文件
extern "C" {
#include <yangrtc/YangPeerConnection.h>
#include <yangutil/yangavinfotype.h>
#include <yangutil/yangavtype.h>
}

// push/capture 相关
#include <yangutil/yangtype.h>
#include <yangutil/sys/YangSysMessageHandle.h>
#include <yangutil/sys/YangSysMessageHandleI.h>
#include <yangutil/sys/YangSysMessageI.h>
#include <yangutil/sys/YangCTimer.h>
#include <yangutil/sys/YangLog.h>
#include <yangutil/sys/YangIni.h>
#include <yangutil/buffer/YangVideoBuffer.h>
#include <yangutil/buffer/YangAudioBuffer.h>
#include <yangutil/sys/YangSysMessage.h>
#include <yangutil/sys/YangThread2.h>
#include "../yangpush/YangPushMessageHandle.h"
#include "../yangpush/YangPushHandleImpl.h"
#include "../thirdparty/metartc/include/yangpush/YangRtcPublish.h"

// 定义RTC连接状态枚举
enum class YangRtcState {
    Idle,
    Connecting,
    Connected,
    Disconnected,
    Failed
};

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

    // 连接服务器（对当前P2P模式下无实际操作）
    bool connectToServer();

    // 关闭连接
    void closeConnection();

    // 获取当前连接状态
    YangRtcState getState() const { return m_state; }

    // 使用 QByteArray 传递视频帧，避免跨线程悬空指针
    void setLocalVideoCallback(std::function<void(const QByteArray&, int, int, int)> callback) {
        m_localVideoCallback = callback;
    }

    void setRemoteVideoCallback(std::function<void(const QByteArray&, int, int, int)> callback) {
        m_remoteVideoCallback = callback;
    }

    YangContext* getContext() const { return m_ctx; } // 可选：用于获取底层 YangContext

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

    // metartc回调函数 - 用于接收音视频帧
    static void onRecvAudioFrame(void* context, YangFrame *audioFrame);
    static void onRecvVideoFrame(void* context, YangFrame *videoFrame);
    static void onRecvMessageFrame(void* context, YangFrame *msgFrame);

    // 初始化 push/capture 流水线，开始从摄像头采集并发送到当前 PeerConnection
    void initCaptureAndPublish();
    void stopCaptureAndPublish();

private:
    YangPeerConnection m_peerConnection;
    YangPeer m_peer;
    YangAVInfo m_avInfo;

    YangRtcState m_state;
    QString m_serverUrl;

    // 视频回调函数：使用 QByteArray 携带一帧数据
    std::function<void(const QByteArray&, int, int, int)> m_localVideoCallback;
    std::function<void(const QByteArray&, int, int, int)> m_remoteVideoCallback;

    // 用于模拟连接状态的计时器
    QTimer* m_connectTimer;

    // push/pub/capture 相关
    YangContext* m_ctx;
    YangPushMessageHandle* m_pushMsgHandle;
    YangPushHandleImpl* m_pushHandle;
    YangRtcPublish* m_rtcPublish;
    bool m_localStreamNotified;
    bool m_remoteStreamNotified;

    // 用于在回调中访问实例的静态映射
    static QHash<void*, YangRtcWrapper*> s_wrapperMap;
};

#endif // YANGRTCWRAPPER_H
