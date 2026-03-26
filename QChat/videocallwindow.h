#ifndef VIDEOCALLWINDOW_H
#define VIDEOCALLWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPoint>
#include "qvideooutputwidget.h"
#include "videocallmanager.h"

namespace Ui {
class VideoCallWindow;
}

class VideoCallWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit VideoCallWindow(QWidget *parent = nullptr);
    ~VideoCallWindow();
    
    // 初始化视频控件
    void initVideoWidgets();
    // 设置本地视频控件
    void setLocalVideoWidget(QVideoOutputWidget* widget);
    // 设置远端视频控件
    void setRemoteVideoWidget(QVideoOutputWidget* widget);
    // 设置通话用户信息
    void setUserInfo(const QString& nick, int uid);
    // 更新通话状态文本
    void updateCallStatus(const QString& status);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Ui::VideoCallWindow *ui;
    QVideoOutputWidget* m_localVideoWidget;
    QVideoOutputWidget* m_remoteVideoWidget;
    QLabel* m_statusLabel;  // 状态标签
    int m_peerUid;          // 对端用户ID
    bool m_useRtcLocal;
    bool m_dragging;
    QPoint m_dragOffset;

private slots:
    void slot_close_window();
    void slot_voice_clicked();
    void slot_video_clicked();
    void slot_leave_clicked();
    void onMinSizeClicked();
    void onMaxSizeClicked();
    void onCloseClicked();

    // 视频通话管理器信号槽
    void onIncomingCall(int caller_uid, const QString& call_id, const QString& caller_name);
    void onCallAccepted(const QString& call_id, const QString& room_id, const QString& turn_ws_url, const QJsonArray& ice_servers);
    void onCallRejected(const QString& call_id, const QString& reason);
    void onCallHangup(const QString& call_id);
    void onCallStateChanged(VideoCallState new_state);
    void onError(const QString& error_msg);
    void onLocalPreviewFrame(const QByteArray& frame, int width, int height, int format);
};

#endif // VIDEOCALLWINDOW_H
