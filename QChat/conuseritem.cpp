#include "conuseritem.h"
#include "ui_conuseritem.h"
#include <QStandardPaths>
#include "usermgr.h"
#include "filetcpmgr.h"

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

    //使用正则表达式检查是否使用默认头像
    QRegularExpression regex("^:/images/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(_info->_icon);
    if (match.hasMatch()) {
        QPixmap pixmap(_info->_icon);
        QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
        ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
        ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
    }
    else {
        // 如果是用户上传的头像，获取存储目录
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir avatarsDir(storageDir + "/user/" + QString::number(_info->_uid) + "/avatars");
        auto file_name = QFileInfo(_info->_icon).fileName();

        // 确保目录存在
        if (avatarsDir.exists()) {
            QString avatarPath = avatarsDir.filePath(file_name); // 获取上传头像的完整路径
            QPixmap pixmap(avatarPath); // 加载上传的头像图片
            if (!pixmap.isNull()) {
                //判断是否正在下载
                bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
                if (is_loading) {
                    qWarning() << "[ConUserItem]: 正在下载: " << file_name;
                    //先加载默认的
                    QPixmap pixmap(":/images/head_default.png");
                    QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                    ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                    ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

                }
                else {
                    QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                    ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                    ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
                }
            }
            else {
                qWarning() << "[ConUserItem]: 无法加载上传的头像：" << avatarPath;
                LoadHeadIcon(avatarPath, ui->icon_lb, file_name, "other_icon");
            }
        }
        else {
            qWarning() << "[ConUserItem]: 头像存储目录不存在：" << avatarsDir.path();
            QString avatarPath = avatarsDir.filePath(QFileInfo(_info->_icon).fileName());
            avatarsDir.mkpath(".");
            LoadHeadIcon(avatarPath, ui->icon_lb, file_name, "other_icon");
        }
    }

    ui->user_name_lb->setText(_info->_name);
}

void ConUserItem::SetInfo(int uid, QString name, QString icon)
{
    _info = std::make_shared<UserInfo>(uid, name, icon);
    //使用正则表达式检查是否使用默认头像
    QRegularExpression regex("^:/images/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(icon);
    //这里单独处理“新的朋友图标”
    QRegularExpression regex2("^:/images/add_friend.png");
    QRegularExpressionMatch match2 = regex2.match(icon);
    if (match.hasMatch() || match2.hasMatch()) {
        QPixmap pixmap(icon);
        QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
        ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
        ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
    }
    else {
        // 如果是用户上传的头像，获取存储目录
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir avatarsDir(storageDir + "/user/" + QString::number(uid) + "/avatars");
        auto file_name = QFileInfo(icon).fileName();

        // 确保目录存在
        if (avatarsDir.exists()) {
            QString avatarPath = avatarsDir.filePath(file_name); // 获取上传头像的完整路径
            QPixmap pixmap(avatarPath); // 加载上传的头像图片
            if (!pixmap.isNull()) {
                //判断是否正在下载
                bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
                if (is_loading) {
                    qWarning() << "[ConUserItem]: 正在下载: " << file_name;
                    //先加载默认的
                    QPixmap pixmap(":/images/head_default.png");
                    QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                    ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                    ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

                }
                else {
                    QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                    ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                    ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
                }
            }
            else {
                qWarning() << "[ConUserItem]: 无法加载上传的头像：" << avatarPath;
                LoadHeadIcon(avatarPath, ui->icon_lb, file_name, "other_icon");
            }
        }
        else {
            qWarning() << "[ConUserItem]: 头像存储目录不存在：" << avatarsDir.path();
            QString avatarPath = avatarsDir.filePath(QFileInfo(icon).fileName());
            avatarsDir.mkpath(".");
            LoadHeadIcon(avatarPath, ui->icon_lb, file_name, "other_icon");
        }
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

    //使用正则表达式检查是否使用默认头像
    QRegularExpression regex("^:/images/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(_info->_icon);
    if (match.hasMatch()) {
        QPixmap pixmap(_info->_icon);
        QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
        ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
        ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
    }
    else {
        // 如果是用户上传的头像，获取存储目录
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

        QDir avatarsDir(storageDir + "/user/" + QString::number(_info->_uid) + "/avatars");
        auto file_name = QFileInfo(_info->_icon).fileName();

        // 确保目录存在
        if (avatarsDir.exists()) {
            QString avatarPath = avatarsDir.filePath(file_name); // 获取上传头像的完整路径
            QPixmap pixmap(avatarPath); // 加载上传的头像图片
            if (!pixmap.isNull()) {
                //判断是否正在下载
                bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
                if (is_loading) {
                    qWarning() << "[ConUserItem]: 正在下载: " << file_name;
                    //先加载默认的
                    QPixmap pixmap(":/images/head_default.png");
                    QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                    ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                    ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

                }
                else {
                    QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                    ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                    ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
                }
            }
            else {
                qWarning() << "[ConUserItem]: 无法加载上传的头像：" << avatarPath;
                LoadHeadIcon(avatarPath, ui->icon_lb, file_name, "other_icon");
            }
        }
        else {
            qWarning() << "[ConUserItem]: 头像存储目录不存在：" << avatarsDir.path();
            QString avatarPath = avatarsDir.filePath(QFileInfo(_info->_icon).fileName());
            avatarsDir.mkpath(".");
            LoadHeadIcon(avatarPath, ui->icon_lb, file_name, "other_icon");
        }
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

void ConUserItem::LoadHeadIcon(QString avatarPath, QLabel *icon_label, QString file_name, QString req_type)
{
    UserMgr::GetInstance()->AddLabelToReset(avatarPath, ui->icon_lb);
    //先加载默认的
    QPixmap pixmap(":/images/head_default.png");
    QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
    ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
    ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

    //判断是否正在下载
    bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
    if (is_loading) {
        qWarning() << "[ConUserItem]: 正在下载: " << file_name;
    }
    else {
        //发送请求获取资源
        auto download_info = std::make_shared<DownloadInfo>();
        download_info->_name = file_name;
        download_info->_current_size = 0;
        download_info->_seq = 1;
        download_info->_total_size = 0;
        download_info->_client_path = avatarPath;
        //添加文件到管理者
        UserMgr::GetInstance()->AddDownloadFile(file_name, download_info);
        //发送消息
        FileTcpMgr::GetInstance()->SendDownloadInfo(download_info, req_type);
    }
}
