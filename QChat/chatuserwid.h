#ifndef CHATUSERWID_H
#define CHATUSERWID_H

#include <QWidget>
#include "listitembase.h"
#include "usermgr.h"
#include <QLabel>

namespace Ui {
class ChatUserWid;
}

class ChatUserWid : public ListItemBase
{
    Q_OBJECT

public:
    explicit ChatUserWid(QWidget *parent = nullptr);
    ~ChatUserWid();
    QSize sizeHint() const override {
        return QSize(230, 65); // 返回自定义的尺寸
    }
    //（已修改）设置聊天列表用户item信息
    void SetChatData(std::shared_ptr<ChatThreadData> chat_data);
    //（已修改）获取聊天列表用户item信息
    std::shared_ptr<ChatThreadData> GetChatData();
    //显示/更新未读消息计数角标
    void UpdateUnreadCount(int count);
    //更新聊天列表显示的最后一条聊天消息
    void UpdateLastMsg(const QString& msg);
    //设置最后消息时间（模仿微信格式）
    void SetLastMsgTime(const QString& time);
    // //设置聊天列表用户item信息（由用户信息）
    // void SetInfo(std::shared_ptr<UserInfo> user_info);
    // //设置聊天列表用户item信息(由好友信息)
    // void SetInfo(std::shared_ptr<FriendInfo> friend_info);
    // //获取用户信息
    // std::shared_ptr<UserInfo> GetUserInfo();
    //更新聊天列表显示的最后一条聊天消息
    void updateLastMsg(std::vector<std::shared_ptr<TextChatData>> msgs);
    //加载头像
    void LoadHeadIcon(QString avatarPath, QLabel* icon_label, QString file_name, QString req_type);

private:
    Ui::ChatUserWid *ui;
    //std::shared_ptr<UserInfo> _user_info;
    std::shared_ptr<ChatThreadData> _chat_data;

};

#endif // CHATUSERWID_H
