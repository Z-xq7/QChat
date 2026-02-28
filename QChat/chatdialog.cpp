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

ChatDialog::ChatDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ChatDialog),_mode(ChatUIMode::ChatMode),_state(ChatUIMode::ChatMode)
    ,_b_loading(false),_cur_chat_uid(0),_last_widget(nullptr)
{
    ui->setupUi(this);

/*******************未知原因search_list和con_user_list和friend_apply_page没有正确提升构建类型********************/
    // // 现在重新设置 search_list 为实际的 SearchList
    // // 删除之前生成的 QListWidget，创建 SearchList
    // if (ui->chat_user_wid && ui->chat_user_wid->layout()) {
    //     SearchList* actualSearchList = new SearchList(this);
    //     actualSearchList->setObjectName("search_list");
    //     // 找到 search_list 在布局中的位置，用 SearchList 替换它
    //     QLayoutItem* item = ui->chat_user_wid->layout()->replaceWidget(ui->search_list, actualSearchList);
    //     delete ui->search_list;  // 删除原来的
    //     ui->search_list = actualSearchList;  // 指向新的
    // }

    // // 现在重新设置 con_user_list 为实际的 ContactUserList
    // if (ui->chat_user_wid && ui->chat_user_wid->layout()) {
    //     ContactUserList* actualContactList = new ContactUserList(this);
    //     actualContactList->setObjectName("con_user_list");
    //     // 找到con_user_list在布局中的位置，用ContactUserList替换它
    //     QLayoutItem* item2 = ui->chat_user_wid->layout()->replaceWidget(ui->con_user_list,actualContactList);
    //     delete ui->con_user_list;  // 删除原来的
    //     ui->con_user_list = actualContactList;  // 指向新的
    // }

    // // 现在重新设置 friend_apply_page 为实际的 ApplyFriendPage
    // ApplyFriendPage* actualApplyFriendPage = new ApplyFriendPage(this);
    // actualApplyFriendPage->setObjectName("friend_apply_page");
    // // 找到friend_apply_page在stackedWidget中的位置，用ApplyFriendPage替换它
    // // 先获取当前在stackedWidget中的索引
    // int index = ui->stackedWidget->indexOf(ui->friend_apply_page);
    // // 移除原来的widget
    // if (index >= 0) {  // 确保索引有效
    //     ui->stackedWidget->removeWidget(ui->friend_apply_page);
    //     // 删除原来的widget
    //     delete ui->friend_apply_page;
    //     // 添加新的ApplyFriendPage
    //     ui->stackedWidget->insertWidget(index, actualApplyFriendPage);
    //     // 更新指针指向新的ApplyFriendPage
    //     ui->friend_apply_page = actualApplyFriendPage;  // 注意：这里可能需要更新成员变量类型
    // }
/*********************************************************************************************************/

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
    ui->search_edit->SetMaxLength(15);

    //连接加载聊天列表信号和槽
    connect(ui->chat_user_list,&ChatUserList::sig_loading_chat_user,this,&ChatDialog::slot_loading_chat_user);

    //连接加载好友信息列表信号和槽
    connect(ui->con_user_list,&ContactUserList::sig_loading_contact_user,this,&ChatDialog::slot_loading_contact_user);

    //加载好友聊天列表
    addChatUserList();

    //获取个人信息
    auto self_info = UserMgr::GetInstance()->GetUserInfo();

    // 设置头像
    if (!self_info->_icon.isEmpty()) {
        // 如果有头像路径，则使用它
        QPixmap head_pix(self_info->_icon);
        // 如果图片加载成功，设置图片
        head_pix = head_pix.scaled(ui->side_head_lb->size(),
                Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->side_head_lb->setPixmap(head_pix);
    } else {
        // 如果没有头像路径，使用默认头像
        // 获取当前应用程序的路径
        QString app_path = QCoreApplication::applicationDirPath();
        QString pix_path = QDir::toNativeSeparators(app_path +
                                                    QDir::separator() + "static"+QDir::separator()+"head_1.jpg");
        QPixmap head_pix(pix_path);
        head_pix = head_pix.scaled(ui->side_head_lb->size(),
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->side_head_lb->setPixmap(head_pix);
    }

    //加载侧边栏
    ui->side_chat_lb->setProperty("state","normal");
    ui->side_chat_lb->SetState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");
    ui->side_contact_lb->setProperty("state","normal");
    ui->side_contact_lb->SetState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");

    AddLBGroup(ui->side_contact_lb);
    AddLBGroup(ui->side_chat_lb);

    //连接侧边栏点击后页面变化的槽函数
    connect(ui->side_chat_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_chat);
    connect(ui->side_contact_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_contact);

    //连接搜索框输入变化
    connect(ui->search_edit,&QLineEdit::textChanged,this,&ChatDialog::slot_text_changed);

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

    //连接聊天列表点击信号
    connect(ui->chat_user_list,&QListWidget::itemClicked,this,&ChatDialog::slot_item_clicked);

    //连接聊天信息缓存在本地
    connect(ui->chat_page,&ChatPage::sig_append_send_chat_msg,this,&ChatDialog::slot_append_send_chat_msg);

    //连接对方发来消息通知
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_text_chat_msg,this,&ChatDialog::slot_text_chat_msg);

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
}

ChatDialog::~ChatDialog()
{
    _timer->start(10000);
    delete ui;
}

//测试：添加随机用户列表
void ChatDialog::addChatUserList()
{
    //先按照好友列表加载聊天记录，等以后客户端实现聊天记录数据库之后再按照最后信息排序
    auto friend_list = UserMgr::GetInstance()->GetChatListPerPage();
    if (friend_list.empty() == false) {
        for(auto & friend_ele : friend_list){
            auto find_iter = _chat_items_added.find(friend_ele->_uid);
            if(find_iter != _chat_items_added.end()){
                continue;
            }
            auto *chat_user_wid = new ChatUserWid();
            auto user_info = std::make_shared<UserInfo>(friend_ele);
            chat_user_wid->SetInfo(user_info);
            QListWidgetItem *item = new QListWidgetItem;
            //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
            item->setSizeHint(chat_user_wid->sizeHint());
            ui->chat_user_list->addItem(item);
            ui->chat_user_list->setItemWidget(item, chat_user_wid);
            _chat_items_added.insert(friend_ele->_uid, item);
        }

        //更新已加载条目
        UserMgr::GetInstance()->UpdateChatLoadedCount();
    }

    // // 创建QListWidgetItem，并设置自定义的widget
    // for(int i = 0; i < 13; i++){
    //     int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
    //     int str_i = randomValue%strs.size();
    //     int head_i = randomValue%heads.size();
    //     int name_i = randomValue%names.size();

    //     auto user_info = std::make_shared<UserInfo>(0,names[name_i],names[name_i],heads[head_i],0,strs[str_i]);
    //     auto *chat_user_wid = new ChatUserWid();
    //     chat_user_wid->SetInfo(user_info);
    //     QListWidgetItem *item = new QListWidgetItem;
    //     //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    //     item->setSizeHint(chat_user_wid->sizeHint());
    //     ui->chat_user_list->addItem(item);
    //     ui->chat_user_list->setItemWidget(item, chat_user_wid);
    // }
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

void ChatDialog::SetSelectChatItem(int uid)
{
    //没有好友则什么都不做
    if(ui->chat_user_list->count() <= 0){
        return;
    }

    //uid为0，默认取出第一个好友信息
    if(uid == 0){
        ui->chat_user_list->setCurrentRow(0);
        QListWidgetItem *firstItem = ui->chat_user_list->item(0);
        if(!firstItem){
            return;
        }

        //转为widget
        QWidget *widget = ui->chat_user_list->itemWidget(firstItem);
        if(!widget){
            return;
        }

        //根据点击的聊天列表的item获取对方user信息
        auto con_item = qobject_cast<ChatUserWid*>(widget);
        if(!con_item){
            return;
        }
        _cur_chat_uid = con_item->GetUserInfo()->_uid;

        return;
    }

    //从当前聊天中查找该用户
    auto find_iter = _chat_items_added.find(uid);
    //没找到->将该聊天添加到最上面
    if(find_iter == _chat_items_added.end()){
        qDebug() << "uid " <<uid<< " not found, set curent row 0";
        ui->chat_user_list->setCurrentRow(0);
        return;
    }
    //找到了
    ui->chat_user_list->setCurrentItem(find_iter.value());

    _cur_chat_uid = uid;
}

void ChatDialog::SetSelectChatPage(int uid)
{
    //没有好友则什么都不做
    if( ui->chat_user_list->count() <= 0){
        return;
    }

    //uid为0，默认取出第一个好友信息
    if (uid == 0) {
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
        auto user_info = con_item->GetUserInfo();
        ui->chat_page->SetUserInfo(user_info);
        return;
    }

    auto find_iter = _chat_items_added.find(uid);
    if(find_iter == _chat_items_added.end()){
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
        qDebug()<< "qobject_cast<ListItemBase*>(widget) is nullptr";
        return;
    }

    auto itemType = customItem->GetItemType();
    if(itemType == CHAT_USER_ITEM){
        auto con_item = qobject_cast<ChatUserWid*>(customItem);
        if(!con_item){
            return;
        }

        //设置信息
        auto user_info = con_item->GetUserInfo();
        ui->chat_page->SetUserInfo(user_info);

        return;
    }
}

void ChatDialog::LoadMoreChatUser()
{
    auto friend_list = UserMgr::GetInstance()->GetChatListPerPage();
    if (friend_list.empty() == false) {
        for(auto & friend_ele : friend_list){
            auto find_iter = _chat_items_added.find(friend_ele->_uid);
            if(find_iter != _chat_items_added.end()){
                continue;
            }
            auto *chat_user_wid = new ChatUserWid();
            auto user_info = std::make_shared<UserInfo>(friend_ele);
            chat_user_wid->SetInfo(user_info);
            QListWidgetItem *item = new QListWidgetItem;
            //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
            item->setSizeHint(chat_user_wid->sizeHint());
            ui->chat_user_list->addItem(item);
            ui->chat_user_list->setItemWidget(item, chat_user_wid);
            _chat_items_added.insert(friend_ele->_uid, item);
        }

        //更新已加载条目
        UserMgr::GetInstance()->UpdateChatLoadedCount();
    }
}

void ChatDialog::LoadMoreConUser()
{
    auto friend_list = UserMgr::GetInstance()->GetConListPerPage();
    if (friend_list.empty() == false) {
        for(auto & friend_ele : friend_list){
            auto *chat_user_wid = new ConUserItem();
            chat_user_wid->SetInfo(friend_ele->_uid,friend_ele->_name,
                                   friend_ele->_icon);
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

void ChatDialog::UpdateChatMsg(std::vector<std::shared_ptr<TextChatData> > msgdata)
{
    for(auto & msg : msgdata){
        if(msg->_from_uid != _cur_chat_uid){
            break;
        }

        ui->chat_page->AppendChatMsg(msg);
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
        if (ui->search_list) ui->search_list->show();
        _mode = ChatUIMode::SearchMode;
    }else if(_state == ChatUIMode::ChatMode){
        if (ui->chat_user_list) ui->chat_user_list->show();
        if (ui->con_user_list) ui->con_user_list->hide();
        if (ui->search_list) ui->search_list->hide();
        _mode = ChatUIMode::ChatMode;
    }else if(_state == ChatUIMode::ContactMode){
        if (ui->chat_user_list) ui->chat_user_list->hide();
        if (ui->search_list) ui->search_list->hide();
        if (ui->con_user_list) ui->con_user_list->show();
        _mode = ChatUIMode::ContactMode;
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
    qDebug() << "add new data to list.....";
    //加载用户
    LoadMoreChatUser();
    // 加载完成后关闭对话框
    loadingDialog->deleteLater();
    _b_loading = false;
}

void ChatDialog::slot_loading_contact_user()
{
    qDebug() << "--- slot loading contact user ---";
    if(_b_loading){
        return;
    }

    _b_loading = true;
    LoadingDialog *loadingDialog = new LoadingDialog(this);
    loadingDialog->setModal(true);
    loadingDialog->show();
    qDebug() << "add new data to list.....";
    //加载更多好友信息列表
    LoadMoreConUser();
    // 加载完成后关闭对话框
    loadingDialog->deleteLater();

    _b_loading = false;
}

//显示聊天页面
void ChatDialog::slot_side_chat()
{
    qDebug()<< "--- receive side chat clicked ---";

    if (!ui->side_chat_lb) return;
    ClearLabelState(ui->side_chat_lb);
    if (!ui->stackedWidget || !ui->chat_page) return;
    //显示聊天页面
    ui->stackedWidget->setCurrentWidget(ui->chat_page);
    _state = ChatUIMode::ChatMode;
    ShowSearch(false);
}

//显示添加好友页面
void ChatDialog::slot_side_contact()
{
    qDebug()<< "--- receive side contect clicked ---";

    if (!ui->side_contact_lb) return;
    ClearLabelState(ui->side_contact_lb);
    if (!ui->stackedWidget || !ui->friend_apply_page) return;
    //显示添加好友页面
    ui->stackedWidget->setCurrentWidget(ui->friend_apply_page);
    _state = ChatUIMode::ContactMode;
    ShowSearch(false);
}

//搜索添加好友
void ChatDialog::slot_text_changed(const QString& str)
{
    qDebug() << "--- receive slot text changed str is " << str << " ---";
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

void ChatDialog::slot_apply_friend(std::shared_ptr<AddFriendApply> apply)
{
    qDebug() << "ChatDialog: receive apply friend slot, applyuid is " << apply->_from_uid << " name is "
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

void ChatDialog::slot_add_auth_firend(std::shared_ptr<AuthInfo> auth_info)
{
    qDebug() << "--- ChatDialog: slot add auth friend, uid is " << auth_info->_uid << " ---";

    //判断好友是否已经在好友列表中了
    bool isFriend = UserMgr::GetInstance()->CheckFriendById(auth_info->_uid);
    if(isFriend){
        return;
    }
    //添加好友信息
    UserMgr::GetInstance()->AddFriend(auth_info);

    // 在 groupitem 之后插入新项
    auto *chat_user_wid = new ChatUserWid();
    auto user_info = std::make_shared<UserInfo>(auth_info);
    //设置好友的用户信息
    chat_user_wid->SetInfo(user_info);

    QListWidgetItem *item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(chat_user_wid->sizeHint());
    ui->chat_user_list->insertItem(0,item);
    ui->chat_user_list->setItemWidget(item,chat_user_wid);
    _chat_items_added.insert(auth_info->_uid,item);
}

void ChatDialog::slot_auth_rsp(std::shared_ptr<AuthRsp> auth_rsp)
{
    qDebug() << "--- ChatDialog: slot auth rsp called, uid is " << auth_rsp->_uid << " ---";

    //判断好友是否已经在好友列表中了
    bool isFriend = UserMgr::GetInstance()->CheckFriendById(auth_rsp->_uid);
    if(isFriend){
        return;
    }
    //添加好友信息
    UserMgr::GetInstance()->AddFriend(auth_rsp);

    // 在 groupitem 之后插入新项
    auto *chat_user_wid = new ChatUserWid();
    auto user_info = std::make_shared<UserInfo>(auth_rsp);
    //设置好友的用户信息
    chat_user_wid->SetInfo(user_info);

    QListWidgetItem *item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(chat_user_wid->sizeHint());
    ui->chat_user_list->insertItem(0,item);
    ui->chat_user_list->setItemWidget(item,chat_user_wid);
    _chat_items_added.insert(auth_rsp->_uid,item);
}

void ChatDialog::slot_jump_chat_item(std::shared_ptr<SearchInfo> si)
{
    qDebug() << "--- slot jump chat item from SearchList---";

    //首先在聊天列表中查询
    auto find_iter = _chat_items_added.find(si->_uid);
    if(find_iter != _chat_items_added.end()){
        qDebug() << "--- jump to chat item , uid is " << si->_uid << " ---";
        ui->chat_user_list->scrollToItem(find_iter.value());
        ui->side_chat_lb->SetSelected(true);
        //更新聊天列表信息
        SetSelectChatItem(si->_uid);
        //更新聊天界面信息
        SetSelectChatPage(si->_uid);
        //显示聊天页面
        slot_side_chat();
        return;
    }

    //如果没找到，则创建新的插入listwidget
    auto* chat_user_wid = new ChatUserWid();
    auto user_info = std::make_shared<UserInfo>(si);
    chat_user_wid->SetInfo(user_info);
    QListWidgetItem* item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(chat_user_wid->sizeHint());
    ui->chat_user_list->insertItem(0, item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);

    _chat_items_added.insert(si->_uid, item);

    ui->side_chat_lb->SetSelected(true);
    //更新聊天列表信息
    SetSelectChatItem(si->_uid);
    //更新聊天界面信息
    SetSelectChatPage(si->_uid);
    //显示聊天页面
    slot_side_chat();
}

void ChatDialog::slot_jump_chat_item_from_infopage(std::shared_ptr<UserInfo> user_info)
{
    qDebug() << "--- slot jump chat item from friendinfopage ---";

    //如果已经在聊天列表中了->直接跳转
    auto find_iter = _chat_items_added.find(user_info->_uid);
    if(find_iter != _chat_items_added.end()){
        qDebug() << "jump to chat item , uid is " << user_info->_uid;
        ui->chat_user_list->scrollToItem(find_iter.value());
        ui->side_chat_lb->SetSelected(true);
        SetSelectChatItem(user_info->_uid);
        //更新聊天界面信息
        SetSelectChatPage(user_info->_uid);
        slot_side_chat();
        return;
    }

    //如果没找到，则创建新的插入listwidget
    auto* chat_user_wid = new ChatUserWid();
    chat_user_wid->SetInfo(user_info);
    QListWidgetItem* item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(chat_user_wid->sizeHint());
    ui->chat_user_list->insertItem(0, item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);

    _chat_items_added.insert(user_info->_uid, item);

    ui->side_chat_lb->SetSelected(true);
    SetSelectChatItem(user_info->_uid);
    //更新聊天界面信息
    SetSelectChatPage(user_info->_uid);
    slot_side_chat();
}

void ChatDialog::slot_friend_info_page(std::shared_ptr<UserInfo> user_info)
{
    qDebug()<<"--- receive switch friend info page sig ---";
    _last_widget = ui->friend_info_page;
    ui->stackedWidget->setCurrentWidget(ui->friend_info_page);
    ui->friend_info_page->SetInfo(user_info);
}

void ChatDialog::slot_switch_apply_friend_page()
{
    qDebug()<<"--- receive switch apply friend page sig ---";
    _last_widget = ui->friend_apply_page;
    ui->stackedWidget->setCurrentWidget(ui->friend_apply_page);
}

void ChatDialog::slot_item_clicked(QListWidgetItem *item)
{
    QWidget *widget = ui->chat_user_list->itemWidget(item); // 获取自定义widget对象
    if(!widget){
        qDebug()<< "*** slot item clicked widget is nullptr ***";
        return;
    }

    // 对自定义widget进行操作， 将item 转化为基类ListItemBase
    ListItemBase *customItem = qobject_cast<ListItemBase*>(widget);
    if(!customItem){
        qDebug()<< "*** slot item clicked widget is nullptr ***";
        return;
    }

    auto itemType = customItem->GetItemType();
    if(itemType == ListItemType::INVALID_ITEM
        || itemType == ListItemType::GROUP_TIP_ITEM){
        qDebug()<< "*** slot invalid item clicked ***";
        return;
    }


    if(itemType == ListItemType::CHAT_USER_ITEM){
        // 创建对话框，提示用户
        qDebug()<< "--- contact user item clicked ---";

        auto chat_wid = qobject_cast<ChatUserWid*>(customItem);
        auto user_info = chat_wid->GetUserInfo();
        //跳转到聊天界面
        ui->chat_page->SetUserInfo(user_info);
        _cur_chat_uid = user_info->_uid;
        return;
    }

}

void ChatDialog::slot_append_send_chat_msg(std::shared_ptr<TextChatData> msgdata)
{
    if (_cur_chat_uid == 0) {
        return;
    }

    auto find_iter = _chat_items_added.find(_cur_chat_uid);
    if (find_iter == _chat_items_added.end()) {
        return;
    }

    //转为widget
    QWidget* widget = ui->chat_user_list->itemWidget(find_iter.value());
    if (!widget) {
        return;
    }

    //判断转化为自定义的widget
    // 对自定义widget进行操作， 将item 转化为基类ListItemBase
    ListItemBase* customItem = qobject_cast<ListItemBase*>(widget);
    if (!customItem) {
        qDebug() << "*** qobject_cast<ListItemBase*>(widget) is nullptr ***";
        return;
    }

    auto itemType = customItem->GetItemType();
    if (itemType == CHAT_USER_ITEM) {
        auto con_item = qobject_cast<ChatUserWid*>(customItem);
        if (!con_item) {
            return;
        }

        //将用户自己发送的信息加入到用户信息中
        auto user_info = con_item->GetUserInfo();
        user_info->_chat_msgs.push_back(msgdata);
        //将用户自己发送到消息加入到好友聊天信息中（存储两个人发的所有消息）
        std::vector<std::shared_ptr<TextChatData>> msg_vec;
        msg_vec.push_back(msgdata);
        UserMgr::GetInstance()->AppendFriendChatMsg(_cur_chat_uid,msg_vec);
        return;
    }
}

void ChatDialog::slot_text_chat_msg(std::shared_ptr<TextChatMsg> msg)
{
    auto find_iter = _chat_items_added.find(msg->_from_uid);
    //找到聊天列表
    if(find_iter != _chat_items_added.end()){
        qDebug() << "--- set chat item msg, uid is " << msg->_from_uid << " ---";
        QWidget *widget = ui->chat_user_list->itemWidget(find_iter.value());
        auto chat_wid = qobject_cast<ChatUserWid*>(widget);
        if(!chat_wid){
            return;
        }
        chat_wid->updateLastMsg(msg->_chat_msgs);
        //更新当前聊天页面记录
        UpdateChatMsg(msg->_chat_msgs);
        //收到信息后将发送来的信息加入到聊天信息中
        UserMgr::GetInstance()->AppendFriendChatMsg(msg->_from_uid,msg->_chat_msgs);
        return;
    }

    //如果没找到，则创建新的插入listwidget
    auto* chat_user_wid = new ChatUserWid();
    //查询好友信息
    auto fi_ptr = UserMgr::GetInstance()->GetFriendById(msg->_from_uid);
    chat_user_wid->SetInfo(fi_ptr);
    QListWidgetItem* item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(chat_user_wid->sizeHint());
    chat_user_wid->updateLastMsg(msg->_chat_msgs);
    //收到信息后将发送来的信息加入到聊天信息中
    UserMgr::GetInstance()->AppendFriendChatMsg(msg->_from_uid,msg->_chat_msgs);
    ui->chat_user_list->insertItem(0, item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);
    _chat_items_added.insert(msg->_from_uid, item);

}



