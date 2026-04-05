#include "chatdialog.h"
#include "ui_chatdialog.h"
#include <QRandomGenerator>
#include "chatuserwid.h"
#include <QLineEdit>
#include <QMouseEvent>
#include "searchlist.h"
#include "contactuserlist.h"
#include "applyfriendpage.h"
#include "tcpmgr.h"
#include "usermgr.h"
#include "conuseritem.h"
#include <QStandardPaths>
#include "filetcpmgr.h"

ChatDialog::ChatDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ChatDialog),_mode(ChatUIMode::ChatMode),_state(ChatUIMode::ChatMode)
    ,_b_loading(false),_cur_chat_thread_id(0),_last_widget(nullptr),_loading_dialog(nullptr)
    ,_more_dialog(nullptr), _user_info_dialog(nullptr)
{
    ui->setupUi(this);

    //对搜索功能进行设置
    ui->add_btn->SetState("normal","hover","press");
    ui->search_edit->SetMaxLength(20);

    QAction *searchAction = new QAction(ui->search_edit);
    searchAction->setIcon(QIcon(":/images/search.png"));
    ui->search_edit->addAction(searchAction,QLineEdit::LeadingPosition);
    ui->search_edit->setPlaceholderText(QStringLiteral("搜索"));

    // 创建一个清除动作并设置图标
    QAction *clearAction = new QAction(ui->search_edit);
    clearAction->setIcon(QIcon(":/images/close_transparent.png"));
    // 初始时不显示清除图标
    // 将清除动作添加到LineEdit的末尾位置
    ui->search_edit->addAction(clearAction, QLineEdit::TrailingPosition);
    // 当需要显示清除图标时，更改为实际的清除图标
    connect(ui->search_edit, &QLineEdit::textChanged, [clearAction](const QString &text) {
        if (!text.isEmpty()) {
            clearAction->setIcon(QIcon(":/images/close.png"));
        } else {
            clearAction->setIcon(QIcon(":/images/close_transparent.png")); // 文本为空时，切换回透明图标
        }
    });

    // 连接清除动作的触发信号到槽函数，用于清除文本
    connect(clearAction, &QAction::triggered, [this, clearAction]() {
        ui->search_edit->clear();
        clearAction->setIcon(QIcon(":/images/close_transparent.png")); // 清除文本后，切换回透明图标
        ui->search_edit->clearFocus();
        //清除按钮被按下则不显示搜索框
        ShowSearch(false);
    });

    //初始化时先不显示搜索列表
    ShowSearch(false);
    ui->ai_user_list->hide();
    ui->search_edit->SetMaxLength(15);

    //连接加载聊天列表信号和槽
    connect(ui->chat_user_list,&ChatUserList::sig_loading_chat_user,this,&ChatDialog::slot_loading_chat_user);

    //连接加载好友信息列表信号和槽
    connect(ui->con_user_list,&ContactUserList::sig_loading_contact_user,this,&ChatDialog::slot_loading_contact_user);

    //加载好友聊天列表（已废弃）
    //addChatUserList();

    //获取个人信息
    auto self_info = UserMgr::GetInstance()->GetUserInfo();

    // 设置头像
    slot_reset_head();

    //刷新侧边栏头像信号和槽函数
    connect(FileTcpMgr::GetInstance().get(), &FileTcpMgr::sig_reset_head, this, &ChatDialog::slot_reset_head);

    //加载侧边栏
    ui->side_chat_lb->setProperty("state","normal");
    ui->side_chat_lb->SetState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");
    ui->side_contact_lb->setProperty("state","normal");
    ui->side_contact_lb->SetState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");
    ui->side_settings_lb->setProperty("state","normal");
    ui->side_settings_lb->SetState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");
    ui->side_tool_lb->setProperty("state","normal");
    ui->side_tool_lb->SetState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");
    ui->side_ai_lb->setProperty("state","normal");
    ui->side_ai_lb->SetState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");
    ui->side_video_lb->setProperty("state","normal");
    ui->side_video_lb->SetState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");
    ui->more_lb->SetState("normal","hover","press","normal","hover","press");
    ui->side_head_lb->SetState("normal","hover","press","normal","hover","press");

    AddLBGroup(ui->side_contact_lb);
    AddLBGroup(ui->side_chat_lb);
    AddLBGroup(ui->side_settings_lb);
    AddLBGroup(ui->side_tool_lb);
    AddLBGroup(ui->side_ai_lb);
    AddLBGroup(ui->side_video_lb);

    //连接侧边栏点击后页面变化的槽函数
    connect(ui->side_chat_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_chat);
    connect(ui->side_contact_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_contact);
    connect(ui->side_settings_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_settings);
    connect(ui->side_tool_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_tools);
    connect(ui->side_ai_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_ai);
    connect(ui->side_video_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_video);

    //连接搜索框输入变化
    connect(ui->search_edit,&QLineEdit::textChanged,this,&ChatDialog::slot_text_changed);

    //连接更多按钮点击信号
    connect(ui->more_lb, &ClickedLabel::clicked, this, &ChatDialog::slot_show_more);

    //连接头像点击信号
    connect(ui->side_head_lb, &ClickedLabel::clicked, this, &ChatDialog::slot_show_user_info);

    //检测鼠标点击位置判断是否要清空搜索框
    this->installEventFilter(this); //安装事件过滤器

    //设置侧边栏聊天label选中状态
    ui->side_chat_lb->SetSelected(true);

    //为searchlist设置search edit
    ui->search_list->SetSearchEdit(ui->search_edit);

    //设置选中聊天条目
    SetSelectChatItem();

    //设置更新聊天界面信息
    SetSelectChatPage();

    //连接申请添加好友信号
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_friend_apply,this,&ChatDialog::slot_apply_friend);

    //链接对端被同意认证后通知的信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_add_auth_friend,this,
            &ChatDialog::slot_add_auth_firend);

    //链接自己点击同意认证后界面刷新
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_auth_rsp,this,
            &ChatDialog::slot_auth_rsp);

    //连接searchlist跳转聊天信号
    connect(ui->search_list,&SearchList::sig_jump_chat_item,this,&ChatDialog::slot_jump_chat_item);

    //连接好友信息页面friendinfopage跳转聊天页面信号
    connect(ui->friend_info_page,&FriendInfoPage::sig_jump_chat_item,this,&ChatDialog::slot_jump_chat_item_from_infopage);

    //连接点击联系人item发出的信号和用户信息展示槽函数
    connect(ui->con_user_list,&ContactUserList::sig_switch_friend_info_page,this,&ChatDialog::slot_friend_info_page);

    //连接联系人页面ContactUserList点击好友申请条目的信号
    connect(ui->con_user_list,&ContactUserList::sig_switch_apply_friend_page,this,&ChatDialog::slot_switch_apply_friend_page);

    //设置中心部件为chatpage
    ui->stackedWidget->setCurrentWidget(ui->chat_page);

    // 添加工具页面
    ui->stackedWidget->addWidget(new ToolsPage(this));

    // 添加 AI 页面
    _ai_page = new AIPage(this);
    ui->stackedWidget->addWidget(_ai_page);

    // 添加短视频页面
    _video_page = new VideoPage(this);
    ui->stackedWidget->addWidget(_video_page);
    
    // 连接 AI 页面最后一条消息更新信号
    connect(_ai_page, &AIPage::sig_update_last_msg, this, [this](int thread_id, const QString& msg){
        auto find_iter = _ai_thread_items.find(thread_id);
        if (find_iter != _ai_thread_items.end()) {
            auto widget = ui->ai_user_list->itemWidget(find_iter.value());
            auto ai_wid = qobject_cast<ChatUserWid*>(widget);
            if (ai_wid) {
                // 更新显示
                auto label = ai_wid->findChild<QLabel*>("user_chat_lb");
                if (label) label->setText(msg);
            }
        }
    });

    // 默认创建一个 AI 会话
    {
        int new_id = -1;
        auto ai_data = std::make_shared<ChatThreadData>(0, new_id, 0);
        auto* chat_user_wid = new ChatUserWid();
        chat_user_wid->SetChatData(ai_data);
        QListWidgetItem* item = new QListWidgetItem;
        item->setSizeHint(chat_user_wid->sizeHint());
        ui->ai_user_list->addItem(item);
        ui->ai_user_list->setItemWidget(item, chat_user_wid);
        _ai_thread_items.insert(new_id, item);
        _cur_ai_thread_id = new_id;
        _ai_page->SetChatData(ai_data);
    }

    //连接聊天列表点击信号
    connect(ui->chat_user_list,&QListWidget::itemClicked,this,&ChatDialog::slot_item_clicked);
    
    //连接 AI 列表点击信号
    connect(ui->ai_user_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item){
        auto widget = ui->ai_user_list->itemWidget(item);
        auto ai_wid = qobject_cast<ChatUserWid*>(widget);
        if (ai_wid) {
            _cur_ai_thread_id = ai_wid->GetChatData()->GetThreadId();
            _ai_page->SetChatData(ai_wid->GetChatData());
        }
    });

    //连接加号按钮点击信号 (如果是 AI 模式则创建新对话)
    connect(ui->add_btn, &ClickedBtn::clicked, this, [this](){
        if (_state == ChatUIMode::AIMode) {
            // 创建新 AI 对话
            int new_id = -(_ai_thread_items.size() + 1); // 使用负 ID 区分
            auto ai_data = std::make_shared<ChatThreadData>(0, new_id, 0);
            
            auto* chat_user_wid = new ChatUserWid();
            chat_user_wid->SetChatData(ai_data);
            
            QListWidgetItem* item = new QListWidgetItem;
            item->setSizeHint(chat_user_wid->sizeHint());
            ui->ai_user_list->insertItem(0, item);
            ui->ai_user_list->setItemWidget(item, chat_user_wid);
            _ai_thread_items.insert(new_id, item);
            
            ui->ai_user_list->setCurrentItem(item);
            _cur_ai_thread_id = new_id;
            _ai_page->SetChatData(ai_data);
        }
    });

    //连接对方发来文本消息通知
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_text_chat_msg, this, &ChatDialog::slot_text_chat_msg);

    //连接对方发来图片消息通知
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_img_chat_msg, this, &ChatDialog::slot_img_chat_msg);

    //连接对方发来文件消息通知
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_file_chat_msg, this, &ChatDialog::slot_file_chat_msg);

    //注册心跳检测
    _timer = new QTimer(this);
    connect(_timer,&QTimer::timeout,this,[this](){
        auto user_info = UserMgr::GetInstance()->GetUserInfo();
        QJsonObject textObj;
        textObj["fromuid"] = user_info->_uid;
        QJsonDocument doc(textObj);
        //压缩一下，并返回QByteArray，json字符数组
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_HEART_BEAT_REQ,jsonData);
    });
    //启动定时器,每10秒给服务器发送一次信息
    _timer->start(10000);

    //连接接收聊天列表
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_load_chat_thread,this,&ChatDialog::slot_load_chat_thread);

    //连接从friendinfopage新创建聊天item
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_create_private_chat,this,&ChatDialog::slot_create_private_chat);

    //连接加载聊天页面chatpage聊天对话消息
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_load_chat_msg,this,&ChatDialog::slot_load_chat_msg);

    //连接发送消息后服务器回传接收到消息的信号后的通知
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_chat_msg_rsp,this,&ChatDialog::slot_add_chat_msg);

    //连接tcp返回的图片聊天信息回复
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_chat_img_rsp, this, &ChatDialog::slot_add_img_msg);

    //连接tcp返回的文件聊天信息回复
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_chat_file_rsp, this, &ChatDialog::slot_add_file_msg);

    //重置label icon
    connect(FileTcpMgr::GetInstance().get(), &FileTcpMgr::sig_reset_label_icon, this, &ChatDialog::slot_reset_icon);

    //接收tcp返回的上传进度信息
    connect(FileTcpMgr::GetInstance().get(), &FileTcpMgr::sig_update_upload_progress,
            this, &ChatDialog::slot_update_upload_progress);

    //接收tcp返回的下载进度信息
    connect(FileTcpMgr::GetInstance().get(), &FileTcpMgr::sig_update_download_progress,
            this, &ChatDialog::slot_update_download_progress);

    //接收tcp返回的下载完成信息
    connect(FileTcpMgr::GetInstance().get(), &FileTcpMgr::sig_download_finish,
            this, &ChatDialog::slot_download_finish);

    //接收tcp返回的传输失败信息
    connect(FileTcpMgr::GetInstance().get(), &FileTcpMgr::sig_file_transfer_failed,
            this, &ChatDialog::slot_file_transfer_failed);

    //连接群聊创建响应信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_create_group_chat_rsp,
            UserMgr::GetInstance().get(), &UserMgr::SlotCreateGroupChatRsp);

    //连接被加入群聊通知信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_group_chat_created,
            UserMgr::GetInstance().get(), &UserMgr::SlotGroupChatCreated);

    //连接群聊创建成功信号，更新UI
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_group_chat_created,
            this, &ChatDialog::slot_group_chat_created);

    // ========== 好友在线状态相关信号 ==========
    // 好友在线状态批量查询结果 -> 一次性刷新所有联系人列表的在线状态
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_friend_online_status,
            UserMgr::GetInstance().get(), [this](QJsonObject online_map){
        UserMgr::GetInstance()->SetFriendOnlineStatusBatch(online_map);
    });

    // 批量更新完成 -> 刷新 UI（只遍历一次列表）
    connect(UserMgr::GetInstance().get(), &UserMgr::sig_friend_online_status_batch,
            this, [this](){
        UpdateAllContactOnlineStatus();
    });

    // 好友上线通知
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_user_online,
            UserMgr::GetInstance().get(), &UserMgr::SlotFriendOnline);

    // 好友下线通知
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_user_offline,
            UserMgr::GetInstance().get(), &UserMgr::SlotFriendOffline);

    // 好友状态变更 -> 更新好友列表中的状态指示器
    connect(UserMgr::GetInstance().get(), &UserMgr::sig_friend_status_changed,
            this, [this](int uid, bool online){
        UpdateContactOnlineStatus(uid, online);
    });

    // 消息已读通知 -> 如果当前正在查看该会话，通知 chatpage 刷新气泡状态
    connect(this, &ChatDialog::sig_notify_msg_read_for_page,
            ui->chat_page, &ChatPage::slot_notify_msg_read);

    // 连接群公告更新信号到 UserMgr，保持内存数据同步
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_update_group_notice,
            UserMgr::GetInstance().get(), &UserMgr::SlotUpdateGroupNotice);

    // 未读消息数查询响应 -> 更新聊天列表角标
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_unread_counts,
            this, [this](QJsonObject unread_counts){
        for(auto it = unread_counts.begin(); it != unread_counts.end(); ++it) {
            int thread_id = it.key().toInt();
            int count = it.value().toInt();
            // 更新 ChatThreadData 中的未读计数
            auto chat_data = UserMgr::GetInstance()->GetChatThreadByThreadId(thread_id);
            if (chat_data) {
                chat_data->SetUnreadCount(count);
            }
            // 更新聊天列表的角标显示
            auto item_iter = _chat_thread_items.find(thread_id);
            if (item_iter != _chat_thread_items.end()) {
                auto* chat_user_wid = dynamic_cast<ChatUserWid*>(ui->chat_user_list->itemWidget(item_iter.value()));
                if (chat_user_wid) {
                    chat_user_wid->UpdateUnreadCount(count);
                }
            }
        }
    });

    // 消息已读通知 -> 更新聊天页面中的消息气泡状态
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_notify_msg_read,
            this, [this](int thread_id, int reader_uid){
        Q_UNUSED(reader_uid);
        // 更新 ChatThreadData 中的消息状态
        auto chat_data = UserMgr::GetInstance()->GetChatThreadByThreadId(thread_id);
        if (chat_data) {
            auto& msg_map = chat_data->GetMsgMapRef();
            for(auto it = msg_map.begin(); it != msg_map.end(); ++it) {
                auto msg = it.value();
                // 只更新自己发送的消息（因为已读回执是对方读了我们的消息）
                auto self_uid = UserMgr::GetInstance()->GetUid();
                if (msg->GetSendUid() == self_uid && msg->GetStatus() != MsgStatus::SEND_FAILED) {
                    msg->SetStatus(MsgStatus::READED);
                }
            }
        }
        // 如果当前正在查看这个会话，刷新 chatpage 中消息气泡的状态图标
        if (_cur_chat_thread_id == thread_id) {
            // 通知 chatpage 刷新已读状态
            emit sig_notify_msg_read_for_page(thread_id, reader_uid);
        }
    });
}

ChatDialog::~ChatDialog()
{
    _timer->start(10000);
    delete ui;
}

// //测试：添加随机用户列表
// void ChatDialog::addChatUserList()
// {
//     //先按照好友列表加载聊天记录，等以后客户端实现聊天记录数据库之后再按照最后信息排序
//     auto friend_list = UserMgr::GetInstance()->GetChatListPerPage();
//     if (friend_list.empty() == false) {
//         for(auto & friend_ele : friend_list){
//             auto find_iter = _chat_items_added.find(friend_ele->_uid);
//             if(find_iter != _chat_items_added.end()){
//                 continue;
//             }
//             auto *chat_user_wid = new ChatUserWid();
//             auto user_info = std::make_shared<UserInfo>(friend_ele);
//             chat_user_wid->SetInfo(user_info);
//             QListWidgetItem *item = new QListWidgetItem;
//             //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
//             item->setSizeHint(chat_user_wid->sizeHint());
//             ui->chat_user_list->addItem(item);
//             ui->chat_user_list->setItemWidget(item, chat_user_wid);
//             _chat_items_added.insert(friend_ele->_uid, item);
//         }

//         //更新已加载条目
//         UserMgr::GetInstance()->UpdateChatLoadedCount();
//     }

//     // // 创建QListWidgetItem，并设置自定义的widget
//     // for(int i = 0; i < 13; i++){
//     //     int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
//     //     int str_i = randomValue%strs.size();
//     //     int head_i = randomValue%heads.size();
//     //     int name_i = randomValue%names.size();

//     //     auto user_info = std::make_shared<UserInfo>(0,names[name_i],names[name_i],heads[head_i],0,strs[str_i]);
//     //     auto *chat_user_wid = new ChatUserWid();
//     //     chat_user_wid->SetInfo(user_info);
//     //     QListWidgetItem *item = new QListWidgetItem;
//     //     //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
//     //     item->setSizeHint(chat_user_wid->sizeHint());
//     //     ui->chat_user_list->addItem(item);
//     //     ui->chat_user_list->setItemWidget(item, chat_user_wid);
//     // }
// }

//(已修改，根据服务器传来的thread_id初始化聊天item)
void ChatDialog::loadChatList()
{
    showLoadingDlg(true);

    //发送请求逻辑
    QJsonObject jsonObj;
    auto uid = UserMgr::GetInstance()->GetUid();
    jsonObj["uid"] = uid;
    jsonObj["thread_id"] = 0;  // 首次加载，从0开始

    QJsonDocument doc(jsonObj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    //发送tcp请求给chat server
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_LOAD_CHAT_THREAD_REQ, jsonData);
}

void ChatDialog::loadChatMsg()
{
    //发送聊天记录请求
    _cur_load_chat = UserMgr::GetInstance()->GetCurLoadData();
    if (_cur_load_chat == nullptr) {
        return;
    }

    showLoadingDlg(true);

    //发送请求给服务器
    //发送请求逻辑
    QJsonObject jsonObj;
    jsonObj["thread_id"] = _cur_load_chat->GetThreadId();
    jsonObj["message_id"] = _cur_load_chat->GetLastMsgId();

    QJsonDocument doc(jsonObj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    //发送tcp请求给chat server
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_LOAD_CHAT_MSG_REQ, jsonData);
}

void ChatDialog::ClearLabelState(StateWidget* lb)
{
    for(auto & ele: _lb_list){
        if(ele == lb){
            continue;
        }
        ele->ClearState();
    }
}

void ChatDialog::SetSelectChatItem(int thread_id)
{
    //没有好友则什么都不做
    if(ui->chat_user_list->count() <= 0){
        return;
    }

    // 临时阻塞信号，防止 setCurrentRow/setCurrentItem 触发 slot_item_clicked
    ui->chat_user_list->blockSignals(true);

    //uid为0，默认取出第一个好友信息
    if(thread_id == 0){
        ui->chat_user_list->setCurrentRow(0);
        QListWidgetItem *firstItem = ui->chat_user_list->item(0);
        if(!firstItem){
            ui->chat_user_list->blockSignals(false);
            return;
        }

        //转为widget
        QWidget *widget = ui->chat_user_list->itemWidget(firstItem);
        if(!widget){
            ui->chat_user_list->blockSignals(false);
            return;
        }

        //根据点击的聊天列表的item获取对方user信息
        auto con_item = qobject_cast<ChatUserWid*>(widget);
        if(!con_item){
            ui->chat_user_list->blockSignals(false);
            return;
        }
        _cur_chat_thread_id = con_item->GetChatData()->GetThreadId();

        ui->chat_user_list->blockSignals(false);
        return;
    }

    //从当前聊天中查找该用户
    auto find_iter =  _chat_thread_items.find(thread_id);
    //没找到->将该聊天添加到最上面
    if(find_iter == _chat_thread_items.end()){
        qDebug() << "[ChatDialog]: thread_id [" << thread_id << "] not found, set curent row 0";
        ui->chat_user_list->setCurrentRow(0);
        ui->chat_user_list->blockSignals(false);
        return;
    }
    //找到了
    ui->chat_user_list->setCurrentItem(find_iter.value());

    _cur_chat_thread_id = thread_id;

    ui->chat_user_list->blockSignals(false);
}

void ChatDialog::SetSelectChatPage(int thread_id)
{
    //没有好友则什么都不做
    if( ui->chat_user_list->count() <= 0){
        return;
    }

    //uid为0，默认取出第一个好友信息
    if (thread_id == 0) {
        auto item = ui->chat_user_list->item(0);
        //转为widget
        QWidget* widget = ui->chat_user_list->itemWidget(item);
        if (!widget) {
            return;
        }

        auto con_item = qobject_cast<ChatUserWid*>(widget);
        if (!con_item) {
            return;
        }

        //设置信息
        auto chat_data = con_item->GetChatData();
        ui->chat_page->SetChatData(chat_data);
        return;
    }

    auto find_iter = _chat_thread_items.find(thread_id);
    if (find_iter == _chat_thread_items.end()) {
        return;
    }

    //转为widget
    QWidget *widget = ui->chat_user_list->itemWidget(find_iter.value());
    if(!widget){
        return;
    }

    //判断转化为自定义的widget
    // 对自定义widget进行操作， 将item 转化为基类ListItemBase
    ListItemBase *customItem = qobject_cast<ListItemBase*>(widget);
    if(!customItem){
        qDebug()<< "[ChatDialog]: qobject_cast<ListItemBase*>(widget) is nullptr";
        return;
    }

    auto itemType = customItem->GetItemType();
    if(itemType == CHAT_USER_ITEM){
        auto con_item = qobject_cast<ChatUserWid*>(customItem);
        if(!con_item){
            return;
        }

        //设置信息
        auto chat_data = con_item->GetChatData();
        ui->chat_page->SetChatData(chat_data);

        return;
    }
}

//todo: 加载更多联系人，后期从数据库里添加
void ChatDialog::LoadMoreChatUser()
{
    // auto friend_list = UserMgr::GetInstance()->GetChatListPerPage();
    // if (friend_list.empty() == false) {
    //     for(auto & friend_ele : friend_list){
    //         auto find_iter = _chat_items_added.find(friend_ele->_uid);
    //         if(find_iter != _chat_items_added.end()){
    //             continue;
    //         }
    //         auto *chat_user_wid = new ChatUserWid();
    //         auto user_info = std::make_shared<UserInfo>(friend_ele);
    //         chat_user_wid->SetInfo(user_info);
    //         QListWidgetItem *item = new QListWidgetItem;
    //         //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    //         item->setSizeHint(chat_user_wid->sizeHint());
    //         ui->chat_user_list->addItem(item);
    //         ui->chat_user_list->setItemWidget(item, chat_user_wid);
    //         _chat_items_added.insert(friend_ele->_uid, item);
    //     }

    //     //更新已加载条目
    //     UserMgr::GetInstance()->UpdateChatLoadedCount();
    // }
}

void ChatDialog::LoadMoreConUser()
{
    auto friend_list = UserMgr::GetInstance()->GetConListPerPage();
    if (friend_list.empty() == false) {
        for(auto & friend_ele : friend_list){
            auto *chat_user_wid = new ConUserItem();
            chat_user_wid->SetInfo(friend_ele->_uid,friend_ele->_name,
                                   friend_ele->_icon);
            // 设置在线状态
            bool online = UserMgr::GetInstance()->IsFriendOnline(friend_ele->_uid);
            chat_user_wid->SetOnlineStatus(online);
            QListWidgetItem *item = new QListWidgetItem;
            //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
            item->setSizeHint(chat_user_wid->sizeHint());
            ui->con_user_list->addItem(item);
            ui->con_user_list->setItemWidget(item, chat_user_wid);
        }

        //更新已加载条目
        UserMgr::GetInstance()->UpdateContactLoadedCount();
    }

}

void ChatDialog::UpdateContactOnlineStatus(int uid, bool online)
{
    qDebug() << "[ChatDialog]: UpdateContactOnlineStatus, uid=" << uid << ", online=" << online;
    // 遍历好友列表，找到对应的 ConUserItem 并更新状态
    int count = ui->con_user_list->count();
    for (int i = 0; i < count; ++i) {
        QListWidgetItem* item = ui->con_user_list->item(i);
        if (!item) continue;
        ConUserItem* con_item = qobject_cast<ConUserItem*>(ui->con_user_list->itemWidget(item));
        if (!con_item) continue;
        // 跳过"新的朋友"等非联系人条目
        ListItemBase* base = qobject_cast<ListItemBase*>(con_item);
        if (!base || base->GetItemType() != ListItemType::CONTACT_USER_ITEM) continue;
        auto info = con_item->GetInfo();
        if (info && info->_uid == uid) {
            con_item->SetOnlineStatus(online);
            break;
        }
    }
}

void ChatDialog::UpdateAllContactOnlineStatus()
{
    // 遍历所有好友列表项，更新在线状态
    int count = ui->con_user_list->count();
    for (int i = 0; i < count; ++i) {
        QListWidgetItem* item = ui->con_user_list->item(i);
        if (!item) continue;
        ConUserItem* con_item = qobject_cast<ConUserItem*>(ui->con_user_list->itemWidget(item));
        if (!con_item) continue;
        // 跳过"新的朋友"等非联系人条目
        ListItemBase* base = qobject_cast<ListItemBase*>(con_item);
        if (!base || base->GetItemType() != ListItemType::CONTACT_USER_ITEM) continue;
        auto info = con_item->GetInfo();
        if (info) {
            bool online = UserMgr::GetInstance()->IsFriendOnline(info->_uid);
            con_item->SetOnlineStatus(online);
        }
    }
}

void ChatDialog::UpdateChatListItem(int thread_id)
{
    auto find_iter = _chat_thread_items.find(thread_id);
    if (find_iter == _chat_thread_items.end()) {
        return;
    }
    QWidget* widget = ui->chat_user_list->itemWidget(find_iter.value());
    if (!widget) return;
    auto* chat_wid = qobject_cast<ChatUserWid*>(widget);
    if (!chat_wid) return;
    auto chat_data = chat_wid->GetChatData();
    if (!chat_data) return;

    // 更新最后一条消息显示
    chat_wid->UpdateLastMsg(chat_data->GetLastMsg());
    // 更新未读计数角标
    chat_wid->UpdateUnreadCount(chat_data->GetUnreadCount());
}

void ChatDialog::UpdateChatMsg(std::vector<std::shared_ptr<TextChatData> > msgdata)
{
    for(auto & msg : msgdata){
        if (msg->GetThreadId() != _cur_chat_thread_id) {
            break;
        }

        ui->chat_page->AppendChatMsg(msg);
    }
}

void ChatDialog::showLoadingDlg(bool show)
{
    if (show) {
        // 如果已经显示了加载对话框，则不再创建新的
        if (_loading_dialog) {
            return;
        }
        
        // 显示加载对话框
        _loading_dialog = new LoadingDialog(this,"正在加载聊天列表...");
        _loading_dialog->setModal(true);
        _loading_dialog->show();
    } else {
        // 隐藏并清理加载对话框
        if (_loading_dialog) {
            _loading_dialog->deleteLater();
            _loading_dialog = nullptr;
        }
    }
}

void ChatDialog::LoadHeadIcon(QString avatarPath, QLabel *icon_label, QString file_name, QString req_type)
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
        qWarning() << "[ChatDialog]: 正在下载: " << file_name;
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

bool ChatDialog::eventFilter(QObject *watched, QEvent *event)
{
    if(event->type() == QEvent::MouseButtonPress){
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        handleGlobalMousePress(mouseEvent);
    }

    return QDialog::eventFilter(watched,event);
}

void ChatDialog::handleGlobalMousePress(QMouseEvent *event)
{
    // 实现点击位置的判断和处理逻辑
    // 先判断是否处于搜索模式，如果不处于搜索模式则直接返回
    if( _mode != ChatUIMode::SearchMode){
        return;
    }
    // 检查 ui->search_list 是否有效
    if (!ui->search_list) {
        return;
    }
    // 将鼠标点击位置转换为搜索列表坐标系中的位置
    QPoint posInSearchList = ui->search_list->mapFromGlobal(event->globalPos());
    // 判断点击位置是否在聊天列表的范围内
    if (!ui->search_list->rect().contains(posInSearchList)) {
        // 如果不在聊天列表内，清空输入框
        ui->search_edit->clear();
        ShowSearch(false);
    }
}

//显示搜索结果
void ChatDialog::ShowSearch(bool bsearch)
{
    if(bsearch){
        if (ui->chat_user_list) ui->chat_user_list->hide();
        if (ui->con_user_list) ui->con_user_list->hide();
        if (ui->ai_user_list) ui->ai_user_list->hide();
        if (ui->search_list) ui->search_list->show();
        _mode = ChatUIMode::SearchMode;
    }else if(_state == ChatUIMode::ChatMode){
        if (ui->chat_user_list) ui->chat_user_list->show();
        if (ui->con_user_list) ui->con_user_list->hide();
        if (ui->ai_user_list) ui->ai_user_list->hide();
        if (ui->search_list) ui->search_list->hide();
        _mode = ChatUIMode::ChatMode;
    }else if(_state == ChatUIMode::ContactMode){
        if (ui->chat_user_list) ui->chat_user_list->hide();
        if (ui->search_list) ui->search_list->hide();
        if (ui->ai_user_list) ui->ai_user_list->hide();
        if (ui->con_user_list) ui->con_user_list->show();
        _mode = ChatUIMode::ContactMode;
    }else if(_state == ChatUIMode::SettingsMode){
        if (ui->chat_user_list) ui->chat_user_list->hide();
        if (ui->search_list) ui->search_list->hide();
        if (ui->ai_user_list) ui->ai_user_list->hide();
        if (ui->con_user_list) ui->con_user_list->hide();
    }else if(_state == ChatUIMode::AIMode){
        if (ui->chat_user_list) ui->chat_user_list->hide();
        if (ui->con_user_list) ui->con_user_list->hide();
        if (ui->search_list) ui->search_list->hide();
        if (ui->ai_user_list) ui->ai_user_list->show();
        _mode = ChatUIMode::AIMode;
    }else if(_state == ChatUIMode::VideoMode){
        if (ui->chat_user_list) ui->chat_user_list->hide();
        if (ui->con_user_list) ui->con_user_list->hide();
        if (ui->search_list) ui->search_list->hide();
        if (ui->ai_user_list) ui->ai_user_list->hide();
        _mode = ChatUIMode::VideoMode;
    }
}

void ChatDialog::AddLBGroup(StateWidget *lb)
{
    _lb_list.push_back(lb);
}

//加载未显示的用户
void ChatDialog::slot_loading_chat_user()
{
    if(_b_loading || !ui->chat_user_list){
        return;
    }
    _b_loading = true;
    LoadingDialog *loadingDialog = new LoadingDialog(this);
    loadingDialog->setModal(true);
    loadingDialog->show();
    qDebug() << "[ChatDialog]: add new data to list.....";
    //加载用户
    LoadMoreChatUser();
    // 加载完成后关闭对话框
    loadingDialog->deleteLater();
    _b_loading = false;
}

void ChatDialog::slot_loading_contact_user()
{
    qDebug() << "[ChatDialog]: --- slot loading contact user ---";
    if(_b_loading){
        return;
    }

    _b_loading = true;
    LoadingDialog *loadingDialog = new LoadingDialog(this);
    loadingDialog->setModal(true);
    loadingDialog->show();
    qDebug() << "[ChatDialog]: add new data to list.....";
    //加载更多好友信息列表
    LoadMoreConUser();
    // 加载完成后关闭对话框
    loadingDialog->deleteLater();

    _b_loading = false;
}

//显示聊天页面
void ChatDialog::slot_side_chat()
{
    qDebug()<< "[ChatDialog]: --- receive side chat clicked ---";

    if (!ui->side_chat_lb) return;
    ClearLabelState(ui->side_chat_lb);
    if (!ui->stackedWidget || !ui->chat_page) return;

    if (_video_page) _video_page->stopAll();

    //显示聊天页面
    ui->stackedWidget->setCurrentWidget(ui->chat_page);
    _state = ChatUIMode::ChatMode;
    ShowSearch(false);
    //设置选中条目
    SetSelectChatItem(_cur_chat_thread_id);
    //更新聊天界面信息
    SetSelectChatPage(_cur_chat_thread_id);
}

//显示添加好友页面
void ChatDialog::slot_side_contact()
{
    qDebug()<< "[ChatDialog]: --- receive side contect clicked ---";

    if (!ui->side_contact_lb) return;
    ClearLabelState(ui->side_contact_lb);
    if (!ui->stackedWidget || !ui->friend_apply_page) return;

    if (_video_page) _video_page->stopAll();

    //显示添加好友页面
    if (_last_widget == nullptr) {
        ui->stackedWidget->setCurrentWidget(ui->friend_apply_page);
        _last_widget = ui->friend_apply_page;
    }
    else {
        ui->stackedWidget->setCurrentWidget(_last_widget);
    }
    _state = ChatUIMode::ContactMode;
    ShowSearch(false);
}

//显示设置个人信息页面
void ChatDialog::slot_side_settings()
{
    qDebug()<< "[ChatDialog]: --- receive side settings clicked ---";

    if (!ui->side_settings_lb) return;
    ClearLabelState(ui->side_settings_lb);
    if (!ui->stackedWidget || !ui->user_info_page) return;

    if (_video_page) _video_page->stopAll();

    //显示设置个人信息页面
    ui->stackedWidget->setCurrentWidget(ui->user_info_page);
    _state = ChatUIMode::SettingsMode;
    ShowSearch(false);
}

//显示工具页面
void ChatDialog::slot_side_tools()
{
    qDebug()<< "[ChatDialog]: --- receive side tools clicked ---";

    if (!ui->side_tool_lb) return;
    ClearLabelState(ui->side_tool_lb);
    if (!ui->stackedWidget) return;
    
    if (_video_page) _video_page->stopAll();

    // 工具页面是最后添加的之前添加了AIPage，所以是倒数第二个
    ui->stackedWidget->setCurrentWidget(ui->stackedWidget->widget(ui->stackedWidget->count() - 3));
    
    _state = ChatUIMode::SettingsMode; // 借用 SettingsMode 的隐藏逻辑
    ShowSearch(false);
}

//显示 AI 页面
void ChatDialog::slot_side_ai()
{
    qDebug()<< "[ChatDialog]: --- receive side ai clicked ---";

    if (!ui->side_ai_lb) return;
    ClearLabelState(ui->side_ai_lb);
    if (!ui->stackedWidget || !_ai_page) return;

    if (_video_page) _video_page->stopAll();

    ui->stackedWidget->setCurrentWidget(_ai_page);
    _state = ChatUIMode::AIMode;
    ShowSearch(false);

    // 选中当前的 AI 会话
    auto find_iter = _ai_thread_items.find(_cur_ai_thread_id);
    if (find_iter != _ai_thread_items.end()) {
        ui->ai_user_list->setCurrentItem(find_iter.value());
    }
}

//显示短视频页面
void ChatDialog::slot_side_video()
{
    qDebug()<< "[ChatDialog]: --- receive side video clicked ---";

    if (!ui->side_video_lb) return;
    ClearLabelState(ui->side_video_lb);
    if (!ui->stackedWidget || !_video_page) return;

    ui->stackedWidget->setCurrentWidget(_video_page);
    _state = ChatUIMode::VideoMode;
    ShowSearch(false);

    if (_video_page) _video_page->playCurrent();
}

//搜索添加好友
void ChatDialog::slot_text_changed(const QString& str)
{
    qDebug() << "[ChatDialog]: --- receive slot text changed str is " << str << " ---";
    if(!str.isEmpty()){
        ShowSearch(true);

        // 检查 ui->search_list 是否有效
        if (!ui->search_list) {
            return;
        }
        // 直接操作 ui->search_list 作为 QListWidget 添加 AddUserItem
        ui->search_list->clear();  // 清空列表
        // 创建并添加 AddUserItem 提示
        auto *add_user_item = new AddUserItem();
        QListWidgetItem *item = new QListWidgetItem();
        item->setSizeHint(add_user_item->sizeHint());  // 使用 AddUserItem 的推荐大小
        ui->search_list->addItem(item);
        ui->search_list->setItemWidget(item, add_user_item);
    }
}

void ChatDialog::slot_reset_head()
{
    //模拟加载自己头像
    QString head_icon = UserMgr::GetInstance()->GetIcon();

    // 先尝试从缓存获取
    QPixmap cachedPixmap = UserMgr::GetInstance()->GetCachedIcon(head_icon);
    if (!cachedPixmap.isNull()) {
        QPixmap scaledPixmap = cachedPixmap.scaled(ui->side_head_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->side_head_lb->setPixmap(scaledPixmap);
        ui->side_head_lb->setScaledContents(true);
        return;
    }

    //使用正则表达式检查是否使用默认头像
    QRegularExpression regex("^:/images/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(head_icon);
    if (match.hasMatch()) {
        // 如果是默认头像（:/images/head_X.jpg 格式）
        QPixmap pixmap(head_icon); // 加载默认头像图片
        UserMgr::GetInstance()->CacheIcon(head_icon, pixmap);
        QPixmap scaledPixmap = pixmap.scaled(ui->side_head_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->side_head_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
        ui->side_head_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
    }
    else {
        // 如果是用户上传的头像，获取存储目录
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        auto uid = UserMgr::GetInstance()->GetUid();
        QDir avatarsDir(storageDir + "/user/" + QString::number(uid) + "/avatars");
        auto file_name = QFileInfo(head_icon).fileName();

        // 确保目录存在
        if (avatarsDir.exists()) {
            QString avatarPath = avatarsDir.filePath(file_name); // 获取上传头像的完整路径
            QPixmap pixmap(avatarPath); // 加载上传的头像图片
            if (!pixmap.isNull()) {
                UserMgr::GetInstance()->CacheIcon(head_icon, pixmap);
                QPixmap scaledPixmap = pixmap.scaled(ui->side_head_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                ui->side_head_lb->setPixmap(scaledPixmap);
                ui->side_head_lb->setScaledContents(true);
            }
            else {
                qWarning() << "[ChatDialog]: 无法加载上传的头像：" << avatarPath;
                LoadHeadIcon(avatarPath, ui->side_head_lb, file_name, "self_icon");
            }
        }
        else {
            qWarning() << "[ChatDialog]: 头像存储目录不存在：" << avatarsDir.path();
            QString avatarPath = avatarsDir.filePath(QFileInfo(head_icon).fileName());
            avatarsDir.mkpath(".");
            LoadHeadIcon(avatarPath, ui->side_head_lb, file_name, "self_icon");
        }
    }
}

void ChatDialog::slot_show_more(QString name, ClickLbState state)
{
    if(_more_dialog){
        _more_dialog->close();
        return;
    }

    _more_dialog = new MoreDialog(this->window());
    connect(_more_dialog, &MoreDialog::switch_login, this, &ChatDialog::slot_switch_login);
    connect(_more_dialog, &QObject::destroyed, [this](){
        _more_dialog = nullptr;
        ui->more_lb->SetCurState(ClickLbState::Normal);
    });

    ui->more_lb->SetCurState(ClickLbState::Selected);

    // 设置位置
    QPoint pos = ui->more_lb->mapToGlobal(QPoint(0, 0));
    _more_dialog->adjustSize();
    _more_dialog->move(pos.x() + ui->more_lb->width() + 5, pos.y() - _more_dialog->height());
    _more_dialog->show();
}

void ChatDialog::slot_show_user_info(QString name, ClickLbState state)
{
    if(_user_info_dialog){
        _user_info_dialog->close();
        return;
    }

    _user_info_dialog = new UserInfoDialog(this->window());
    connect(_user_info_dialog, &QObject::destroyed, [this](){
        _user_info_dialog = nullptr;
        ui->side_head_lb->SetCurState(ClickLbState::Normal);
    });

    ui->side_head_lb->SetCurState(ClickLbState::Selected);

    // 设置位置
    QPoint pos = ui->side_head_lb->mapToGlobal(QPoint(0, 0));
    _user_info_dialog->adjustSize();
    _user_info_dialog->move(pos.x() + ui->side_head_lb->width() + 10, pos.y());
    _user_info_dialog->show();
}

void ChatDialog::slot_switch_login()
{
    emit switch_login();
}

void ChatDialog::slot_apply_friend(std::shared_ptr<AddFriendApply> apply)
{
    qDebug() << "[ChatDialog]: receive apply friend slot, applyuid is " << apply->_from_uid << " name is "
             << apply->_name << " desc is " << apply->_desc;

    //判断是否已经申请过了
    bool b_already = UserMgr::GetInstance()->AlreadyApply(apply->_from_uid);
    if(b_already){
        return;
    }

    //没申请过
    UserMgr::GetInstance()->AddApplyList(std::make_shared<ApplyInfo>(apply));
    ui->side_contact_lb->ShowRedPoint(true);
    ui->con_user_list->ShowRedPoint(true);
    ui->friend_apply_page->AddNewApply(apply);

}
//(已修改)
void ChatDialog::slot_add_auth_firend(std::shared_ptr<AuthInfo> auth_info)
{
    qDebug() << "[ChatDialog]: --- receive slot_add_auth__friend uid is " << auth_info->_uid
             << " name is " << auth_info->_name << " nick is " << auth_info->_nick << " ---";
    //判断如果已经是好友则跳过
    auto bfriend = UserMgr::GetInstance()->CheckFriendById(auth_info->_uid);
    if (bfriend) {
        return;
    }

    UserMgr::GetInstance()->AddFriend(auth_info);

    auto chat_thread_data = std::make_shared<ChatThreadData>(auth_info->_uid, auth_info->_thread_id, 0);
    UserMgr::GetInstance()->AddChatThreadData(chat_thread_data, auth_info->_uid);
    for (auto& chat_msg : auth_info->_chat_datas) {
        chat_thread_data->AppendMsg(chat_msg->GetMsgId(), chat_msg);
    }

    //判断好友列表是否已经有该item，有则直接返回
    auto iter = _chat_thread_items.find(auth_info->_thread_id);
    if(iter != _chat_thread_items.end()){
        return;
    }

    auto* chat_user_wid = new ChatUserWid();
    chat_user_wid->SetChatData(chat_thread_data);
    QListWidgetItem* item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(chat_user_wid->sizeHint());
    ui->chat_user_list->insertItem(0, item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);
    _chat_thread_items.insert(auth_info->_thread_id, item);
}

//(已修改)
void ChatDialog::slot_auth_rsp(std::shared_ptr<AuthRsp> auth_rsp)
{
    qDebug() << "[ChatDialog]: --- receive slot_auth_rsp uid is " << auth_rsp->_uid
             << " name is " << auth_rsp->_name << " nick is " << auth_rsp->_nick << " ---";
    //判断如果已经是好友则跳过
    auto bfriend = UserMgr::GetInstance()->CheckFriendById(auth_rsp->_uid);
    if (bfriend) {
        return;
    }
    //添加好友
    UserMgr::GetInstance()->AddFriend(auth_rsp);

    auto* chat_user_wid = new ChatUserWid();
    auto chat_thread_data = std::make_shared<ChatThreadData>(auth_rsp->_uid, auth_rsp->_thread_id, 0);

    UserMgr::GetInstance()->AddChatThreadData(chat_thread_data, auth_rsp->_uid);

    for (auto& chat_msg : auth_rsp->_chat_datas) {
        chat_thread_data->AppendMsg(chat_msg->GetMsgId(), chat_msg);
    }
    chat_user_wid->SetChatData(chat_thread_data);

    QListWidgetItem* item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(chat_user_wid->sizeHint());
    ui->chat_user_list->insertItem(0, item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);
    _chat_thread_items.insert(auth_rsp->_thread_id, item);
}

//todo: 点击搜索的联系人聊天，跳转到聊天界面，之后添加申请创建私聊或者查找已有 私聊消息
void ChatDialog::slot_jump_chat_item(std::shared_ptr<SearchInfo> si)
{
    qDebug() << "[ChatDialog]: --- slot jump chat item from SearchList---";

    //首先在聊天列表中查询
    auto chat_thread_data = UserMgr::GetInstance()->GetChatThreadByUid(si->_uid);
    if (chat_thread_data) {
        auto find_iter = _chat_thread_items.find(chat_thread_data->GetThreadId());
        if (find_iter != _chat_thread_items.end()) {
            qDebug() << "[ChatDialog]: jump to chat item , uid is " << si->_uid;
            ui->chat_user_list->scrollToItem(find_iter.value());
            ui->side_chat_lb->SetSelected(true);
            SetSelectChatItem(chat_thread_data->GetThreadId());
            //更新聊天界面信息
            SetSelectChatPage(chat_thread_data->GetThreadId());
            slot_side_chat();
            return;
        } //说明之前有缓存过聊天列表，只是被删除了，那么重新加进来即可
        else {
            auto* chat_user_wid = new ChatUserWid();
            chat_user_wid->SetChatData(chat_thread_data);
            QListWidgetItem* item = new QListWidgetItem;
            qDebug() << "[ChatDialog]: chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
            ui->chat_user_list->insertItem(0, item);
            ui->chat_user_list->setItemWidget(item, chat_user_wid);
            _chat_thread_items.insert(chat_thread_data->GetThreadId(), item);
            ui->side_chat_lb->SetSelected(true);
            SetSelectChatItem(chat_thread_data->GetThreadId());
            //更新聊天界面信息
            SetSelectChatPage(chat_thread_data->GetThreadId());
            slot_side_chat();
            return;
        }
    }

    //如果没找到，则发送创建请求
    auto uid = UserMgr::GetInstance()->GetUid();
    QJsonObject jsonObj;
    jsonObj["uid"] = uid;
    jsonObj["other_id"] = si->_uid;

    QJsonDocument doc(jsonObj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    //发送tcp请求给chat server
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_CREATE_PRIVATE_CHAT_REQ, jsonData);
}

//(已修改，根据thread_id加载聊天)
void ChatDialog::slot_jump_chat_item_from_infopage(std::shared_ptr<UserInfo> user_info)
{
    qDebug() << "[ChatDialog]: --- slot jump chat item from friendinfopage ---";

    auto chat_thread_data = UserMgr::GetInstance()->GetChatThreadByUid(user_info->_uid);
    if (chat_thread_data) {
        auto find_iter = _chat_thread_items.find(chat_thread_data->GetThreadId());
        //找到了
        if(find_iter != _chat_thread_items.end()){
            qDebug() << "[ChatDialog]: --- jump to chat item, uid is: " << user_info->_uid;
            ui->chat_user_list->scrollToItem(find_iter.value());
            ui->side_chat_lb->SetSelected(true);
            SetSelectChatItem(chat_thread_data->GetThreadId());
            //更新聊天界面信息
            SetSelectChatPage(chat_thread_data->GetThreadId());
            slot_side_chat();
            return;
        }//说明之前有缓存过聊天列表，只是被删除了，重新加进来即可
        else{
            auto* chat_user_wid = new ChatUserWid();
            chat_user_wid->SetChatData(chat_thread_data);
            QListWidgetItem* item = new QListWidgetItem;
            qDebug()<<"[ChatDialog]: chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
            item->setSizeHint(chat_user_wid->sizeHint());
            ui->chat_user_list->insertItem(0, item);
            ui->chat_user_list->setItemWidget(item, chat_user_wid);

            _chat_thread_items.insert(chat_thread_data->GetThreadId(), item);

            ui->side_chat_lb->SetSelected(true);
            SetSelectChatItem(chat_thread_data->GetThreadId());
            //更新聊天界面信息
            SetSelectChatPage(chat_thread_data->GetThreadId());
            slot_side_chat();
            return;
        }
    }

    //没找到，则发送创建请求
    auto uid = UserMgr::GetInstance()->GetUid();
    QJsonObject jsonObj;
    jsonObj["uid"] = uid;
    jsonObj["other_id"] = user_info->_uid;

    QJsonDocument doc(jsonObj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    //发送tcp请求给chatserver
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_CREATE_PRIVATE_CHAT_REQ,jsonData);
}

void ChatDialog::slot_friend_info_page(std::shared_ptr<UserInfo> user_info)
{
    qDebug()<<"[ChatDialog]: --- receive switch friend info page sig ---";
    _last_widget = ui->friend_info_page;
    ui->stackedWidget->setCurrentWidget(ui->friend_info_page);
    ui->friend_info_page->SetInfo(user_info);
}

void ChatDialog::slot_switch_apply_friend_page()
{
    qDebug()<<"[ChatDialog]: --- receive switch apply friend page sig ---";
    _last_widget = ui->friend_apply_page;
    ui->stackedWidget->setCurrentWidget(ui->friend_apply_page);
}

//（已修改）
void ChatDialog::slot_item_clicked(QListWidgetItem *item)
{
    QWidget *widget = ui->chat_user_list->itemWidget(item); // 获取自定义widget对象
    if(!widget){
        qDebug()<< "[ChatDialog]: *** slot item clicked widget is nullptr ***";
        return;
    }

    // 对自定义widget进行操作， 将item 转化为基类ListItemBase
    ListItemBase *customItem = qobject_cast<ListItemBase*>(widget);
    if(!customItem){
        qDebug()<< "[ChatDialog]: *** slot item clicked widget is nullptr ***";
        return;
    }

    auto itemType = customItem->GetItemType();
    if(itemType == ListItemType::INVALID_ITEM
        || itemType == ListItemType::GROUP_TIP_ITEM){
        qDebug()<< "[ChatDialog]: *** slot invalid item clicked ***";
        return;
    }


    if(itemType == ListItemType::CHAT_USER_ITEM){
        // 创建对话框，提示用户
        qDebug()<< "[ChatDialog]: --- contact user item clicked ---";

        auto chat_wid = qobject_cast<ChatUserWid*>(customItem);
        auto chat_data = chat_wid->GetChatData();

        //判断是否是群聊
        if (chat_data->IsGroup()) {
            qDebug()<< "[ChatDialog]: --- group chat item clicked ---";
        } else {
            qDebug()<< "[ChatDialog]: --- private chat item clicked ---";
        }

        // 清除未读计数
        chat_data->SetUnreadCount(0);
        chat_wid->UpdateUnreadCount(0);

        // 上报已读到服务端（通知对方消息已被阅读）
        {
            QJsonObject jsonObj;
            jsonObj["uid"] = UserMgr::GetInstance()->GetUid();
            jsonObj["thread_id"] = chat_data->GetThreadId();
            QJsonDocument doc(jsonObj);
            QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
            emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_MARK_MSG_READ_REQ, jsonData);
            qDebug() << "[ChatDialog]: sent mark msg read request, thread_id=" << chat_data->GetThreadId();
        }

        //跳转到聊天界面（ChatPage已支持群聊和私聊）
        ui->chat_page->SetChatData(chat_data);
        ui->stackedWidget->setCurrentWidget(ui->chat_page);
        _cur_chat_thread_id = chat_data->GetThreadId();
        return;
    }

}

// void ChatDialog::slot_append_send_chat_msg(std::shared_ptr<TextChatData> msgdata)
// {
//     if (_cur_chat_uid == 0) {
//         return;
//     }

//     auto find_iter = _chat_items_added.find(_cur_chat_uid);
//     if (find_iter == _chat_items_added.end()) {
//         return;
//     }

//     //转为widget
//     QWidget* widget = ui->chat_user_list->itemWidget(find_iter.value());
//     if (!widget) {
//         return;
//     }

//     //判断转化为自定义的widget
//     // 对自定义widget进行操作， 将item 转化为基类ListItemBase
//     ListItemBase* customItem = qobject_cast<ListItemBase*>(widget);
//     if (!customItem) {
//         qDebug() << "*** qobject_cast<ListItemBase*>(widget) is nullptr ***";
//         return;
//     }

//     auto itemType = customItem->GetItemType();
//     if (itemType == CHAT_USER_ITEM) {
//         auto con_item = qobject_cast<ChatUserWid*>(customItem);
//         if (!con_item) {
//             return;
//         }

//         //将用户自己发送的信息加入到用户信息中
//         auto user_info = con_item->GetUserInfo();
//         user_info->_chat_msgs.push_back(msgdata);
//         //将用户自己发送到消息加入到好友聊天信息中（存储两个人发的所有消息）
//         std::vector<std::shared_ptr<TextChatData>> msg_vec;
//         msg_vec.push_back(msgdata);
//         UserMgr::GetInstance()->AppendFriendChatMsg(_cur_chat_uid,msg_vec);
//         return;
//     }
// }

void ChatDialog::slot_text_chat_msg(std::vector<std::shared_ptr<TextChatData>> msglists)
{
    for (auto& msg : msglists) {
        //更新数据
        auto thread_id = msg->GetThreadId();
        auto thread_data = UserMgr::GetInstance()->GetChatThreadByThreadId(thread_id);
        //将该对方发来的消息加到会话列表中
        thread_data->AddMsg(msg);
        //若当前打开的会话不是该thread_id，增加未读计数
        if (_cur_chat_thread_id != thread_id) {
            thread_data->IncrementUnreadCount();
        } else {
            // 当前打开的页面直接追加显示
            ui->chat_page->AppendChatMsg(msg);
        }
        // 更新聊天列表中的显示（最后一条消息 + 未读角标）
        UpdateChatListItem(thread_id);
    }
}

void ChatDialog::slot_img_chat_msg(std::shared_ptr<ImgChatData> imgchat)
{
    //更新数据
    auto thread_id = imgchat->GetThreadId();
    auto thread_data = UserMgr::GetInstance()->GetChatThreadByThreadId(thread_id);
    thread_data->AddMsg(imgchat);
    if (_cur_chat_thread_id != thread_id) {
        thread_data->IncrementUnreadCount();
    } else {
        ui->chat_page->AppendOtherMsg(imgchat);
    }
    // 更新聊天列表中的显示
    UpdateChatListItem(thread_id);
}

void ChatDialog::slot_file_chat_msg(std::shared_ptr<FileChatData> file_chat)
{
    //更新数据
    auto thread_id = file_chat->GetThreadId();
    auto thread_data = UserMgr::GetInstance()->GetChatThreadByThreadId(thread_id);
    thread_data->AddMsg(file_chat);
    if (_cur_chat_thread_id != thread_id) {
        thread_data->IncrementUnreadCount();
    } else {
        ui->chat_page->AppendOtherMsg(file_chat);
    }
    // 更新聊天列表中的显示
    UpdateChatListItem(thread_id);
}

void ChatDialog::slot_load_chat_thread(bool load_more,int last_thread_id,
     std::vector<std::shared_ptr<ChatThreadInfo>> chat_threads)
{
    for (auto& cti : chat_threads) {
        //处理群聊
        if (cti->_type == "group") {
            // 以 _chat_thread_items 判重：UI item 已经存在就跳过
            if (_chat_thread_items.contains(cti->_thread_id)) {
                continue;
            }

            // 优先使用登录时 AppendGroupList 已加载的完整数据（含公告、图标等）
            auto group_data = UserMgr::GetInstance()->GetGroupChat(cti->_thread_id);
            if (!group_data) {
                // 登录响应中没有该群（理论上不应发生），兜底创建
                group_data = std::make_shared<ChatThreadData>();
                group_data->SetThreadId(cti->_thread_id);
                group_data->SetIsGroup(true);
                group_data->SetOtherId(0);
                group_data->SetGroupName(cti->_group_name);
                //添加到群聊管理
                UserMgr::GetInstance()->AddGroupChat(cti->_thread_id, group_data);
            } else {
                // 已有数据：确保群名以 ChatThreadInfo 中的为准（更新本地可能陈旧的名称）
                if (!cti->_group_name.isEmpty()) {
                    group_data->SetGroupName(cti->_group_name);
                }
            }

            //创建聊天列表项
            auto* chat_user_wid = new ChatUserWid();
            chat_user_wid->SetChatData(group_data);

            QListWidgetItem* item = new QListWidgetItem;
            item->setSizeHint(chat_user_wid->sizeHint());
            ui->chat_user_list->addItem(item);
            ui->chat_user_list->setItemWidget(item, chat_user_wid);
            _chat_thread_items.insert(cti->_thread_id, item);

            //发送获取群成员请求
            QJsonObject jsonObj;
            jsonObj["requester_uid"] = UserMgr::GetInstance()->GetUid();
            jsonObj["thread_id"] = cti->_thread_id;
            QJsonDocument doc(jsonObj);
            QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
            emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_GET_GROUP_MEMBERS_REQ, jsonData);

            continue;
        }

        //处理单聊
        auto uid = UserMgr::GetInstance()->GetUid();
        auto other_uid = 0;
        if (uid == cti->_user1_id) {
            other_uid = cti->_user2_id;
        }else {
            other_uid = cti->_user1_id;
        }

        auto chat_thread_data = std::make_shared<ChatThreadData>(other_uid, cti->_thread_id, 0);
        UserMgr::GetInstance()->AddChatThreadData(chat_thread_data, other_uid);

        auto* chat_user_wid = new ChatUserWid();
        chat_user_wid->SetChatData(chat_thread_data);

        QListWidgetItem* item = new QListWidgetItem;
        //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
        item->setSizeHint(chat_user_wid->sizeHint());
        ui->chat_user_list->addItem(item);
        ui->chat_user_list->setItemWidget(item, chat_user_wid);
        _chat_thread_items.insert(cti->_thread_id, item);
    }

    UserMgr::GetInstance()->SetLastChatThreadId(last_thread_id);

    if (load_more) {
        //发送请求逻辑
        QJsonObject jsonObj;
        auto uid = UserMgr::GetInstance()->GetUid();
        jsonObj["uid"] = uid;
        jsonObj["thread_id"] = last_thread_id;
        QJsonDocument doc(jsonObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
        //发送tcp请求给chat server
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_LOAD_CHAT_THREAD_REQ, jsonData);
        return;
    }

    //更新聊天界面信息
    SetSelectChatItem();
    SetSelectChatPage();
    showLoadingDlg(false);

    //继续加载聊天数据
    loadChatMsg();

    // 登录后首次加载完聊天线程，查询好友在线状态
    UserMgr::GetInstance()->RequestFriendOnlineStatus();

    // 登录后发送获取未读消息数请求
    {
        QJsonObject jsonObj;
        jsonObj["uid"] = UserMgr::GetInstance()->GetUid();
        QJsonDocument doc(jsonObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_GET_UNREAD_COUNTS_REQ, jsonData);
        qDebug() << "[ChatDialog]: sent get unread counts request";
    }

    // 登录后默认选中的第一个会话也上报已读
    if (_cur_chat_thread_id > 0) {
        QJsonObject markJson;
        markJson["uid"] = UserMgr::GetInstance()->GetUid();
        markJson["thread_id"] = _cur_chat_thread_id;
        QJsonDocument markDoc(markJson);
        QByteArray markData = markDoc.toJson(QJsonDocument::Compact);
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_MARK_MSG_READ_REQ, markData);
        qDebug() << "[ChatDialog]: sent mark msg read for default thread_id=" << _cur_chat_thread_id;
    }
}

void ChatDialog::slot_create_private_chat(int uid, int other_id, int thread_id)
{
            auto* chat_user_wid = new ChatUserWid();
            auto chat_thread_data = std::make_shared<ChatThreadData>(other_id, thread_id, 0);
            if(chat_thread_data == nullptr){
                return;
            }
            UserMgr::GetInstance()->AddChatThreadData(chat_thread_data, other_id);
    
            chat_user_wid->SetChatData(chat_thread_data);
    
            QListWidgetItem* item = new QListWidgetItem;
            item->setSizeHint(chat_user_wid->sizeHint());
            qDebug() << "[ChatDialog]: chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
            ui->chat_user_list->insertItem(0, item);
            ui->chat_user_list->setItemWidget(item, chat_user_wid);
            _chat_thread_items.insert(thread_id, item);
    
            ui->side_chat_lb->SetSelected(true);
            SetSelectChatItem(thread_id);
            //更新聊天界面信息
            SetSelectChatPage(thread_id);
            slot_side_chat();
            return;
    // auto chat_thread_data = std::make_shared<ChatThreadData>(other_id, thread_id, 0);
    // if (chat_thread_data == nullptr) {
    //     return;
    // }
    // UserMgr::GetInstance()->AddChatThreadData(chat_thread_data, other_id);

    // chat_user_wid->SetChatData(chat_thread_data);
    // QListWidgetItem* item = new QListWidgetItem;
    // item->setSizeHint(chat_user_wid->sizeHint());
    // qDebug() << "chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    // ui->chat_user_list->insertItem(0, item);
    // ui->chat_user_list->setItemWidget(item, chat_user_wid);
    // _chat_thread_items.insert(thread_id, item);

    // ui->side_chat_lb->SetSelected(true);
    // SetSelectChatItem(thread_id);
    // //更新聊天界面信息
    // SetSelectChatPage(thread_id);
    // slot_side_chat();
    // return;
}

void ChatDialog::slot_load_chat_msg(int thread_id, int last_msg_id, bool load_more,
                std::vector<std::shared_ptr<ChatDataBase> > chat_datas)
{
    // 安全检查：确保服务端返回的 thread_id 与当前请求的匹配
    if (!_cur_load_chat || _cur_load_chat->GetThreadId() != thread_id) {
        // thread_id 不匹配，可能是乱序响应，跳过
        qWarning() << "[ChatDialog]: slot_load_chat_msg thread_id mismatch, expected="
                   << (_cur_load_chat ? _cur_load_chat->GetThreadId() : -1)
                   << ", got=" << thread_id;
        return;
    }

    //设置最后的msg_id到内存，后续会加到本地数据库
    _cur_load_chat->SetLastMsgId(last_msg_id);
    //加载聊天信息
    for(auto& chat_msg : chat_datas){
        _cur_load_chat->AppendMsg(chat_msg->GetMsgId(),chat_msg);
    }

    // 更新聊天列表项的 UI（最后消息 + 未读角标）
    UpdateChatListItem(_cur_load_chat->GetThreadId());

    //还有未加载完的消息，就继续加载
    if(load_more){
        //发送请求给服务器
        QJsonObject jsonObj;
        jsonObj["thread_id"] = _cur_load_chat->GetThreadId();
        jsonObj["message_id"] = _cur_load_chat->GetLastMsgId();

        QJsonDocument doc(jsonObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

        //发送tcp请求给chatserver
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_LOAD_CHAT_MSG_REQ,jsonData);
        return;
    }

    //获取下一个chat_thread
    _cur_load_chat = UserMgr::GetInstance()->GetNextLoadData();
    //都加载完了
    if(!_cur_load_chat){
        //更新聊天界面信息
        SetSelectChatItem();
        SetSelectChatPage();
        showLoadingDlg(false);
        return;
    }

    //还没加载完->继续加载下一个聊天，发送请求给服务器
    //发送请求给服务器
    QJsonObject jsonObj;
    jsonObj["thread_id"] = _cur_load_chat->GetThreadId();
    jsonObj["message_id"] = _cur_load_chat->GetLastMsgId();

    QJsonDocument doc(jsonObj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    //发送tcp请求给chatserver
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_LOAD_CHAT_MSG_REQ,jsonData);
}

void ChatDialog::slot_add_chat_msg(int thread_id, std::vector<std::shared_ptr<TextChatData> > msglists)
{
    auto chat_data = UserMgr::GetInstance()->GetChatThreadByThreadId(thread_id);
    if (chat_data == nullptr) {
        return;
    }
    //将消息放入数据中管理
    for (auto& msg : msglists) {
        chat_data->MoveMsg(msg);
        if (_cur_chat_thread_id != thread_id) {
            continue;
        }
        //更新聊天界面信息
        ui->chat_page->UpdateChatStatus(msg);
    }
    // 更新聊天列表中的最后一条消息显示（自己发的也要更新）
    UpdateChatListItem(thread_id);
}

void ChatDialog::slot_add_img_msg(int thread_id, std::shared_ptr<ImgChatData> img_msg)
{
    auto chat_data = UserMgr::GetInstance()->GetChatThreadByThreadId(thread_id);
    if (chat_data == nullptr) {
        return;
    }

    chat_data->MoveMsg(img_msg);

    if (_cur_chat_thread_id != thread_id) {
        // 更新聊天列表显示
        UpdateChatListItem(thread_id);
        return;
    }

    //更新聊天界面信息
    ui->chat_page->UpdateImgChatStatus(img_msg);
    // 更新聊天列表中的最后一条消息显示
    UpdateChatListItem(thread_id);
}

void ChatDialog::slot_add_file_msg(int thread_id, std::shared_ptr<FileChatData> file_msg)
{
    auto chat_data = UserMgr::GetInstance()->GetChatThreadByThreadId(thread_id);
    if (chat_data == nullptr) {
        return;
    }

    chat_data->MoveMsg(file_msg);

    if (_cur_chat_thread_id != thread_id) {
        UpdateChatListItem(thread_id);
        return;
    }

    //更新聊天界面信息
    ui->chat_page->UpdateChatStatus(file_msg);
    // 更新聊天列表中的最后一条消息显示
    UpdateChatListItem(thread_id);
}

void ChatDialog::slot_file_transfer_failed(std::shared_ptr<MsgInfo> msg_info)
{
    auto chat_data = UserMgr::GetInstance()->GetChatThreadByThreadId(msg_info->_thread_id);
    if (chat_data == nullptr) {
        return;
    }

    //更新消息状态
    chat_data->UpdateProgress(msg_info);

    if (_cur_chat_thread_id != msg_info->_thread_id) {
        return;
    }

    //更新聊天界面信息
    ui->chat_page->FileTransferFailed(msg_info);
}

void ChatDialog::slot_reset_icon(QString path)
{
    UserMgr::GetInstance()->ResetLabelIcon(path);
}

void ChatDialog::slot_update_upload_progress(std::shared_ptr<MsgInfo> msg_info) {
    auto chat_data = UserMgr::GetInstance()->GetChatThreadByThreadId(msg_info->_thread_id);
    if (chat_data == nullptr) {
        return;
    }

    //更新消息，其实不用更新，都是共享msg_info的一块内存，这里为了安全还是再次更新下
    chat_data->UpdateProgress(msg_info);

    if (_cur_chat_thread_id != msg_info->_thread_id) {
        return;
    }

    //更新聊天界面信息
    ui->chat_page->UpdateFileProgress(msg_info);
}

void ChatDialog::slot_update_download_progress(std::shared_ptr<MsgInfo> msg_info)
{
    auto chat_data = UserMgr::GetInstance()->GetChatThreadByThreadId(msg_info->_thread_id);
    if (chat_data == nullptr) {
        return;
    }

    //更新消息，其实不用更新，都是共享msg_info的一块内存，这里为了安全还是再次更新下
    chat_data->UpdateProgress(msg_info);

    if (_cur_chat_thread_id != msg_info->_thread_id) {
        return;
    }

    //更新聊天界面信息
    ui->chat_page->UpdateFileProgress(msg_info);
}

void ChatDialog::slot_download_finish(std::shared_ptr<MsgInfo> msg_info, QString file_path)
{
    auto chat_data = UserMgr::GetInstance()->GetChatThreadByThreadId(msg_info->_thread_id);
    if (chat_data == nullptr) {
        return;
    }

    //更新消息，其实不用更新，都是共享msg_info的一块内存，这里为了安全还是再次更新下
    chat_data->UpdateProgress(msg_info);

    if (_cur_chat_thread_id != msg_info->_thread_id) {
        return;
    }

    //更新聊天界面信息
    ui->chat_page->DownloadFileFinished(msg_info, file_path);
}

void ChatDialog::slot_group_chat_created(int thread_id, const QString& group_name)
{
    qDebug() << "[ChatDialog]: --- slot_group_chat_created thread_id:" << thread_id << "group_name:" << group_name;

    //获取群聊数据
    auto group_data = UserMgr::GetInstance()->GetGroupChat(thread_id);
    if (group_data == nullptr) {
        qDebug() << "[ChatDialog]: *** group data is nullptr ***";
        return;
    }

    //检查是否已经在聊天列表中
    auto iter = _chat_thread_items.find(thread_id);
    if (iter != _chat_thread_items.end()) {
        qDebug() << "[ChatDialog]: group chat already exists in list";
        return;
    }

    //创建聊天列表项
    auto* chat_user_wid = new ChatUserWid();
    chat_user_wid->SetChatData(group_data);

    QListWidgetItem* item = new QListWidgetItem;
    item->setSizeHint(chat_user_wid->sizeHint());
    ui->chat_user_list->insertItem(0, item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);
    _chat_thread_items.insert(thread_id, item);

    //切换到聊天列表
    ui->side_chat_lb->SetSelected(true);
    slot_side_chat();

    //选中新创建的群聊
    SetSelectChatItem(thread_id);
    SetSelectChatPage(thread_id);
}


