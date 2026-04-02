#include "chatpage.h"
#include "ui_chatpage.h"
#include <QStyleOption>
#include <QPainter>
#include "chatitembase.h"
#include "textbubble.h"
#include "picturebubble.h"
#include "imageviewerdialog.h"
#include "emojipopup.h"
#include "usermgr.h"
#include <QUuid>
#include "tcpmgr.h"
#include <algorithm>
#include <QStandardPaths>
#include "filetcpmgr.h"
#include <memory>
#include <QGuiApplication>
#include <QScreen>
#include <QLineEdit>

#include "filebubble.h"
#include "fileconfirmdlg.h"
#include <QFileDialog>
#include <QStandardPaths>

ChatPage::ChatPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatPage)
    , _emoji_popup(nullptr)
{
    ui->setupUi(this);
    //设置按钮样式
    ui->receive_btn->SetState("normal","hover","press");
    ui->send_btn->SetState("normal","hover","press");
    //设置图标样式
    ui->emo_lb->SetState("normal","hover","press","normal","hover","press");
    ui->file_lb->SetState("normal","hover","press","normal","hover","press");
    connect(ui->emo_lb, &ClickedLabel::clicked, this, &ChatPage::on_emo_clicked);
    connect(ui->file_lb, &ClickedLabel::clicked, this, &ChatPage::on_file_clicked);
    // 连接搜索聊天记录
    connect(ui->chat_search_edit, &QLineEdit::textChanged, this, &ChatPage::slot_chat_search);
}

ChatPage::~ChatPage()
{
    delete ui;
}

void ChatPage::SetChatData(std::shared_ptr<ChatThreadData> chat_data) {
    _chat_data = chat_data;
    auto other_id = _chat_data->GetOtherId();
    if(other_id == 0) {
        //说明是群聊
        ui->title_lb->setText(_chat_data->GetGroupName());
        //todo...加载群聊信息和成员信息
        return;
    }

    //私聊
    auto friend_info = UserMgr::GetInstance()->GetFriendById(other_id);
    if (friend_info == nullptr) {
        return;
    }
    ui->title_lb->setText(friend_info->_name);
    ui->chat_data_list->removeAllItem();
    _unrsp_item_map.clear();
    _base_item_map.clear();
    
    // （已修改）加载缓存的聊天信息 - 先将消息按 msg_id 排序
    auto msg_map = chat_data->GetMsgMapRef();
    
    // 创建一个向量来存储消息，并按 msg_id 排序
    std::vector<std::shared_ptr<ChatDataBase>> sorted_msgs;
    for(auto& msg : msg_map) {
        sorted_msgs.push_back(msg);
    }
    
    // 按 msg_id 排序
    std::sort(sorted_msgs.begin(), sorted_msgs.end(), 
              [](const std::shared_ptr<ChatDataBase>& a, const std::shared_ptr<ChatDataBase>& b) {
                  return a->GetMsgId() < b->GetMsgId();
              });
    
    // 按排序后的顺序添加消息
    for(auto& msg : sorted_msgs) {
        AppendChatMsg(msg, true);
    }

    // 添加未响应的消息（这些通常是用户自己发送的未确认消息）
    for(auto& msg : chat_data->GetMsgUnRspRef()){
        AppendChatMsg(msg, false);
    }
}

void ChatPage::LoadHeadIcon(QString avatarPath, QLabel *icon_label, QString file_name, QString req_type)
{
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
        FileTcpMgr::GetInstance()->SendDownloadInfo(download_info, req_type);
    }
}

void ChatPage::SetChatIcon(ChatItemBase *pChatItem, int uid, QString icon, QString req_type)
{
    // 先尝试从缓存获取
    QPixmap cachedPixmap = UserMgr::GetInstance()->GetCachedIcon(icon);
    if (!cachedPixmap.isNull()) {
        pChatItem->setUserIcon(cachedPixmap);
        return;
    }

    // 使用正则表达式检查是否是默认头像
    QRegularExpression regex("^:/images/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(icon);
    if (match.hasMatch()) {
        QPixmap pixmap(icon);
        UserMgr::GetInstance()->CacheIcon(icon, pixmap);
        pChatItem->setUserIcon(pixmap);
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
                UserMgr::GetInstance()->CacheIcon(icon, pixmap);
                pChatItem->setUserIcon(pixmap);
            }
            else {
                qWarning() << "无法加载上传的头像：" << avatarPath;
                auto icon_label = pChatItem->getIconLabel();

                LoadHeadIcon(avatarPath, icon_label, file_name, req_type);
            }
        }
        else {
            qWarning() << "头像存储目录不存在：" << avatarsDir.path();
            //创建目录
            avatarsDir.mkpath(".");
            auto icon_label = pChatItem->getIconLabel();
            QString avatarPath = avatarsDir.filePath(file_name);

            LoadHeadIcon(avatarPath, icon_label, file_name, req_type);
        }
    }
}

void ChatPage::AppendChatMsg(std::shared_ptr<ChatDataBase> msg, bool rsp)
{
    auto self_info = UserMgr::GetInstance()->GetUserInfo();
    ChatRole role;
    //todo... 添加聊天显示
    if (msg->GetSendUid() == self_info->_uid) {
        role = ChatRole::Self;
        ChatItemBase* pChatItem = new ChatItemBase(role);

        pChatItem->setUserName(self_info->_name);

        SetChatIcon(pChatItem, self_info->_uid, self_info->_icon, "self_icon");

        QWidget* pBubble = nullptr;
        if (msg->GetMsgType() == ChatMsgType::TEXT) {
            pBubble = new TextBubble(role, msg->GetMsgContent());
        }
        else if (msg->GetMsgType() == ChatMsgType::PIC) {
            auto img_msg = std::dynamic_pointer_cast<ImgChatData>(msg);
            auto pic_bubble =  new PictureBubble(img_msg->_msg_info->_preview_pix, role, img_msg->_msg_info->_total_size);
            pic_bubble->setMsgInfo(img_msg->_msg_info);
            pBubble = pic_bubble;

            //连接暂停和恢复信号
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::viewRequested,
                    this, &ChatPage::on_view_picture);
        }
        else if (msg->GetMsgType() == ChatMsgType::FILE) {
            auto file_msg = std::dynamic_pointer_cast<FileChatData>(msg);
            auto file_bubble = new FileBubble(role, file_msg->_msg_info->_text_or_url, file_msg->_msg_info->_total_size);
            file_bubble->setMsgInfo(file_msg->_msg_info);
            pBubble = file_bubble;

            connect(dynamic_cast<FileBubble*>(pBubble), &FileBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<FileBubble*>(pBubble), &FileBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
        }

        // 连接删除消息信号
        connect(dynamic_cast<BubbleFrame*>(pBubble), &BubbleFrame::sig_delete_msg, this, [this, pChatItem, msg]() {
            ui->chat_data_list->layout()->removeWidget(pChatItem);
            pChatItem->deleteLater();
            // 同时从 map 中移除
            _base_item_map.remove(msg->GetMsgId());
            _unrsp_item_map.remove(msg->GetUniqueId());
        });

        pChatItem->setWidget(pBubble);
        auto status = msg->GetStatus();
        pChatItem->setStatus(status);
        ui->chat_data_list->appendChatItem(pChatItem);
        if (rsp) {
            _base_item_map[msg->GetMsgId()] = pChatItem;
        }
        else {
            _unrsp_item_map[msg->GetUniqueId()] = pChatItem;
        }
    }
    else {
        role = ChatRole::Other;
        ChatItemBase* pChatItem = new ChatItemBase(role);
        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->GetSendUid());
        if (friend_info == nullptr) {
            return;
        }

        pChatItem->setUserName(friend_info->_name);

        SetChatIcon(pChatItem, friend_info->_uid, friend_info->_icon, "other_icon");

        QWidget* pBubble = nullptr;
        if (msg->GetMsgType() == ChatMsgType::TEXT) {
            pBubble = new TextBubble(role, msg->GetMsgContent());
        }
        else if(msg->GetMsgType() == ChatMsgType::PIC) {
            auto img_msg = std::dynamic_pointer_cast<ImgChatData>(msg);
            auto pic_bubble = new PictureBubble(img_msg->_msg_info->_preview_pix, role, img_msg->_msg_info->_total_size);
            pic_bubble->setMsgInfo(img_msg->_msg_info);
            pBubble = pic_bubble;

            //连接暂停和恢复信号
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::viewRequested,
                    this, &ChatPage::on_view_picture);
        }
        else if(msg->GetMsgType() == ChatMsgType::FILE) {
            auto file_msg = std::dynamic_pointer_cast<FileChatData>(msg);
            auto file_bubble = new FileBubble(role, file_msg->_msg_info->_text_or_url, file_msg->_msg_info->_total_size);
            file_bubble->setMsgInfo(file_msg->_msg_info);
            pBubble = file_bubble;

            connect(dynamic_cast<FileBubble*>(pBubble), &FileBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<FileBubble*>(pBubble), &FileBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
        }

        // 连接删除消息信号
        connect(dynamic_cast<BubbleFrame*>(pBubble), &BubbleFrame::sig_delete_msg, this, [this, pChatItem, msg]() {
            ui->chat_data_list->layout()->removeWidget(pChatItem);
            pChatItem->deleteLater();
            _base_item_map.remove(msg->GetMsgId());
            _unrsp_item_map.remove(msg->GetUniqueId());
        });

        pChatItem->setWidget(pBubble);
        auto status = msg->GetStatus();
        pChatItem->setStatus(status);
        ui->chat_data_list->appendChatItem(pChatItem);
        if (rsp) {
            _base_item_map[msg->GetMsgId()] = pChatItem;
        }
        else {
            _unrsp_item_map[msg->GetUniqueId()] = pChatItem;
        }
    }
}

void ChatPage::AppendOtherMsg(std::shared_ptr<ChatDataBase> msg) {
    auto self_info = UserMgr::GetInstance()->GetUserInfo();
    ChatRole role;
    if (msg->GetSendUid() == self_info->_uid) {
        role = ChatRole::Self;
        ChatItemBase* pChatItem = new ChatItemBase(role);

        pChatItem->setUserName(self_info->_name);
        //设置头像
        SetChatIcon(pChatItem, self_info->_uid, self_info->_icon, "self_icon");

        QWidget* pBubble = nullptr;
        if (msg->GetMsgType() == ChatMsgType::TEXT) {
            pBubble = new TextBubble(role, msg->GetMsgContent());
        }
        else if (msg->GetMsgType() == ChatMsgType::PIC) {
            auto img_msg = std::dynamic_pointer_cast<ImgChatData>(msg);
            auto pic_bubble = new PictureBubble(img_msg->_msg_info->_preview_pix, role, img_msg->_msg_info->_total_size);
            pic_bubble->setMsgInfo(img_msg->_msg_info);
            pBubble = pic_bubble;
            //连接暂停和恢复信号
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::viewRequested,
                    this, &ChatPage::on_view_picture);
        }
        else if (msg->GetMsgType() == ChatMsgType::FILE) {
            auto file_msg = std::dynamic_pointer_cast<FileChatData>(msg);
            auto file_bubble = new FileBubble(role, file_msg->_msg_info->_text_or_url, file_msg->_msg_info->_total_size);
            file_bubble->setMsgInfo(file_msg->_msg_info);
            pBubble = file_bubble;
            //连接暂停和恢复信号
            connect(dynamic_cast<FileBubble*>(pBubble), &FileBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<FileBubble*>(pBubble), &FileBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
        }

        pChatItem->setWidget(pBubble);
        auto status = msg->GetStatus();
        pChatItem->setStatus(status);
        ui->chat_data_list->appendChatItem(pChatItem);
        _base_item_map[msg->GetMsgId()] = pChatItem;
    }
    else {
        role = ChatRole::Other;
        ChatItemBase* pChatItem = new ChatItemBase(role);
        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->GetSendUid());
        if (friend_info == nullptr) {
            return;
        }
        pChatItem->setUserName(friend_info->_name);

        //设置头像
        SetChatIcon(pChatItem, friend_info->_uid, friend_info->_icon, "other_icon");

        QWidget* pBubble = nullptr;
        if (msg->GetMsgType() == ChatMsgType::TEXT) {
            pBubble = new TextBubble(role, msg->GetMsgContent());
        }
        else if (msg->GetMsgType() == ChatMsgType::PIC) {
            auto img_msg = std::dynamic_pointer_cast<ImgChatData>(msg);
            auto pic_bubble = new PictureBubble(img_msg->_msg_info->_preview_pix, role, img_msg->_msg_info->_total_size);
            pic_bubble->setMsgInfo(img_msg->_msg_info);
            pBubble = pic_bubble;
            //连接暂停和恢复信号
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::viewRequested,
                    this, &ChatPage::on_view_picture);
        }
        else if (msg->GetMsgType() == ChatMsgType::FILE) {
            auto file_msg = std::dynamic_pointer_cast<FileChatData>(msg);
            auto file_bubble = new FileBubble(role, file_msg->_msg_info->_text_or_url, file_msg->_msg_info->_total_size);
            file_bubble->setMsgInfo(file_msg->_msg_info);
            pBubble = file_bubble;
            //连接暂停和恢复信号
            connect(dynamic_cast<FileBubble*>(pBubble), &FileBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<FileBubble*>(pBubble), &FileBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
        }
        pChatItem->setWidget(pBubble);
        auto status = msg->GetStatus();
        pChatItem->setStatus(status);
        ui->chat_data_list->appendChatItem(pChatItem);
        _base_item_map[msg->GetMsgId()] = pChatItem;
    }
}

void ChatPage::clearItems()
{
    ui->chat_data_list->removeAllItem();
    _unrsp_item_map.clear();
    _base_item_map.clear();
}

void ChatPage::UpdateChatStatus(std::shared_ptr<ChatDataBase> msg)
{
    auto iter = _unrsp_item_map.find(msg->GetUniqueId());
    //没找到则直接返回
    if (iter == _unrsp_item_map.end()) {
        return;
    }

    iter.value()->setStatus(msg->GetStatus());
    _base_item_map[msg->GetMsgId()] = iter.value();
    _unrsp_item_map.erase(iter);
}

void ChatPage::UpdateImgChatStatus(std::shared_ptr<ImgChatData> msg) {
    auto iter = _unrsp_item_map.find(msg->GetUniqueId());
    //没找到则直接返回
    if (iter == _unrsp_item_map.end()) {
        return;
    }

    iter.value()->setStatus(msg->GetStatus());
    _base_item_map[msg->GetMsgId()] = iter.value();
    _unrsp_item_map.erase(iter);

    auto bubble = _base_item_map[msg->GetMsgId()]->getBubble();
    PictureBubble* pic_bubble = dynamic_cast<PictureBubble*>(bubble);
    pic_bubble->setMsgInfo(msg->_msg_info);
}

void ChatPage::UpdateFileProgress(std::shared_ptr<MsgInfo> msg_info) {
    auto iter = _base_item_map.find(msg_info->_msg_id);
    if (iter == _base_item_map.end()) {
        return;
    }

    if (msg_info->_msg_type == MsgType::IMG_MSG) {
        auto bubble = iter.value()->getBubble();
        PictureBubble*  pic_bubble = dynamic_cast<PictureBubble*>(bubble);
        pic_bubble->setProgress(msg_info->_rsp_size, msg_info->_total_size);
    }
    else if (msg_info->_msg_type == MsgType::FILE_MSG) {
        auto bubble = iter.value()->getBubble();
        FileBubble* file_bubble = dynamic_cast<FileBubble*>(bubble);
        file_bubble->setProgress(msg_info->_rsp_size, msg_info->_total_size);
    }
}

void ChatPage::DownloadFileFinished(std::shared_ptr<MsgInfo> msg_info, QString file_path)
{
    auto iter = _base_item_map.find(msg_info->_msg_id);
    if (iter == _base_item_map.end()) {
        return;
    }

    if (msg_info->_msg_type == MsgType::IMG_MSG) {
        auto bubble = iter.value()->getBubble();
        PictureBubble* pic_bubble = dynamic_cast<PictureBubble*>(bubble);
        pic_bubble->setDownloadFinish(msg_info, file_path);

        auto chat_data_base = _chat_data->GetChatDataBase(msg_info->_msg_id);
        if (chat_data_base == nullptr) {
            return;
        }
        auto img_data = std::dynamic_pointer_cast<ImgChatData>(chat_data_base);
        img_data->_msg_info->_preview_pix =  QPixmap(file_path);
        img_data->_msg_info->_transfer_state = TransferState::Completed;
        img_data->_msg_info->_current_size = img_data->_msg_info->_total_size;
    }
    else if (msg_info->_msg_type == MsgType::FILE_MSG) {
        auto bubble = iter.value()->getBubble();
        FileBubble* file_bubble = dynamic_cast<FileBubble*>(bubble);
        file_bubble->setDownloadFinish(msg_info, file_path);
    }
}

void ChatPage::FileTransferFailed(std::shared_ptr<MsgInfo> msg_info)
{
    auto iter = _base_item_map.find(msg_info->_msg_id);
    if (iter == _base_item_map.end()) {
        return;
    }

    auto bubble = iter.value()->getBubble();
    if (msg_info->_msg_type == MsgType::IMG_MSG) {
        PictureBubble* pic_bubble = dynamic_cast<PictureBubble*>(bubble);
        // pic_bubble->setFailed(); 
    }
    else if (msg_info->_msg_type == MsgType::FILE_MSG) {
        FileBubble* file_bubble = dynamic_cast<FileBubble*>(bubble);
        file_bubble->setFailed();
    }
}

//让ChatPage（继承自QWidget的自定义控件）具备样式表渲染能力。
//因为纯QWidget的默认paintEvent是空实现，无法识别样式表（比如你设置的背景色、边框、圆角等），
//而这段代码通过调用 Qt 的样式系统绘制控件的基础外观，从而让样式表生效。
void ChatPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ChatPage::on_send_btn_clicked()
{
    //如果当前用户信息为空，则返回
    if(_chat_data == nullptr){
        qDebug() << "[ChatPage]: *** Friend_info is empty ***";
        return;
    }

    //获取自身的信息
    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Self;
    QString userName = user_info->_name;
    QString userIcon = user_info->_icon;
    //获取聊天会话id
    auto thread_id = _chat_data->GetThreadId();

    //准备信息列表
    const QVector<std::shared_ptr<MsgInfo>>& msgList = pTextEdit->getMsgList();
    QJsonObject textObj;
    QJsonArray textArray;
    int txt_size = 0;

    for(int i = 0; i < msgList.size(); ++i)
    {
        //文本消息内容长度不合规就跳过
        if (msgList[i]->_text_or_url.length() > 1024) {
            continue;
        }

        //取出文本类型
        MsgType type = msgList[i]->_msg_type;

        //构造消息item
        ChatItemBase* pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);

        SetChatIcon(pChatItem, user_info->_uid, user_info->_icon, "self_icon");

        QWidget* pBubble = nullptr;

        //生成唯一id
        QUuid uuid = QUuid::createUuid();
        //转为字符串
        QString uuidString = uuid.toString();

        if (type == MsgType::TEXT_MSG)
        {
            pBubble = new TextBubble(role, msgList[i]->_text_or_url);
            //累计的文本大小大于1024，则发送给服务器
            if (txt_size + msgList[i]->_text_or_url.length() > 1024) {
                qDebug() << "[ChatPage]: --- Send to ChatServer: textArray is " << textArray << " ---";
                textObj["fromuid"] = user_info->_uid;
                textObj["touid"] = _chat_data->GetOtherId();
                textObj["thread_id"] = thread_id;
                textObj["text_array"] = textArray;
                QJsonDocument doc(textObj);
                QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

                //发送并清空之前累计的文本列表
                txt_size = 0;
                textArray = QJsonArray();
                textObj = QJsonObject();

                //发送tcp请求给chat server
                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
            }

            //将bubble和uid绑定，以后可以等网络返回消息后设置是否送达
            //_bubble_map[uuidString] = pBubble;
            txt_size += msgList[i]->_text_or_url.length();
            QJsonObject obj;
            QByteArray utf8Message = msgList[i]->_text_or_url.toUtf8();
            auto content = QString::fromUtf8(utf8Message);
            obj["content"] = content;
            obj["unique_id"] = uuidString;
            textArray.append(obj);

            //todo... 注意，此处先按私聊处理
            auto txt_msg = std::make_shared<TextChatData>(uuidString, thread_id, ChatFormType::PRIVATE,
                ChatMsgType::TEXT, content, user_info->_uid, 0);
            //将未回复的消息加入到未回复列表中，以便后续处理
            _chat_data->AppendUnRspMsg(uuidString, txt_msg);
        }
        else if (type == MsgType::IMG_MSG)
        {
            //将之前缓存的文本发送过去
            if (txt_size) {
                textObj["fromuid"] = user_info->_uid;
                textObj["touid"] = _chat_data->GetOtherId();
                textObj["thread_id"] = thread_id;
                textObj["text_array"] = textArray;
                QJsonDocument doc(textObj);
                QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
                //发送并清空之前累计的文本列表
                txt_size = 0;
                textArray = QJsonArray();
                textObj = QJsonObject();
                //发送tcp请求给chat server
                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
            }

            auto pic_bubble = new PictureBubble(QPixmap(msgList[i]->_text_or_url), role, msgList[i]->_total_size);
            pic_bubble->setMsgInfo(msgList[i]);
            pBubble = pic_bubble;
            //需要组织成文件发送，具体参考头像上传
            auto img_msg = std::make_shared<ImgChatData>(msgList[i],uuidString, thread_id, ChatFormType::PRIVATE,
                    ChatMsgType::PIC, user_info->_uid, 0);
            //将未回复的消息加入到未回复列表中，以便后续处理
            _chat_data->AppendUnRspMsg(uuidString, img_msg);
            textObj["fromuid"] = user_info->_uid;
            textObj["touid"] = _chat_data->GetOtherId();
            textObj["thread_id"] = thread_id;
            textObj["md5"] = msgList[i]->_md5;
            textObj["name"] = msgList[i]->_unique_name;
            textObj["token"] = UserMgr::GetInstance()->GetToken();
            textObj["unique_id"] = uuidString;
            textObj["text_or_url"] = msgList[i]->_text_or_url;
            
            msgList[i]->_transfer_type = TransferType::Upload;
            msgList[i]->_transfer_state = TransferState::Uploading;
            msgList[i]->_thread_id = thread_id;

            //文件信息加入管理
            UserMgr::GetInstance()->AddTransFile(msgList[i]->_unique_name, msgList[i]);
            QJsonDocument doc(textObj);
            QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
            //发送tcp请求给chat server
            emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_IMG_CHAT_MSG_REQ, jsonData);

            //链接暂停信号
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            //链接恢复信号
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::viewRequested,
                    this, &ChatPage::on_view_picture);
        }
        else if(type == MsgType::FILE_MSG)
        {
            //将之前缓存的文本发送过去
            if (txt_size) {
                textObj["fromuid"] = user_info->_uid;
                textObj["touid"] = _chat_data->GetOtherId();
                textObj["thread_id"] = thread_id;
                textObj["text_array"] = textArray;
                QJsonDocument doc(textObj);
                QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
                txt_size = 0;
                textArray = QJsonArray();
                textObj = QJsonObject();
                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
            }

            auto file_bubble = new FileBubble(role, msgList[i]->_text_or_url, msgList[i]->_total_size);
            file_bubble->setMsgInfo(msgList[i]);
            pBubble = file_bubble;

            auto file_msg = std::make_shared<FileChatData>(msgList[i], uuidString, thread_id, ChatFormType::PRIVATE,
                    ChatMsgType::FILE, user_info->_uid, 0);
            _chat_data->AppendUnRspMsg(uuidString, file_msg);

            textObj["fromuid"] = user_info->_uid;
            textObj["touid"] = _chat_data->GetOtherId();
            textObj["thread_id"] = thread_id;
            textObj["md5"] = msgList[i]->_md5;
            textObj["origin_name"] = msgList[i]->_origin_name;
            textObj["name"] = msgList[i]->_unique_name;
            textObj["token"] = UserMgr::GetInstance()->GetToken();
            textObj["unique_id"] = uuidString;
            textObj["text_or_url"] = msgList[i]->_text_or_url;
            
            msgList[i]->_transfer_type = TransferType::Upload;
            msgList[i]->_transfer_state = TransferState::Uploading;
            msgList[i]->_thread_id = thread_id;

            UserMgr::GetInstance()->AddTransFile(msgList[i]->_unique_name, msgList[i]);
            QJsonDocument doc(textObj);
            QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
            emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_FILE_CHAT_MSG_REQ, jsonData);

            connect(dynamic_cast<FileBubble*>(pBubble), &FileBubble::pauseRequested,
                    this, &ChatPage::on_clicked_paused);
            connect(dynamic_cast<FileBubble*>(pBubble), &FileBubble::resumeRequested,
                    this, &ChatPage::on_clicked_resume);
        }
        //发送消息到聊天框
        if(pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            pChatItem->setStatus(0);
            ui->chat_data_list->appendChatItem(pChatItem);
            _unrsp_item_map[uuidString] = pChatItem;
        }
    }

    //最后不管消息大小有没有达到1024，都会发送给服务器
    if (txt_size > 0) {
        qDebug() << "[ChatPage]: --- Send to ChatServer: textArray is " << textArray << " ---";
        //发送给服务器
        textObj["text_array"] = textArray;
        textObj["fromuid"] = user_info->_uid;
        textObj["touid"] = _chat_data->GetOtherId();
        textObj["thread_id"] = thread_id;
        QJsonDocument doc(textObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

        //发送并清空之前累计的文本列表
        txt_size = 0;
        textArray = QJsonArray();
        textObj = QJsonObject();

        //发送tcp请求给chat server
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
    }
}


void ChatPage::on_receive_btn_clicked()
{
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Other;

    auto friend_info = UserMgr::GetInstance()->GetFriendById(_chat_data->GetOtherId());
    QString userName = friend_info->_name;
    QString userIcon = friend_info->_icon;

    const QVector<std::shared_ptr<MsgInfo>>& msgList = pTextEdit->getMsgList();
    for(int i=0; i<msgList.size(); ++i)
    {
        MsgType type = msgList[i]->_msg_type;
        ChatItemBase *pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        pChatItem->setUserIcon(QPixmap(userIcon));
        QWidget *pBubble = nullptr;
        if(type == MsgType::TEXT_MSG)
        {
            pBubble = new TextBubble(role, msgList[i]->_text_or_url);
        }
        else if(type == MsgType::IMG_MSG)
        {
            auto pic_bubble = new PictureBubble(QPixmap(msgList[i]->_text_or_url) , role, msgList[i]->_total_size);
            pic_bubble->setMsgInfo(msgList[i]);
            pBubble = pic_bubble;
            connect(dynamic_cast<PictureBubble*>(pBubble), &PictureBubble::viewRequested,
                    this, &ChatPage::on_view_picture);
        }
        else if(type == MsgType::FILE_MSG)
        {

        }
        if(pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            ui->chat_data_list->appendChatItem(pChatItem);
        }
    }
}

void ChatPage::on_clicked_paused(std::shared_ptr<MsgInfo> msg_info)
{
    UserMgr::GetInstance()->PauseTransFileByName(msg_info->_unique_name);
}

void ChatPage::on_clicked_resume(std::shared_ptr<MsgInfo> msg_info)
{
    // 确保在管理中
    UserMgr::GetInstance()->AddTransFile(msg_info->_unique_name, msg_info);
    
    UserMgr::GetInstance()->ResumeTransFileByName(msg_info->_unique_name);
    //继续发送或者下载
    if (msg_info->_transfer_type == TransferType::Upload) {
        FileTcpMgr::GetInstance()->ContinueUploadFile(msg_info->_unique_name);
        return;
    }

    if (msg_info->_transfer_type == TransferType::Download || msg_info->_transfer_type == TransferType::None) {
        // 如果是None，默认发起下载
        msg_info->_transfer_type = TransferType::Download;
        FileTcpMgr::GetInstance()->ContinueDownloadFile(msg_info->_unique_name);
        return;
    }
}

void ChatPage::on_view_picture(const QString& file_path, const QPixmap& preview_pix)
{
    if (file_path.isEmpty() && preview_pix.isNull()) {
        return;
    }
    auto viewer = new ImageViewerDialog(this->window());
    viewer->setAttribute(Qt::WA_DeleteOnClose, true);
    viewer->setImage(file_path, preview_pix);
    viewer->show();
    viewer->raise();
    viewer->activateWindow();
}

void ChatPage::on_file_clicked(QString, ClickLbState)
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择文件", QDir::homePath(), "All Files (*.*)");
    if (filePath.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(filePath);
    FileConfirmDlg* pDlg = new FileConfirmDlg(this);
    pDlg->setFileInfo(filePath, fileInfo.size());
    pDlg->setTargetName(ui->title_lb->text());

    if (pDlg->exec() == QDialog::Accepted) {
        ui->chatEdit->insertFile(filePath);
        // 自动触发发送
        on_send_btn_clicked();
    }
    delete pDlg;
}

void ChatPage::slot_chat_search(const QString &text)
{
    if (text.isEmpty()) {
        for (auto& item : _base_item_map) {
            item->show();
        }
        return;
    }

    if (_chat_data == nullptr) {
        return;
    }

    auto msg_map = _chat_data->GetMsgMapRef();
    for (auto iter = msg_map.begin(); iter != msg_map.end(); ++iter) {
        auto msg = iter.value();
        auto item = _base_item_map.value(msg->GetMsgId());
        if (!item) continue;

        if (msg->GetMsgType() == ChatMsgType::TEXT) {
            if (msg->GetMsgContent().contains(text, Qt::CaseInsensitive)) {
                item->show();
            } else {
                item->hide();
            }
        } else {
            // 目前只支持搜索文本消息，非文本消息隐藏
            item->hide();
        }
    }
}

void ChatPage::on_emo_clicked(QString, ClickLbState)
{
    if (_emoji_popup) {
        _emoji_popup->close();
        return;
    }

    _emoji_popup = new EmojiPopup(this->window());
    connect(_emoji_popup, &EmojiPopup::emojiSelected, this, &ChatPage::on_emoji_selected);
    connect(_emoji_popup, &QObject::destroyed, this, [this]() {
        _emoji_popup = nullptr;
        ui->emo_lb->SetCurState(ClickLbState::Normal);
    });

    ui->emo_lb->SetCurState(ClickLbState::Selected);

    const QPoint toolTopLeft = ui->tool_wid->mapToGlobal(QPoint(0, 0));
    const QPoint toolBottomLeft = ui->tool_wid->mapToGlobal(QPoint(0, ui->tool_wid->height()));
    QRect avail;
    if (auto screen = QGuiApplication::screenAt(toolTopLeft)) {
        avail = screen->availableGeometry();
    } else if (QGuiApplication::primaryScreen()) {
        avail = QGuiApplication::primaryScreen()->availableGeometry();
    }

    const int desiredWidth = ui->tool_wid->width();
    const int popupWidth = qMax(380, qMin(520, desiredWidth));
    const int popupHeight = 300;
    _emoji_popup->setFixedSize(popupWidth, popupHeight);

    int x = toolTopLeft.x();
    int y = toolTopLeft.y() - _emoji_popup->height() - 6;
    if (!avail.isNull()) {
        const int availRight = avail.x() + avail.width();
        const int availBottom = avail.y() + avail.height();
        if (x + _emoji_popup->width() > availRight) {
            x = availRight - _emoji_popup->width();
        }
        if (x < avail.left()) {
            x = avail.left();
        }
        if (y < avail.top()) {
            y = avail.top();
        }
        if (y + _emoji_popup->height() > availBottom) {
            y = toolBottomLeft.y() - _emoji_popup->height() - 6;
        }
    }

    _emoji_popup->move(x, y);
    _emoji_popup->show();
}

void ChatPage::on_emoji_selected(const QString& emoji)
{
    if (emoji.isEmpty()) return;
    ui->chatEdit->insertPlainText(emoji);
    ui->chatEdit->setFocus();
}
