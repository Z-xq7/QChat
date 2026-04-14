#include "chatuserwid.h"
#include "ui_chatuserwid.h"
#include "usermgr.h"
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDateTime>
#include "filetcpmgr.h"

ChatUserWid::ChatUserWid(QWidget *parent) :
    ListItemBase(parent),
    ui(new Ui::ChatUserWid)
{
    ui->setupUi(this);
    SetItemType(ListItemType::CHAT_USER_ITEM);
    ui->unread_badge->hide();
    // 强制设置时间标签及其父容器的宽度，确保日期能完整显示
    ui->user_info_wid->setMaximumWidth(105);
    ui->user_info_wid->setMinimumWidth(105);
    ui->time_lb->setMinimumWidth(55);
    ui->time_lb->setMaximumWidth(55);
    ui->time_wid->setMinimumWidth(60);
    ui->time_wid->setMaximumWidth(60);
}

ChatUserWid::~ChatUserWid()
{
    delete ui;
}

void ChatUserWid::SetChatData(std::shared_ptr<ChatThreadData> chat_data) {
    _chat_data = chat_data;
    auto other_id = _chat_data->GetOtherId();

    // 设置最后消息时间
    SetLastMsgTime(chat_data->GetLastMsgTime());

    //处理群聊
    if (_chat_data->IsGroup()) {
        //群聊头像使用默认群聊图标
        QPixmap pixmap(":/images/group.png");
        if (pixmap.isNull()) {
            //如果群聊图标不存在，使用默认头像
            pixmap = QPixmap(":/images/head_default.png");
        }
        QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->icon_lb->setPixmap(scaledPixmap);
        ui->icon_lb->setScaledContents(true);
        ui->user_name_lb->setText(_chat_data->GetGroupName().isEmpty() ? "群聊" : _chat_data->GetGroupName());
        ui->user_chat_lb->setText(chat_data->GetLastMsg().isEmpty() ? "" : chat_data->GetLastMsg());
        return;
    }

    if (other_id == 0) {
        // AI 助手特殊处理
        QPixmap pixmap(":/images/doubao.png");
        QPixmap scaledPixmap = pixmap.scaled(ui->icon_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->icon_lb->setPixmap(scaledPixmap);
        ui->icon_lb->setScaledContents(true);
        ui->user_name_lb->setText("豆包 AI 助手");
        ui->user_chat_lb->setText(chat_data->GetLastMsg().isEmpty() ? "点击开始对话" : chat_data->GetLastMsg());
        return;
    }

    auto other_info = UserMgr::GetInstance()->GetFriendById(other_id);
    if (other_info == nullptr) {
        qWarning() << "[ChatUserWid]: other_info is nullptr, other_id=" << other_id;
        return;
    }

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
        auto other_uid = other_info->_uid;
        QDir avatarsDir(storageDir + "/user/" + QString::number(other_uid) + "/avatars");

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
                qWarning() << "[ChatUserWid]: 无法加载上传的头像：" << avatarPath;
                LoadHeadIcon(avatarPath, ui->icon_lb, file_name,"other_icon");
            }
        }
        else {
            qWarning() << "[ChatUserWid]: 头像存储目录不存在：" << avatarsDir.path();
            QString avatarPath = avatarsDir.filePath(QFileInfo(other_info->_icon).fileName());
            avatarsDir.mkpath(".");
            LoadHeadIcon(avatarPath, ui->icon_lb, file_name, "other_icon");
        }
    }

    ui->user_name_lb->setText(other_info->_name);
    ui->user_chat_lb->setText(chat_data->GetLastMsg());
}

std::shared_ptr<ChatThreadData> ChatUserWid::GetChatData()
{
    return _chat_data;
}

void ChatUserWid::UpdateUnreadCount(int count)
{
    if (count <= 0) {
        ui->unread_badge->hide();
        return;
    }
    // 超过99显示99+
    QString text = count > 99 ? "99+" : QString::number(count);
    ui->unread_badge->setText(text);
    ui->unread_badge->show();
    ui->unread_badge->raise();
}

void ChatUserWid::UpdateLastMsg(const QString& msg)
{
    if (!msg.isEmpty()) {
        _chat_data->SetLastMsg(msg);
    }
    ui->user_chat_lb->setText(_chat_data->GetLastMsg());
}

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

void ChatUserWid::LoadHeadIcon(QString avatarPath, QLabel* icon_label, QString file_name,QString req_type) {
    UserMgr::GetInstance()->AddLabelToReset(avatarPath, icon_label);
    //先加载默认的
    QPixmap pixmap(":/images/head_default.png");
    QPixmap scaledPixmap = pixmap.scaled(icon_label->size(),
                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
    icon_label->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
    icon_label->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

    //判断是否正在下载
    bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
    if (is_loading) {
        qWarning() << "[ChatUserWid]: 正在下载: " << file_name;
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

void ChatUserWid::SetLastMsgTime(const QString& timeStr)
{
    if (timeStr.isEmpty()) {
        ui->time_lb->setText("");
        return;
    }

    // 解析服务器返回的时间字符串 (格式: "2026-04-12 21:15:30")
    QDateTime msgDateTime = QDateTime::fromString(timeStr, "yyyy-MM-dd hh:mm:ss");
    if (!msgDateTime.isValid()) {
        // 尝试其他可能的格式
        msgDateTime = QDateTime::fromString(timeStr, "yyyy-MM-dd HH:mm:ss");
    }
    if (!msgDateTime.isValid()) {
        ui->time_lb->setText(timeStr);
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    QDate msgDate = msgDateTime.date();
    QDate today = now.date();
    QDate yesterday = today.addDays(-1);

    QString displayText;
    if (msgDate == today) {
        // 今天：显示 HH:mm
        displayText = msgDateTime.toString("hh:mm");
    } else if (msgDate == yesterday) {
        // 昨天：显示"昨天"
        displayText = "昨天";
    } else if (msgDate.daysTo(today) < 7) {
        // 一周内：显示周几
        QStringList weekDays = {"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
        int dayOfWeek = msgDate.dayOfWeek(); // 1=周一, 7=周日
        displayText = weekDays[dayOfWeek - 1];
    } else {
        // 更早：显示月日 (如"5月12日")
        displayText = msgDate.toString("M月d日");
    }

    ui->time_lb->setText(displayText);
}
