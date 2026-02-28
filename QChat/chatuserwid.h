#ifndef CHATUSERWID_H
#define CHATUSERWID_H

#include <QWidget>
#include "listitembase.h"
#include "userdata.h"

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
    //设置聊天列表用户item信息（由用户信息）
    void SetInfo(std::shared_ptr<UserInfo> user_info);
    //设置聊天列表用户item信息(由好友信息)
    void SetInfo(std::shared_ptr<FriendInfo> friend_info);
    //获取用户信息
    std::shared_ptr<UserInfo> GetUserInfo();
    //更新聊天列表显示的最后一条聊天消息
    void updateLastMsg(std::vector<std::shared_ptr<TextChatData>> msgs);

private:
    Ui::ChatUserWid *ui;
    std::shared_ptr<UserInfo> _user_info;

};

#endif // CHATUSERWID_H
