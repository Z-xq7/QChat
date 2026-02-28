#ifndef APPLYFRIENDPAGE_H
#define APPLYFRIENDPAGE_H

#include <QWidget>
#include "userdata.h"
#include <memory>
#include <QJsonArray>
#include <unordered_map>
#include "applyfrienditem.h"

namespace Ui {
class ApplyFriendPage;
}

class ApplyFriendPage : public QWidget
{
    Q_OBJECT

public:
    explicit ApplyFriendPage(QWidget *parent = nullptr);
    ~ApplyFriendPage();
    //添加新的好友申请
    void AddNewApply(std::shared_ptr<AddFriendApply> apply);

protected:
    //喷绘
    void paintEvent(QPaintEvent *event);

private:
    //加载申请列表
    void loadApplyList();
    Ui::ApplyFriendPage *ui;
    std::unordered_map<int, ApplyFriendItem*> _unauth_items;

public slots:
    //同意添加好友槽函数
    void slot_auth_rsp(std::shared_ptr<AuthRsp> );

signals:
    //搜索好友后显示信息
    void sig_show_search(bool);
};

#endif // APPLYFRIENDPAGE_H
