#ifndef APPLYFRIENDITEM_H
#define APPLYFRIENDITEM_H

#include <QWidget>
#include <listitembase.h>
#include "userdata.h"
#include <memory>
#include <QLabel>

namespace Ui {
class ApplyFriendItem;
}

class ApplyFriendItem : public ListItemBase
{
    Q_OBJECT

public:
    explicit ApplyFriendItem(QWidget *parent = nullptr);
    ~ApplyFriendItem();
    void SetInfo(std::shared_ptr<ApplyInfo> apply_info);
    void ShowAddBtn(bool bshow);
    QSize sizeHint() const override {
        return QSize(250, 80); // 返回自定义的尺寸
    }
    int GetUid();
    //加载头像
    void LoadHeadIcon(QString avatarPath, QLabel* icon_label, QString file_name, QString req_type);

private:
    Ui::ApplyFriendItem *ui;
    std::shared_ptr<ApplyInfo> _apply_info;
    bool _added;

signals:
    //同意添加好友信号
    void sig_auth_friend(std::shared_ptr<ApplyInfo> apply_info);
};

#endif // APPLYFRIENDITEM_H
