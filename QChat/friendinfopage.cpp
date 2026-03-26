#include "friendinfopage.h"
#include "ui_friendinfopage.h"
#include <QDebug>
#include <QStandardPaths>
#include "usermgr.h"
#include "filetcpmgr.h"
#include "tcpmgr.h"
#include <QMessageBox>
#include <QStyleOption>
#include <QPainter>

FriendInfoPage::FriendInfoPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FriendInfoPage), video_window(nullptr), _user_info(nullptr)
{
    ui->setupUi(this);
    this->setObjectName("friend_info_page");
    this->setAttribute(Qt::WA_StyledBackground); // 确保子窗口能够正确继承样式
    ui->msg_chat->SetState("normal","hover","press");
    ui->video_chat->SetState("normal","hover","press");
    ui->voice_chat->SetState("normal","hover","press");
    
    // 连接视频通话管理器信号
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigIncomingCall,
            this, &FriendInfoPage::onIncomingCall);
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigCallAccepted,
            this, &FriendInfoPage::onCallAccepted);
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigCallRejected,
            this, &FriendInfoPage::onCallRejected);
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigCallHangup,
            this, &FriendInfoPage::onCallHangup);
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigCallStateChanged,
            this, &FriendInfoPage::onCallStateChanged);
}

FriendInfoPage::~FriendInfoPage()
{
    delete ui;
}

void FriendInfoPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
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
        auto file_name = QFileInfo(_user_info->_icon).fileName();

        // 确保目录存在
        if (avatarsDir.exists()) {
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
            LoadHeadIcon(avatarPath, ui->icon_lb, file_name, "other_icon");
        }
    }


    ui->name_lb->setText(user_info->_name);
    ui->nick_lb->setText(user_info->_nick);
    ui->bak_lb->setText(user_info->_nick);
}

void FriendInfoPage::LoadHeadIcon(QString avatarPath, QLabel *icon_label, QString file_name, QString req_type)
{
    UserMgr::GetInstance()->AddLabelToReset(avatarPath, icon_label);
    //先加载默认的
    QPixmap pixmap(":/images/head_default.png");
    QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(),
                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
    icon_label->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
    icon_label->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

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

void FriendInfoPage::on_video_chat_clicked()
{
    qDebug() << "[FriendInfoPage]: --- video chat btn clicked ---";
    
    if (!_user_info) {
        qDebug() << "[FriendInfoPage]: User info is null, cannot start video call";
        return;
    }

    if (video_window) {
        video_window->raise();
        video_window->activateWindow();
        return;
    }
    
    // 设置好友信息到视频通话管理器
    VideoCallManager::GetInstance()->setFriendInfo(_user_info);
    
    // 启动视频通话
    VideoCallManager::GetInstance()->startCall(_user_info->_uid);
    
    // 也可以显示一个等待界面
    video_window = new VideoCallWindow();
    video_window->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(video_window, &QObject::destroyed, this, [this]() { video_window = nullptr; });
    video_window->setUserInfo(_user_info->_nick, _user_info->_uid);
    video_window->show();
}

// 视频通话管理器信号处理槽函数
void FriendInfoPage::onIncomingCall(int caller_uid, const QString& call_id, const QString& caller_name)
{
    qDebug() << "[FriendInfoPage]: Incoming call from" << caller_uid << caller_name;
    
    // 在这里可以显示来电弹窗
    // 通常在主窗口或聊天窗口中处理来电，而不是在好友信息页
    // 可以通过信号将事件传递给主窗口处理
    if (video_window) {
        video_window->raise();
        video_window->activateWindow();
    }
}

void FriendInfoPage::onCallAccepted(const QString& call_id, const QString& room_id, const QString& turn_ws_url, const QJsonArray& ice_servers)
{
    qDebug() << "[FriendInfoPage]: Call accepted, room_id:" << room_id;
    
    // 显示视频通话窗口
    if (!video_window) {
        video_window = new VideoCallWindow();
        video_window->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(video_window, &QObject::destroyed, this, [this]() { video_window = nullptr; });
        video_window->show();
    }
    
    video_window->raise();
    video_window->activateWindow();
}

void FriendInfoPage::onCallRejected(const QString& call_id, const QString& reason)
{
    qDebug() << "[FriendInfoPage]: Call rejected, reason:" << reason;
    // 可以显示通知给用户
    //使用静态方法直接弹出一个信息框
    QMessageBox::information(this,"视频通话","对方拒绝接听！");
    // 通话被拒绝，关闭视频窗口
    if (video_window) {
        video_window->close();
        video_window = nullptr;
    }
}

void FriendInfoPage::onCallHangup(const QString& call_id)
{
    qDebug() << "[FriendInfoPage]: Call hangup";
    // 通话结束，可以关闭视频窗口
    if (video_window) {
        video_window->close();
        video_window = nullptr;
    }
}

void FriendInfoPage::onCallStateChanged(VideoCallState new_state)
{
    qDebug() << "[FriendInfoPage]: Call state changed to:" << static_cast<int>(new_state);
    // 根据状态更新UI
    switch (new_state) {
        case VideoCallState::Idle:
            break;
        case VideoCallState::Ringing:
            break;
        case VideoCallState::Connecting:
            break;
        case VideoCallState::InCall:
            break;
    }
}
