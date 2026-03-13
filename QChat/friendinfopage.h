#ifndef FRIENDINFOPAGE_H
#define FRIENDINFOPAGE_H

#include <QWidget>
#include "userdata.h"
#include <QLabel>

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

private:
    Ui::FriendInfoPage *ui;
    std::shared_ptr<UserInfo> _user_info;

signals:
    //跳转到聊天页面
    void sig_jump_chat_item(std::shared_ptr<UserInfo> si);
};

#endif // FRIENDINFOPAGE_H
