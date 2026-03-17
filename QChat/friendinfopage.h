#ifndef FRIENDINFOPAGE_H
#define FRIENDINFOPAGE_H

#include <QWidget>
#include "userdata.h"
#include <QLabel>
#include "videocallwindow.h"
#include "videocallmanager.h"

namespace Ui {
class FriendInfoPage;
}

class FriendInfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit FriendInfoPage(QWidget *parent = nullptr);
    ~FriendInfoPage();
    void SetInfo(std::shared_ptr<UserInfo> ui);
    void LoadHeadIcon(QString avatarPath, QLabel* icon_label, QString file_name, QString req_type);

private slots:
    void on_msg_chat_clicked();

    void on_video_chat_clicked();
    
    // 视频通话管理器信号处理槽函数
    void onIncomingCall(int caller_uid, const QString& call_id, const QString& caller_name);
    void onCallAccepted(const QString& call_id, const QString& room_id, const QString& turn_ws_url, const QJsonArray& ice_servers);
    void onCallRejected(const QString& call_id, const QString& reason);
    void onCallHangup(const QString& call_id);
    void onCallStateChanged(VideoCallState new_state);

private:
    Ui::FriendInfoPage* ui;
    VideoCallWindow* video_window;
    std::shared_ptr<UserInfo> _user_info;

signals:
    //跳转到聊天页面
    void sig_jump_chat_item(std::shared_ptr<UserInfo> si);
};

#endif // FRIENDINFOPAGE_H
