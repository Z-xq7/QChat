#include "chatuserwid.h"
#include "ui_chatuserwid.h"
#include "usermgr.h"
#include <QRegularExpression>
#include <QStandardPaths>
#include "filetcpmgr.h"

ChatUserWid::ChatUserWid(QWidget *parent) :
    ListItemBase(parent),
    ui(new Ui::ChatUserWid)
{
    ui->setupUi(this);
    SetItemType(ListItemType::CHAT_USER_ITEM);
}

ChatUserWid::~ChatUserWid()
{
    delete ui;
}

void ChatUserWid::SetChatData(std::shared_ptr<ChatThreadData> chat_data) {
    _chat_data = chat_data;
    auto other_id = _chat_data->GetOtherId();
    auto other_info = UserMgr::GetInstance()->GetFriendById(other_id);
    // 加载头像图片
    QString head_icon = UserMgr::GetInstance()->GetIcon();

    // 使用正则表达式检查是否是默认头像
    QRegularExpression regex("^:/images/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(other_info->_icon);

    if (match.hasMatch()) {
        // 如果是默认头像（:/images/head_X.jpg 格式）
        QPixmap pixmap(other_info->_icon); // 加载默认头像图片
        QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->icon_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
        ui->icon_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
    }
    else {
        // 如果是用户上传的头像，获取存储目录
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir avatarsDir(storageDir + "/avatars");
        auto file_name = QFileInfo(other_info->_icon).fileName();

        // 确保目录存在
        if (avatarsDir.exists()) {
            QString avatarPath = avatarsDir.filePath(file_name); // 获取上传头像的完整路径
            QPixmap pixmap(avatarPath); // 加载上传的头像图片
            if (!pixmap.isNull()) {
                QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                ui->icon_lb->setPixmap(scaledPixmap);
                ui->icon_lb->setScaledContents(true);
            }
            else {
                qWarning() << "无法加载上传的头像：" << avatarPath;
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
                    qWarning() << "正在下载: " << file_name;
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
                    FileTcpMgr::GetInstance()->SendDownloadInfo(download_info);
                }
            }
        }
        else {
            qWarning() << "头像存储目录不存在：" << avatarsDir.path();
        }
    }

    ui->user_name_lb->setText(other_info->_name);
    ui->user_chat_lb->setText(chat_data->GetLastMsg());
}

std::shared_ptr<ChatThreadData> ChatUserWid::GetChatData()
{
    return _chat_data;
}

void ChatUserWid::ShowRedPoint(bool bshow)
{
    // if(bshow){
    //     ui->red_point->show();
    // }else{
    //     ui->red_point->hide();
    // }
}



// void ChatUserWid::SetInfo(std::shared_ptr<UserInfo> user_info)
// {
//     _user_info = user_info;
//     // 加载图片
//     QPixmap pixmap(_user_info->_icon);

//     // 设置图片自动缩放
//     ui->icon_lb->setPixmap(pixmap.scaled(ui->icon_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
//     ui->icon_lb->setScaledContents(true);

//     ui->user_name_lb->setText(_user_info->_name);
//     ui->user_chat_lb->setText(_user_info->_last_msg);
// }

// void ChatUserWid::SetInfo(std::shared_ptr<FriendInfo> friend_info)
// {
//     _user_info = std::make_shared<UserInfo>(friend_info);
//     // 加载图片
//     QPixmap pixmap(_user_info->_icon);

//     // 设置图片自动缩放
//     ui->icon_lb->setPixmap(pixmap.scaled(ui->icon_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
//     ui->icon_lb->setScaledContents(true);

//     ui->user_name_lb->setText(_user_info->_name);
//     ui->user_chat_lb->setText(_user_info->_last_msg);
// }

// std::shared_ptr<UserInfo> ChatUserWid::GetUserInfo()
// {
//     return _user_info;
// }

void ChatUserWid::updateLastMsg(std::vector<std::shared_ptr<TextChatData> > msgs)
{
    int last_msg_id = 0;
    QString last_msg = "";
    for (auto& msg : msgs) {
        last_msg = msg->GetContent();
        last_msg_id = msg->GetMsgId();
        _chat_data->AddMsg(msg);
    }

    _chat_data->SetLastMsgId(last_msg_id);
    ui->user_chat_lb->setText(last_msg);
}

