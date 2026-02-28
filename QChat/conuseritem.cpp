#include "conuseritem.h"
#include "ui_conuseritem.h"

ConUserItem::ConUserItem(QWidget *parent) :
    ListItemBase(parent),
    ui(new Ui::ConUserItem)
{
    ui->setupUi(this);
    SetItemType(ListItemType::CONTACT_USER_ITEM);
    ui->red_point->raise();
    ShowRedPoint(false);
}

ConUserItem::~ConUserItem()
{
    delete ui;
}

QSize ConUserItem::sizeHint() const
{
    return QSize(230, 65); // 返回自定义的尺寸
}

void ConUserItem::SetInfo(std::shared_ptr<AuthInfo> auth_info)
{
    if (!auth_info) {
        ui->user_name_lb->setText("");
        ui->icon_lb->clear();
        return;
    }
    _info = std::make_shared<UserInfo>(auth_info);
    // 加载图片，先检查图片路径是否有效
    QPixmap pixmap;
    if (!pixmap.load(_info->_icon)) {
        // 如果图片加载失败，可以设置默认图片
        ui->icon_lb->clear();
        pixmap = QPixmap(":/images/head_default.png");
    } else {
        // 设置图片自动缩放
        ui->icon_lb->setPixmap(pixmap.scaled(ui->icon_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->icon_lb->setScaledContents(true);
    }
    ui->user_name_lb->setText(_info->_name);
}

void ConUserItem::SetInfo(int uid, QString name, QString icon)
{
    _info = std::make_shared<UserInfo>(uid,name, icon);
    // 加载图片，先检查图片路径是否有效
    QPixmap pixmap;
    if (!_info || !pixmap.load(_info->_icon)) {
        // 如果图片加载失败，可以设置默认图片
        ui->icon_lb->clear();
        pixmap = QPixmap(":/images/head_default.png");
    } else {
        // 设置图片自动缩放
        ui->icon_lb->setPixmap(pixmap.scaled(ui->icon_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->icon_lb->setScaledContents(true);
    }
    if (_info) {
        ui->user_name_lb->setText(_info->_name);
    } else {
        ui->user_name_lb->setText(name);
    }
}

void ConUserItem::SetInfo(std::shared_ptr<AuthRsp> auth_rsp){
    if (!auth_rsp) {
        ui->user_name_lb->setText("");
        ui->icon_lb->clear();
        return;
    }
    _info = std::make_shared<UserInfo>(auth_rsp);
    // 加载图片，先检查图片路径是否有效
    QPixmap pixmap;
    if (!pixmap.load(_info->_icon)) {
        // 如果图片加载失败，可以设置默认图片
        ui->icon_lb->clear();
        pixmap = QPixmap(":/images/head_default.png");
    } else {
        // 设置图片自动缩放
        ui->icon_lb->setPixmap(pixmap.scaled(ui->icon_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->icon_lb->setScaledContents(true);
    }
    ui->user_name_lb->setText(_info->_name);
}

void ConUserItem::ShowRedPoint(bool show)
{
    if(show){
        ui->red_point->show();
    }else{
        ui->red_point->hide();
    }
}

std::shared_ptr<UserInfo> ConUserItem::GetInfo()
{
    return _info;
}
