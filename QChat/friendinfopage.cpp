#include "friendinfopage.h"
#include "ui_friendinfopage.h"
#include <QDebug>
#include <QStandardPaths>
#include "usermgr.h"
#include "filetcpmgr.h"

FriendInfoPage::FriendInfoPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FriendInfoPage),_user_info(nullptr)
{
    ui->setupUi(this);
    ui->msg_chat->SetState("normal","hover","press");
    ui->video_chat->SetState("normal","hover","press");
    ui->voice_chat->SetState("normal","hover","press");
}

FriendInfoPage::~FriendInfoPage()
{
    delete ui;
}

void FriendInfoPage::SetInfo(std::shared_ptr<UserInfo> user_info)
{
    _user_info = user_info;

    //使用正则表达式检查是否使用默认头像
    QRegularExpression regex("^:/images/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(_user_info->_icon);
    if (match.hasMatch()) {
        QPixmap pixmap(_user_info->_icon);
        QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
        ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
        ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
    }
    else {
        // 如果是用户上传的头像，获取存储目录
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

        QDir avatarsDir(storageDir + "/user/" + QString::number(_user_info->_uid) + "/avatars");

        // 确保目录存在
        if (avatarsDir.exists()) {
            auto file_name = QFileInfo(_user_info->_icon).fileName();
            QString avatarPath = avatarsDir.filePath(file_name); // 获取上传头像的完整路径
            QPixmap pixmap(avatarPath); // 加载上传的头像图片
            if (!pixmap.isNull()) {
                //判断是否正在下载
                bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
                if (is_loading) {
                    qWarning() << "[FriendInfoPage]: 正在下载: " << file_name;
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
                qWarning() << "[FriendInfoPage]: 无法加载上传的头像：" << avatarPath;
                LoadHeadIcon(avatarPath, ui->icon_lb, file_name, "other_icon");
            }
        }
        else {
            qWarning() << "[FriendInfoPage]: 头像存储目录不存在：" << avatarsDir.path();
            QString avatarPath = avatarsDir.filePath(QFileInfo(_user_info->_icon).fileName());
            avatarsDir.mkpath(".");
            LoadHeadIcon(avatarPath, ui->icon_lb, _user_info->_icon, "other_icon");
        }
    }


    ui->name_lb->setText(user_info->_name);
    ui->nick_lb->setText(user_info->_nick);
    ui->bak_lb->setText(user_info->_nick);
}

void FriendInfoPage::LoadHeadIcon(QString avatarPath, QLabel *icon_label, QString file_name, QString req_type)
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
        qWarning() << "[FriendInfoPage]: 正在下载: " << file_name;
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

void FriendInfoPage::on_msg_chat_clicked()
{
    qDebug() << "[FriendInfoPage]: --- msg chat btn clicked ---";
    emit sig_jump_chat_item(_user_info);
}
