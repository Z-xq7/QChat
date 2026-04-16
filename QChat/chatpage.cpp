#include "chatpage.h"
#include "ui_chatpage.h"
#include <QDateTime>
#include <QStyleOption>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSpacerItem>
#include <QDir>
#include <QRegularExpression>
#include "chatsidepage.h"
#include "groupsidepage.h"
#include "groupmemberlistpage.h"
#include "videocallmanager.h"
#include "videocallwindow.h"
#include "groupchatdialog.h"
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
#include "groupnoticedlg.h"
#include "filebubble.h"
#include "fileconfirmdlg.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <limits>
#include <QScrollBar>

ChatPage::ChatPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatPage)
    , _emoji_popup(nullptr)
    , m_sidePage(nullptr)
    , m_groupSidePage(nullptr)
    , m_memberListPage(nullptr)
    , m_sideAnim(nullptr)
    , m_currentSideType(0)
    , m_selfEditingNoticeThreadId(0)
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

    //右上角按钮样式设置
    ui->voice_chat_lb->SetState("normal", "hover", "press", "normal", "hover", "press");
    ui->video_chat_lb->SetState("normal", "hover", "press", "normal", "hover", "press");
    ui->screen_share_lb->SetState("normal", "hover", "press", "normal", "hover", "press");
    ui->remote_control_lb->SetState("normal", "hover", "press", "normal", "hover", "press");
    ui->group_chat_lb->SetState("normal", "hover", "press", "normal", "hover", "press");
    ui->more_lb->SetState("normal", "hover", "press", "normal", "hover", "press");

    //设置图片
    ui->voice_chat_lb->setProperty("state", "normal");
    ui->video_chat_lb->setProperty("state", "normal");
    ui->screen_share_lb->setProperty("state", "normal");
    ui->remote_control_lb->setProperty("state", "normal");
    ui->group_chat_lb->setProperty("state", "normal");
    ui->more_lb->setProperty("state", "normal");

    connect(ui->video_chat_lb, &ClickedLabel::clicked, this, &ChatPage::on_video_chat_clicked);
    connect(ui->group_chat_lb, &ClickedLabel::clicked, this, &ChatPage::on_group_chat_clicked);
    connect(ui->remote_control_lb, &ClickedLabel::clicked, this, &ChatPage::on_group_notice_clicked);

    // 设置标题栏样式
    ui->title_lb->setStyleSheet("QLabel { font-size: 16px; font-weight: bold; color: #333333; background-color: transparent; }");

    // 创建在线状态指示器（放在 title_lb 右边）
    m_onlineDot = new QLabel(this);
    m_onlineDot->setFixedSize(10, 10);
    m_onlineDot->setObjectName("chatpage_online_dot");
    m_onlineDot->hide();

    m_onlineText = new QLabel(this);
    m_onlineText->setObjectName("chatpage_online_text");
    m_onlineText->setStyleSheet("QLabel { font-size: 12px; color: #999999; background-color: transparent; }");
    m_onlineText->hide();

    // 在 widget_3 的 HBoxLayout 中，将在线指示器插入到 title_lb 之后、spacer 之前
    QHBoxLayout* titleHLayout = qobject_cast<QHBoxLayout*>(ui->widget_3->layout());
    if (titleHLayout) {
        int titleIndex = titleHLayout->indexOf(ui->title_lb);
        if (titleIndex >= 0) {
            // 在 title_lb 之后插入一个间距，再插入在线指示器
            titleHLayout->insertSpacerItem(titleIndex + 1, new QSpacerItem(8, 1, QSizePolicy::Fixed, QSizePolicy::Minimum));
            titleHLayout->insertWidget(titleIndex + 2, m_onlineDot);
            titleHLayout->insertWidget(titleIndex + 3, m_onlineText);
        }
    }

    // 连接好友状态变更信号
    connect(UserMgr::GetInstance().get(), &UserMgr::sig_friend_status_changed, this, &ChatPage::slot_friend_status_changed);

    // 连接批量状态更新信号，刷新当前聊天页面的在线状态
    connect(UserMgr::GetInstance().get(), &UserMgr::sig_friend_online_status_batch, this, [this](){
        slot_friend_status_changed(_chat_data ? _chat_data->GetOtherId() : 0, true);
    });

    // 连接滚动条信号：滚动到顶部时触发历史消息加载
    connect(ui->chat_data_list->scrollArea()->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
        // 滚动到顶端附近（距离 top 50px 以内）触发加载
        if (value <= ui->chat_data_list->scrollArea()->verticalScrollBar()->minimum() + 50) {
            slot_load_more_history();
        }
    });

    // 初始化侧边栏
    initSidePage();

    // 初始化侧边栏动画
    m_sideAnim = new QPropertyAnimation(ui->side_page_wid, "maximumWidth", this);
    m_sideAnim->setDuration(250);
    m_sideAnim->setEasingCurve(QEasingCurve::InOutQuad);

    connect(ui->more_lb, &ClickedLabel::clicked, this, &ChatPage::on_more_clicked);
}

void ChatPage::on_more_clicked()
{
    toggleSidePage();
}

void ChatPage::on_video_chat_clicked()
{
    qDebug() << "[ChatPage]: --- video chat btn clicked ---";

    auto other_id = _chat_data->GetOtherId();
    if(other_id == 0) {
        //说明是群聊
        ui->title_lb->setText(_chat_data->GetGroupName());
        //todo...加载群聊信息和成员信息
        return;
    }

    //私聊
    auto friend_info = UserMgr::GetInstance()->GetFriendById(other_id);

    if (!friend_info) {
        qDebug() << "[ChatPage]: User info is null, cannot start video call";
        return;
    }

    // 设置好友信息到视频通话管理器
    VideoCallManager::GetInstance()->setFriendInfo(friend_info);

    // 启动视频通话
    VideoCallManager::GetInstance()->startCall(friend_info->_uid);

    // 显示视频通话窗口
    VideoCallWindow* video_window = new VideoCallWindow();
    video_window->setAttribute(Qt::WA_DeleteOnClose, true);
    video_window->setUserInfo(friend_info->_nick, friend_info->_uid);
    video_window->show();
}

void ChatPage::on_group_chat_clicked()
{
    qDebug() << "[ChatPage]: --- group chat btn clicked ---";
    GroupChatDialog* group_dialog = new GroupChatDialog(this);
    group_dialog->setAttribute(Qt::WA_DeleteOnClose, true);

    //连接创建群聊信号到TcpMgr发送请求
    connect(group_dialog, &GroupChatDialog::sig_create_group_chat, this, [](const QString& group_name, const QVector<int>& member_uids){
        qDebug() << "[ChatPage]: --- sending create group chat request ---";

        QJsonObject jsonObj;
        jsonObj["creator_uid"] = UserMgr::GetInstance()->GetUid();
        jsonObj["group_name"] = group_name;

        QJsonArray memberArray;
        for (int uid : member_uids) {
            memberArray.append(uid);
        }
        jsonObj["member_uids"] = memberArray;

        QJsonDocument doc(jsonObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

        //发送tcp请求给chat server
        TcpMgr::GetInstance()->SendData(ReqId::ID_CREATE_GROUP_CHAT_REQ, jsonData);
    });

    group_dialog->show();
}

void ChatPage::slot_group_update_notice(int thread_id, const QString& notice)
{
    qDebug() << "[ChatPage]: --- handle group update notice, thread_id:" << thread_id << "notice:" << notice << " ---";
    
    // 如果是自己编辑的公告（刚发了更新请求），收到 RSP 后不弹窗，只更新数据
    bool isSelfEdit = (m_selfEditingNoticeThreadId == thread_id);
    if (isSelfEdit) {
        m_selfEditingNoticeThreadId = 0; // 重置标记
    }
    
    // 更新本地内存中的数据
    auto thread_data = UserMgr::GetInstance()->GetChatThreadByThreadId(thread_id);
    if (thread_data) {
        thread_data->SetNotice(notice);
        
        // 如果当前正在查看这个群，更新侧边栏显示
        if (_chat_data && _chat_data->GetThreadId() == thread_id) {
            m_groupSidePage->SetGroupData(_chat_data);
        }
    }
    
    // 自己编辑的公告不弹窗
    if (isSelfEdit) {
        return;
    }
    
    // 他人更新的公告：当前页面立即弹，非当前页面延迟弹
    if (_chat_data && _chat_data->GetThreadId() == thread_id) {
        // 当前正在该群页面，立即弹出公告
        QTimer::singleShot(300, this, [this, thread_id]() {
            showGroupNoticeDialog(thread_id);
        });
    } else {
        // 不在该群页面，记录待弹出
        m_pendingNoticeThreadIds.insert(thread_id);
        qDebug() << "[ChatPage]: Pending notice for thread_id=" << thread_id
                 << ", pending count=" << m_pendingNoticeThreadIds.size();
    }
}

void ChatPage::on_group_update_notice(int thread_id, const QString& notice)
{
    qDebug() << "[ChatPage]: --- update group notice, thread_id:" << thread_id << "notice:" << notice << " ---";
    
    QJsonObject json;
    json["thread_id"] = thread_id;
    json["from_uid"] = UserMgr::GetInstance()->GetUid();
    json["notice"] = notice;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    TcpMgr::GetInstance()->SendData(ReqId::ID_UPDATE_GROUP_NOTICE_REQ, data);

    // 标记为自己正在编辑的公告，收到 RSP 后不弹窗
    m_selfEditingNoticeThreadId = thread_id;
    if (_chat_data && _chat_data->GetThreadId() == thread_id) {
        _chat_data->SetNotice(notice);
        m_groupSidePage->SetGroupData(_chat_data);
    }
}

ChatPage::~ChatPage()
{
    delete ui;
}

void ChatPage::SetChatData(std::shared_ptr<ChatThreadData> chat_data) {
    _chat_data = chat_data;

    // 如果已加载过历史（m_history_loaded=true），从 _msg_map 重建列表
    // _msg_map 包含初始消息(ASC) + 历史消息(DESC)
    // 排序后：历史最新在顶部，初始消息在底部（正确的视觉顺序）
    if (m_history_loaded) {
        _base_item_map.clear();
        _unrsp_item_map.clear();
        ui->chat_data_list->removeAllItem();

        auto& msg_map = _chat_data->GetMsgMapRef();
        std::vector<std::shared_ptr<ChatDataBase>> sorted_msgs;
        for (auto& msg : msg_map) sorted_msgs.push_back(msg);
        std::sort(sorted_msgs.begin(), sorted_msgs.end(),
                  [](auto& a, auto& b) { return a->GetMsgId() < b->GetMsgId(); });

        const int INITIAL_RENDER_COUNT = 20;
        int startIdx = 0;
        if ((int)sorted_msgs.size() > INITIAL_RENDER_COUNT) {
            startIdx = sorted_msgs.size() - INITIAL_RENDER_COUNT;
        }

        ui->chat_data_list->beginBatchAppend();
        for (int i = startIdx; i < (int)sorted_msgs.size(); ++i) {
            AppendChatMsg(sorted_msgs[i], true);
        }
        ui->chat_data_list->endBatchAppend(true);
        return;
    }

    // 切换到新聊天：清空并重建
    ui->chat_data_list->removeAllItem();
    _unrsp_item_map.clear();
    _base_item_map.clear();

    // 判断是否是群聊
    if(_chat_data->IsGroup()) {
        //群聊
        ui->title_lb->setText(_chat_data->GetGroupName());

        // 切换按钮为群公告模式：改 objectName 以匹配 QSS 中的 #group_notice_lb 样式
        ui->remote_control_lb->setObjectName("group_notice_lb");
        ui->remote_control_lb->SetState("normal", "hover", "press", "normal", "hover", "press");
        ui->remote_control_lb->setProperty("state", "normal");
        // 强制刷新样式
        ui->remote_control_lb->style()->unpolish(ui->remote_control_lb);
        ui->remote_control_lb->style()->polish(ui->remote_control_lb);
        ui->remote_control_lb->update();

        // 设置侧边栏类型为群聊
        m_currentSideType = 1;

        // 群聊不显示在线状态
        m_onlineDot->hide();
        m_onlineText->hide();
        
        // 更新群聊侧边栏数据
        if (m_groupSidePage) {
            m_groupSidePage->SetGroupData(_chat_data);
        }

        // 检查是否有待弹出的群公告（用户之前不在该群页面时收到的公告更新）
        int thread_id = _chat_data->GetThreadId();
        if (m_pendingNoticeThreadIds.contains(thread_id)) {
            m_pendingNoticeThreadIds.remove(thread_id);
            // 延迟弹出，确保 UI 已经显示
            QTimer::singleShot(300, this, [this, thread_id]() {
                showGroupNoticeDialog(thread_id);
            });
        }
        
        //加载群聊消息
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
        
        // ── 虚拟滚动：只渲染最近 20 条，其余等滚动到顶再加载 ──
        const int INITIAL_RENDER_COUNT = 20;
        int startIdx = 0;
        if (sorted_msgs.size() > INITIAL_RENDER_COUNT) {
            startIdx = sorted_msgs.size() - INITIAL_RENDER_COUNT;
            _chat_data->SetHasMoreHistory(true);                  // map 中还有更早消息
            m_oldest_rendered_msg_id = sorted_msgs[startIdx]->GetMsgId();
        }

    // 如果已经加载过历史消息（m_history_loaded=true），直接返回
    // 因为 slot_load_more_history_rsp 已经通过 PrependChatMsg 渲染了历史消息
    // SetChatData 重建列表会丢失这些已渲染的历史
    if (m_history_loaded) {
        // 仍然需要将 _msg_map 中新增的消息渲染到 UI（仅追加，不清空）
        // 注意：此时 _base_item_map 已被清空，但 _msg_map 包含所有消息
        // 因为 _msg_map 中历史消息是 DESC 存储，排序后 H_newest 在顶部
        // 这与 PrependChatMsg 的视觉效果一致（newest 在上）
        ui->chat_data_list->beginBatchAppend();
        for (auto& msg : sorted_msgs) {
            if (!_base_item_map.contains(msg->GetMsgId())) {
                AppendChatMsg(msg, true);
            }
        }
        ui->chat_data_list->endBatchAppend(true);
        return;
    }

        ui->chat_data_list->beginBatchAppend();
        for (int i = startIdx; i < sorted_msgs.size(); ++i) {
            AppendChatMsg(sorted_msgs[i], true);
        }

        // 添加未响应的消息
        for(auto& msg : chat_data->GetMsgUnRspRef()){
            AppendChatMsg(msg, false);
        }
        ui->chat_data_list->endBatchAppend(true);
        return;
    }

    //私聊
    auto other_id = _chat_data->GetOtherId();
    auto friend_info = UserMgr::GetInstance()->GetFriendById(other_id);
    if (friend_info == nullptr) {
        return;
    }
    ui->title_lb->setText(friend_info->_name);

    // 显示好友在线状态
    bool online = UserMgr::GetInstance()->IsFriendOnline(other_id);
    if (online) {
        m_onlineDot->show();
        m_onlineText->setText("在线");
        m_onlineText->show();
    } else {
        m_onlineDot->hide();
        m_onlineText->hide();
    }

    // 切换按钮为远程控制模式
    ui->remote_control_lb->setObjectName("remote_control_lb");
    ui->remote_control_lb->SetState("normal", "hover", "press", "normal", "hover", "press");
    ui->remote_control_lb->setProperty("state", "normal");
    ui->remote_control_lb->style()->unpolish(ui->remote_control_lb);
    ui->remote_control_lb->style()->polish(ui->remote_control_lb);
    ui->remote_control_lb->update();
    
    // 设置侧边栏类型为私聊
    m_currentSideType = 0;
    
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

    // ── 虚拟滚动：只渲染最近 20 条，其余等滚动到顶再加载 ──
    const int INITIAL_RENDER_COUNT = 20;
    int startIdx = 0;
    if (sorted_msgs.size() > INITIAL_RENDER_COUNT) {
        startIdx = sorted_msgs.size() - INITIAL_RENDER_COUNT;
        _chat_data->SetHasMoreHistory(true);                  // map 中还有更早消息
        m_oldest_rendered_msg_id = sorted_msgs[startIdx]->GetMsgId();
    }

    ui->chat_data_list->beginBatchAppend();
    for (int i = startIdx; i < sorted_msgs.size(); ++i) {
        AppendChatMsg(sorted_msgs[i], true);
    }

    // 添加未响应的消息（这些通常是用户自己发送的未确认消息）
    for(auto& msg : chat_data->GetMsgUnRspRef()){
        AppendChatMsg(msg, false);
    }
    ui->chat_data_list->endBatchAppend(true);
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
        if (_chat_data && _chat_data->IsGroup()) {
            pChatItem->showUserName(true);
        }

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

        // 群聊中先从群成员列表找发送者信息，再从好友列表找（兼容群成员同时是好友的情况）
        QString sender_name;
        QString sender_icon;
        int sender_uid = msg->GetSendUid();

        if (_chat_data && _chat_data->IsGroup()) {
            // 群聊：从群成员列表查找
            auto members = _chat_data->GetGroupMembers();
            bool found = false;
            for (const auto& member : members) {
                if (member->_uid == sender_uid) {
                    sender_name = member->_name;
                    sender_icon = member->_icon;
                    found = true;
                    break;
                }
            }
            if (!found) {
                // 群成员列表中没找到，尝试好友列表
                auto friend_info = UserMgr::GetInstance()->GetFriendById(sender_uid);
                if (friend_info) {
                    sender_name = friend_info->_name;
                    sender_icon = friend_info->_icon;
                } else {
                    // 都找不到，用默认显示
                    sender_name = QString("用户%1").arg(sender_uid);
                    sender_icon = "";
                }
            }
        } else {
            // 私聊：保持原逻辑
            auto friend_info = UserMgr::GetInstance()->GetFriendById(sender_uid);
            if (friend_info == nullptr) {
                return;
            }
            sender_name = friend_info->_name;
            sender_icon = friend_info->_icon;
        }

        pChatItem->setUserName(sender_name);
        if (_chat_data && _chat_data->IsGroup()) {
            pChatItem->showUserName(true);
        }

        SetChatIcon(pChatItem, sender_uid, sender_icon, "other_icon");

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
        if (_chat_data && _chat_data->IsGroup()) {
            pChatItem->showUserName(true);
        }

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

        // 群聊中先从群成员列表找发送者信息，再从好友列表找（兼容群成员同时是好友的情况）
        QString sender_name;
        QString sender_icon;
        int sender_uid = msg->GetSendUid();

        if (_chat_data && _chat_data->IsGroup()) {
            // 群聊：从群成员列表查找
            auto members = _chat_data->GetGroupMembers();
            bool found = false;
            for (const auto& member : members) {
                if (member->_uid == sender_uid) {
                    sender_name = member->_name;
                    sender_icon = member->_icon;
                    found = true;
                    break;
                }
            }
            if (!found) {
                // 群成员列表中没找到，尝试好友列表
                auto friend_info = UserMgr::GetInstance()->GetFriendById(sender_uid);
                if (friend_info) {
                    sender_name = friend_info->_name;
                    sender_icon = friend_info->_icon;
                } else {
                    // 都找不到，用默认显示
                    sender_name = QString("用户%1").arg(sender_uid);
                    sender_icon = "";
                }
            }
        } else {
            // 私聊：保持原逻辑
            auto friend_info = UserMgr::GetInstance()->GetFriendById(sender_uid);
            if (friend_info == nullptr) {
                return;
            }
            sender_name = friend_info->_name;
            sender_icon = friend_info->_icon;
        }

        pChatItem->setUserName(sender_name);
        if (_chat_data && _chat_data->IsGroup()) {
            pChatItem->showUserName(true);
        }

        //设置头像
        SetChatIcon(pChatItem, sender_uid, sender_icon, "other_icon");

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
    if (msg_info->_msg_type == MsgType::IMG_MSG) {
        // 无论 bubble 是否已经 append，先把 msg_info 中的数据更新好。
        // msg_info 与 ImgChatData._msg_info 是同一个 shared_ptr，
        // 若下载完成比 AppendOtherMsg 更早到达主线程（竞态），
        // AppendOtherMsg 在后续执行时就能直接拿到真实图片。
        QPixmap picture(file_path);
        msg_info->_preview_pix     = picture;
        msg_info->_transfer_state  = TransferState::Completed;
        msg_info->_current_size    = msg_info->_total_size;
        msg_info->_text_or_url     = file_path;

        auto iter = _base_item_map.find(msg_info->_msg_id);
        if (iter == _base_item_map.end()) {
            // bubble 尚未 append（竞态），数据已更新，AppendOtherMsg 会用正确图片创建 bubble
            return;
        }

        // bubble 已存在，直接刷新显示
        auto bubble = iter.value()->getBubble();
        PictureBubble* pic_bubble = dynamic_cast<PictureBubble*>(bubble);
        if (pic_bubble) {
            pic_bubble->setDownloadFinish(msg_info, file_path);
        }
    }
    else if (msg_info->_msg_type == MsgType::FILE_MSG) {
        auto iter = _base_item_map.find(msg_info->_msg_id);
        if (iter == _base_item_map.end()) {
            return;
        }
        auto bubble = iter.value()->getBubble();
        FileBubble* file_bubble = dynamic_cast<FileBubble*>(bubble);
        if (file_bubble) {
            file_bubble->setDownloadFinish(msg_info, file_path);
        }
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

// ==================== 群公告相关方法 ====================

void ChatPage::on_group_notice_clicked()
{
    if (!_chat_data || !_chat_data->IsGroup()) {
        // 私聊模式下 remote_control_lb 不应有群公告功能，忽略
        return;
    }
    showGroupNoticeDialog(_chat_data->GetThreadId());
}

void ChatPage::slot_friend_status_changed(int uid, bool online)
{
    // 只在私聊时更新在线状态
    if (!_chat_data || _chat_data->IsGroup()) {
        return;
    }

    int other_id = _chat_data->GetOtherId();
    if (uid != other_id) {
        return; // 不是当前聊天对象的状态变化
    }

    if (online) {
        m_onlineDot->show();
        m_onlineText->setText("在线");
        m_onlineText->show();
    } else {
        m_onlineDot->hide();
        m_onlineText->hide();
    }
}

void ChatPage::slot_notify_msg_read(int thread_id, int reader_uid)
{
    Q_UNUSED(reader_uid);
    // 只处理当前会话的已读通知
    if (!_chat_data || _chat_data->GetThreadId() != thread_id) {
        return;
    }

    // 遍历所有消息气泡，将 Self 的消息状态更新为 READED
    for (auto it = _base_item_map.begin(); it != _base_item_map.end(); ++it) {
        auto* item = it.value();
        if (!item) continue;
        // 只更新自己发送的消息
        if (item->getRole() == ChatRole::Self) {
            item->setStatus(MsgStatus::READED);
        }
    }

    qDebug() << "[ChatPage]: messages marked as read in thread" << thread_id;
}

void ChatPage::showGroupNoticeDialog(int thread_id)
{
    QString notice;
    if (_chat_data && _chat_data->GetThreadId() == thread_id) {
        notice = _chat_data->GetNotice();
    } else {
        // 从 UserMgr 获取
        auto thread_data = UserMgr::GetInstance()->GetGroupChat(thread_id);
        if (thread_data) {
            notice = thread_data->GetNotice();
        }
    }

    GroupNoticeViewDialog dlg(notice, this);
    dlg.exec();
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

// ==================== 侧边栏相关方法 ====================

void ChatPage::initSidePage()
{
    // 创建私聊侧边栏
    m_sidePage = new ChatSidePage(this);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_update_group_notice, this, &ChatPage::slot_group_update_notice);

    connect(m_sidePage, &ChatSidePage::sig_search_text_changed, this, &ChatPage::slot_chat_search);
    
    // 创建群聊侧边栏
    m_groupSidePage = new GroupSidePage(this);
    connect(m_groupSidePage, &GroupSidePage::sig_view_members, this, &ChatPage::on_group_view_members);
    connect(m_groupSidePage, &GroupSidePage::sig_invite_friend, this, &ChatPage::on_group_invite_friend);
    connect(m_groupSidePage, &GroupSidePage::sig_clear_history, this, &ChatPage::on_group_clear_history);
    connect(m_groupSidePage, &GroupSidePage::sig_dismiss_group, this, &ChatPage::on_group_dismiss);
    connect(m_groupSidePage, &GroupSidePage::sig_exit_group, this, &ChatPage::on_group_exit);
    connect(m_groupSidePage, &GroupSidePage::sig_report_group, this, &ChatPage::on_group_report);
    connect(m_groupSidePage, &GroupSidePage::sig_update_notice, this, &ChatPage::on_group_update_notice);
    
    // 初始化侧边栏布局 (两个侧边栏都添加到布局中，通过 show/hide 切换)
    QVBoxLayout* sideLayout = new QVBoxLayout(ui->side_page_wid);
    sideLayout->setContentsMargins(0,0,0,0);
    sideLayout->addWidget(m_sidePage);
    sideLayout->addWidget(m_groupSidePage);
    m_groupSidePage->hide(); // 默认隐藏群聊侧边栏
    ui->side_page_wid->hide();
}

void ChatPage::toggleSidePage()
{
    if (m_sideAnim->state() == QAbstractAnimation::Running) {
        return;
    }

    // 优化性能：动画期间禁用聊天区域的布局更新，减少重绘压力
    ui->chat_data_wid->setUpdatesEnabled(false);

    // 如果当前正在显示群成员列表，先返回到群聊侧边栏
    if (m_memberListPage && m_memberListPage->isVisible()) {
        m_memberListPage->hide();
        if (m_groupSidePage) {
            m_groupSidePage->show();
        }
    }

    // 根据当前聊天类型切换侧边栏内容 (通过 show/hide 切换，不重建布局)
    if (m_currentSideType == 1 && _chat_data && _chat_data->IsGroup()) {
        // 群聊侧边栏
        m_sidePage->hide();
        m_groupSidePage->SetGroupData(_chat_data);
        m_groupSidePage->show();
    } else {
        // 私聊侧边栏
        m_groupSidePage->hide();
        m_sidePage->show();
    }

    if (ui->side_page_wid->isHidden()) {
        ui->side_page_wid->show();
        // 展开前先解除固定宽度，让 maximumWidth 动画生效
        ui->side_page_wid->setMinimumWidth(0);
        ui->side_page_wid->setMaximumWidth(16777215);
        m_sideAnim->setStartValue(0);
        m_sideAnim->setEndValue(280);
        disconnect(m_sideAnim, &QPropertyAnimation::finished, nullptr, nullptr);
        connect(m_sideAnim, &QPropertyAnimation::finished, this, [this](){
            // 动画结束后，强制设置容器的精确宽度，防止布局偏差导致子控件被裁剪
            ui->side_page_wid->setFixedWidth(280);
            ui->chat_data_wid->setUpdatesEnabled(true);
            ui->chat_data_wid->update();
        });
        m_sideAnim->start();
    } else {
        // 收起前先恢复约束，让动画能正常工作
        ui->side_page_wid->setMinimumWidth(0);
        ui->side_page_wid->setMaximumWidth(16777215);
        m_sideAnim->setStartValue(280);
        m_sideAnim->setEndValue(0);
        disconnect(m_sideAnim, &QPropertyAnimation::finished, nullptr, nullptr);
        connect(m_sideAnim, &QPropertyAnimation::finished, this, [this](){
            ui->side_page_wid->hide();
            ui->chat_data_wid->setUpdatesEnabled(true);
            ui->chat_data_wid->update();
        });
        m_sideAnim->start();
    }
}

void ChatPage::showGroupMemberList(int thread_id)
{
    // 创建群成员列表页面
    if (m_memberListPage) {
        m_memberListPage->deleteLater();
        m_memberListPage = nullptr;
    }
    
    m_memberListPage = new GroupMemberListPage(thread_id, this);
    
    // 获取群成员数据
    auto group_data = UserMgr::GetInstance()->GetGroupChat(thread_id);
    if (group_data) {
        m_memberListPage->SetMembers(group_data->GetGroupMembers());
    }
    
    // 连接信号
    connect(m_memberListPage, &GroupMemberListPage::sig_back, this, [this](){
        // 返回群聊侧边栏
        if (m_memberListPage) {
            m_memberListPage->hide();
        }
        if (m_groupSidePage) {
            m_groupSidePage->show();
        }
    });
    
    connect(m_memberListPage, &GroupMemberListPage::sig_invite_friend, this, [this](int tid){
        // 显示邀请好友对话框
        // TODO: 实现邀请好友功能
        qDebug() << "[ChatPage]: Invite friend to group" << tid;
    });
    
    connect(m_memberListPage, &GroupMemberListPage::sig_member_clicked, this, [this](int uid){
        // 点击成员，显示成员信息
        qDebug() << "[ChatPage]: Member clicked" << uid;
    });
    
    // 隐藏群聊侧边栏，显示成员列表
    if (m_groupSidePage) {
        m_groupSidePage->hide();
    }
    
    QVBoxLayout* sideLayout = qobject_cast<QVBoxLayout*>(ui->side_page_wid->layout());
    if (sideLayout) {
        sideLayout->addWidget(m_memberListPage);
    }
    m_memberListPage->show();
}

// ==================== 群聊侧边栏信号处理 ====================

void ChatPage::on_group_view_members(int thread_id)
{
    qDebug() << "[ChatPage]: View group members" << thread_id;
    showGroupMemberList(thread_id);
}

void ChatPage::on_group_invite_friend(int thread_id)
{
    qDebug() << "[ChatPage]: Invite friend to group" << thread_id;
    // TODO: 显示邀请好友对话框
}

void ChatPage::on_group_clear_history(int thread_id)
{
    qDebug() << "[ChatPage]: Clear group chat history" << thread_id;
    // TODO: 清空聊天记录
}

void ChatPage::on_group_dismiss(int thread_id)
{
    qDebug() << "[ChatPage]: Dismiss group" << thread_id;
    // TODO: 解散群聊
}

void ChatPage::on_group_exit(int thread_id)
{
    qDebug() << "[ChatPage]: Exit group" << thread_id;
    // TODO: 退出群聊
}

void ChatPage::on_group_report(int thread_id)
{
    qDebug() << "[ChatPage]: Report group" << thread_id;
    // TODO: 举报群聊
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

    //判断是否是群聊
    bool is_group_chat = _chat_data->IsGroup();
    ChatFormType form_type = is_group_chat ? ChatFormType::GROUP : ChatFormType::PRIVATE;

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
                // 群聊标记
                textObj["is_group"] = is_group_chat;
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

            //根据是否是群聊选择消息类型
            QString currentTimeTxt = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            auto txt_msg = std::make_shared<TextChatData>(uuidString, thread_id, form_type,
                ChatMsgType::TEXT, content, user_info->_uid, 0, currentTimeTxt);
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
                textObj["is_group"] = is_group_chat;
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
            QString currentTimeImg = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            auto img_msg = std::make_shared<ImgChatData>(msgList[i],uuidString, thread_id, form_type,
                    ChatMsgType::PIC, user_info->_uid, 0, currentTimeImg);
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
            textObj["is_group"] = is_group_chat;

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
                textObj["is_group"] = is_group_chat;
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

            QString currentTimeFile = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            auto file_msg = std::make_shared<FileChatData>(msgList[i], uuidString, thread_id, form_type,
                    ChatMsgType::FILE, user_info->_uid, 0, currentTimeFile);
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
            textObj["is_group"] = is_group_chat;

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
        textObj["is_group"] = is_group_chat;
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
    if (_chat_data == nullptr) {
        return;
    }

    // 性能优化：搜索期间禁用更新
    ui->chat_data_list->setUpdatesEnabled(false);

    if (text.isEmpty()) {
        for (auto& item : _base_item_map) {
            item->show();
        }
        ui->chat_data_list->setUpdatesEnabled(true);
        ui->chat_data_list->update();
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

    ui->chat_data_list->setUpdatesEnabled(true);
    ui->chat_data_list->update();
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

void ChatPage::slot_load_more_history()
{
    // 没有聊天数据、没有更多历史，或者正在加载中时，直接返回
    if (!_chat_data) return;
    if (!_chat_data->HasMoreHistory()) return;
    if (m_loading_history) return;

    m_loading_history = true;

    // 找到当前已渲染的最旧 msg_id
    // _base_item_map key 是 msg_id（qint64），取最小值即为最旧
    if (_base_item_map.isEmpty()) {
        m_loading_history = false;
        return;
    }

    // QHash 无序，需要遍历找最小 key
    qint64 oldest_id = std::numeric_limits<qint64>::max();
    for (auto it = _base_item_map.cbegin(); it != _base_item_map.cend(); ++it) {
        if (it.key() < oldest_id) {
            oldest_id = it.key();
        }
    }

    qDebug() << "[ChatPage]: slot_load_more_history - thread_id=" << _chat_data->GetThreadId()
             << "oldest_rendered_msg_id=" << oldest_id;

    // 发出信号，让 ChatDialog 负责发 TCP 请求
    emit sig_request_load_history(_chat_data->GetThreadId(), static_cast<int>(oldest_id));
}

void ChatPage::slot_load_more_history_rsp(int thread_id, bool has_more,
                                           std::vector<std::shared_ptr<ChatDataBase>> chat_datas)
{
    // 释放加载锁
    m_loading_history = false;

    // 标记已加载过历史，防止 SetChatData 重建列表时丢失已头插的消息
    m_history_loaded = true;

    // 验证 thread_id 是否仍是当前显示的聊天
    if (!_chat_data || _chat_data->GetThreadId() != thread_id) {
        return;
    }

    if (chat_datas.empty()) {
        return;
    }

    // 按 msg_id 升序排列（服务端已排序，但以防万一）
    std::sort(chat_datas.begin(), chat_datas.end(),
              [](const std::shared_ptr<ChatDataBase>& a, const std::shared_ptr<ChatDataBase>& b) {
                  return a->GetMsgId() < b->GetMsgId();
              });

    // 批量头插：先暂停 repaint，锁定滚动位置
    // 注意：PrependChatMsg 插入到位置 0，所以需要反向迭代
    // 这样 oldest 消息最终在底部，newest 在顶部
    ui->chat_data_list->beginBatchPrepend();
    for (auto it = chat_datas.rbegin(); it != chat_datas.rend(); ++it) {
        // 已经在 _base_item_map 中的消息不重复渲染
        if (_base_item_map.contains((*it)->GetMsgId())) {
            continue;
        }
        PrependChatMsg(*it);
    }
    ui->chat_data_list->endBatchPrepend();

    qDebug() << "[ChatPage]: slot_load_more_history_rsp - prepended" << chat_datas.size()
             << "msgs, has_more=" << has_more;
}

void ChatPage::PrependChatMsg(std::shared_ptr<ChatDataBase> msg)
{
    auto self_info = UserMgr::GetInstance()->GetUserInfo();
    ChatRole role = (msg->GetSendUid() == self_info->_uid) ? ChatRole::Self : ChatRole::Other;

    ChatItemBase* pChatItem = new ChatItemBase(role);

    if (role == ChatRole::Self) {
        pChatItem->setUserName(self_info->_name);
        if (_chat_data && _chat_data->IsGroup()) {
            pChatItem->showUserName(true);
        }
        SetChatIcon(pChatItem, self_info->_uid, self_info->_icon, "self_icon");
    } else {
        int sender_uid = msg->GetSendUid();
        QString sender_name;
        QString sender_icon;

        if (_chat_data && _chat_data->IsGroup()) {
            auto members = _chat_data->GetGroupMembers();
            bool found = false;
            for (const auto& member : members) {
                if (member->_uid == sender_uid) {
                    sender_name = member->_name;
                    sender_icon = member->_icon;
                    found = true;
                    break;
                }
            }
            if (!found) {
                auto friend_info = UserMgr::GetInstance()->GetFriendById(sender_uid);
                if (friend_info) {
                    sender_name = friend_info->_name;
                    sender_icon = friend_info->_icon;
                } else {
                    sender_name = QString("用户%1").arg(sender_uid);
                }
            }
        } else {
            auto friend_info = UserMgr::GetInstance()->GetFriendById(sender_uid);
            if (!friend_info) {
                delete pChatItem;
                return;
            }
            sender_name = friend_info->_name;
            sender_icon = friend_info->_icon;
        }

        pChatItem->setUserName(sender_name);
        if (_chat_data && _chat_data->IsGroup()) {
            pChatItem->showUserName(true);
        }
        SetChatIcon(pChatItem, msg->GetSendUid(), sender_icon, "other_icon");
    }

    QWidget* pBubble = nullptr;
    if (msg->GetMsgType() == ChatMsgType::TEXT) {
        pBubble = new TextBubble(role, msg->GetMsgContent());
    } else if (msg->GetMsgType() == ChatMsgType::PIC) {
        auto img_msg = std::dynamic_pointer_cast<ImgChatData>(msg);
        auto pic_bubble = new PictureBubble(img_msg->_msg_info->_preview_pix, role, img_msg->_msg_info->_total_size);
        pic_bubble->setMsgInfo(img_msg->_msg_info);
        pBubble = pic_bubble;
        connect(pic_bubble, &PictureBubble::pauseRequested,  this, &ChatPage::on_clicked_paused);
        connect(pic_bubble, &PictureBubble::resumeRequested, this, &ChatPage::on_clicked_resume);
        connect(pic_bubble, &PictureBubble::viewRequested,   this, &ChatPage::on_view_picture);
    } else if (msg->GetMsgType() == ChatMsgType::FILE) {
        auto file_msg = std::dynamic_pointer_cast<FileChatData>(msg);
        auto file_bubble = new FileBubble(role, file_msg->_msg_info->_text_or_url, file_msg->_msg_info->_total_size);
        file_bubble->setMsgInfo(file_msg->_msg_info);
        pBubble = file_bubble;
        connect(file_bubble, &FileBubble::pauseRequested,  this, &ChatPage::on_clicked_paused);
        connect(file_bubble, &FileBubble::resumeRequested, this, &ChatPage::on_clicked_resume);
    }

    if (!pBubble) {
        delete pChatItem;
        return;
    }

    connect(dynamic_cast<BubbleFrame*>(pBubble), &BubbleFrame::sig_delete_msg,
            this, [this, pChatItem, msg]() {
        ui->chat_data_list->layout()->removeWidget(pChatItem);
        pChatItem->deleteLater();
        _base_item_map.remove(msg->GetMsgId());
    });

    pChatItem->setWidget(pBubble);
    pChatItem->setStatus(msg->GetStatus());

    // 头插到 layout 顶部
    ui->chat_data_list->prependChatItem(pChatItem);
    _base_item_map[msg->GetMsgId()] = pChatItem;
}
