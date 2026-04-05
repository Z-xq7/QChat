#ifndef CONUSERITEM_H
#define CONUSERITEM_H

#include <QWidget>
#include "listitembase.h"
#include "userdata.h"
#include <QLabel>

namespace Ui {
class ConUserItem;
}

class ConUserItem : public ListItemBase
{
    Q_OBJECT

public:
    explicit ConUserItem(QWidget *parent = nullptr);
    ~ConUserItem();

    QSize sizeHint() const override;
    void SetInfo(std::shared_ptr<AuthInfo> auth_info);
    void SetInfo(std::shared_ptr<AuthRsp> auth_rsp);
    void SetInfo(int uid, QString name, QString icon);
    void ShowRedPoint(bool show = false);
    std::shared_ptr<UserInfo> GetInfo();
    // 设置在线状态（显示在头像右下角）
    void SetOnlineStatus(bool online);
    // 隐藏在线状态标签（"新的朋友"等非联系人条目使用）
    void HideOnlineStatus();
    // 隐藏签名标签（"新的朋友"等非联系人条目使用）
    void HideUserSignature();

    void LoadHeadIcon(QString avatarPath, QLabel* icon_label, QString file_name, QString req_type);

private:
    Ui::ConUserItem *ui;
    std::shared_ptr<UserInfo> _info;
};

#endif // CONUSERITEM_H
