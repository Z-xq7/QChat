#ifndef CONTACTUSERLIST_H
#define CONTACTUSERLIST_H
#include <QListWidget>
#include <QEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QDebug>
#include <QTimer>
#include <memory>
#include "userdata.h"
class ConUserItem;

class ContactUserList : public QListWidget
{
    Q_OBJECT
public:
    ContactUserList(QWidget* parent = nullptr);
    //消息提示红点显示
    void ShowRedPoint(bool bshow = true);

protected:
    //事件过滤器
    bool eventFilter(QObject *watched, QEvent *event) override ;

private:
    //模拟添加用户列表
    void addContactUserList();

public slots:
    //点击每个item
    void slot_item_clicked(QListWidgetItem *item);
    //申请添加好友被对方同意成功
    void slot_add_auth_firend(std::shared_ptr<AuthInfo>);
    //同意添加对方为好友
    void slot_auth_rsp(std::shared_ptr<AuthRsp>);

signals:
    //加载联系人
    void sig_loading_contact_user();
    //切换到申请好友页面
    void sig_switch_apply_friend_page();
    //切换好友信息页面
    void sig_switch_friend_info_page(std::shared_ptr<UserInfo> user_info);

private:
    ConUserItem* _add_friend_item;
    QListWidgetItem * _groupitem;
    bool _load_pending;
};

#endif // CONTACTUSERLIST_H
