#include "groupchatdialog.h"
#include "usermgr.h"
#include <QIcon>
#include <QDebug>
#include <QPainter>
#include <QStyleOption>
#include <QScrollBar>

GroupChatDialog::GroupChatDialog(QWidget *parent)
    : QDialog(parent)
{
    initUi();
    loadFriends();
}

GroupChatDialog::~GroupChatDialog()
{
}

void GroupChatDialog::initUi()
{
    this->setFixedSize(650, 500);
    this->setWindowTitle("发起群聊");
    this->setObjectName("GroupChatDialog");
    this->setStyleSheet("QDialog#GroupChatDialog { background-color: #ffffff; border-radius: 10px; }");

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- 左侧区域 ---
    QWidget* leftWidget = new QWidget(this);
    leftWidget->setFixedWidth(320);
    leftWidget->setStyleSheet("QWidget { background-color: #f7f7f7; border-right: 1px solid #e0e0e0; }");
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(15, 20, 15, 20);
    leftLayout->setSpacing(10);

    // 搜索框
    m_searchEdit = new QLineEdit(leftWidget);
    m_searchEdit->setPlaceholderText("搜索");
    m_searchEdit->setFixedHeight(32);
    m_searchEdit->setStyleSheet("QLineEdit { background-color: #e2e2e2; border-radius: 5px; padding-left: 10px; border: none; }");
    leftLayout->addWidget(m_searchEdit);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &GroupChatDialog::on_search_text_changed);

    // 好友树
    m_friendsTree = new QTreeWidget(leftWidget);
    m_friendsTree->setHeaderHidden(true);
    m_friendsTree->setIndentation(15);
    m_friendsTree->setFocusPolicy(Qt::NoFocus);
    m_friendsTree->setStyleSheet(
        "QTreeWidget { background-color: transparent; border: none; }"
        "QTreeWidget::item { height: 45px; }"
    );
    leftLayout->addWidget(m_friendsTree);
    connect(m_friendsTree, &QTreeWidget::itemChanged, this, &GroupChatDialog::on_item_checked);

    m_recentGroup = new QTreeWidgetItem(m_friendsTree);
    m_recentGroup->setText(0, "最近聊天");
    m_recentGroup->setExpanded(true);

    m_friendsGroup = new QTreeWidgetItem(m_friendsTree);
    m_friendsGroup->setText(0, "我的好友");
    m_friendsGroup->setExpanded(true);

    // --- 右侧区域 ---
    QWidget* rightWidget = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(20, 20, 20, 20);
    rightLayout->setSpacing(10);

    // 标题栏
    QHBoxLayout* titleLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel("选择联系人", rightWidget);
    titleLabel->setStyleSheet("font-size: 14px; color: #333333; font-weight: bold;");
    m_countLabel = new QLabel("已选 0 个联系人", rightWidget);
    m_countLabel->setStyleSheet("font-size: 12px; color: #999999;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_countLabel);
    rightLayout->addLayout(titleLayout);

    // 已选列表
    m_selectedList = new QListWidget(rightWidget);
    m_selectedList->setStyleSheet("QListWidget { background-color: transparent; border: none; }");
    m_selectedList->setFocusPolicy(Qt::NoFocus);
    rightLayout->addWidget(m_selectedList);

    // 底部按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_cancelBtn = new QPushButton("取消", rightWidget);
    m_cancelBtn->setFixedSize(80, 32);
    m_cancelBtn->setStyleSheet("QPushButton { background-color: #f0f0f0; border-radius: 5px; color: #333333; border: none; }");
    
    m_confirmBtn = new QPushButton("确定", rightWidget);
    m_confirmBtn->setFixedSize(80, 32);
    m_confirmBtn->setStyleSheet("QPushButton { background-color: #00c0ff; border-radius: 5px; color: #ffffff; border: none; }");
    
    btnLayout->addWidget(m_confirmBtn);
    btnLayout->addWidget(m_cancelBtn);
    rightLayout->addLayout(btnLayout);

    mainLayout->addWidget(leftWidget);
    mainLayout->addWidget(rightWidget);

    connect(m_confirmBtn, &QPushButton::clicked, this, &GroupChatDialog::on_confirm_clicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &GroupChatDialog::on_cancel_clicked);
}

void GroupChatDialog::loadFriends()
{
    // 加载所有好友
    auto friends = UserMgr::GetInstance()->GetAllFriends();
    for(auto& friend_info : friends) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_friendsGroup);
        item->setText(0, friend_info->_name);
        
        // 尝试从缓存获取头像，如果没有则使用默认
        QPixmap icon = UserMgr::GetInstance()->GetCachedIcon(friend_info->_icon);
        if(icon.isNull()) {
            icon = QPixmap(":/images/head_default.png");
        }
        item->setIcon(0, QIcon(icon));
        
        item->setCheckState(0, Qt::Unchecked);
        item->setData(0, Qt::UserRole, friend_info->_uid);
        m_uidToTreeItem[friend_info->_uid] = item;
    }

    // 加载最近聊天
    auto recent_threads = UserMgr::GetInstance()->GetAllChatThreadData();
    for(auto& thread : recent_threads) {
        int other_uid = thread->GetOtherId();
        if(other_uid == 0) continue; // 跳过群聊

        auto friend_info = UserMgr::GetInstance()->GetFriendById(other_uid);
        if(!friend_info) continue;

        QTreeWidgetItem* item = new QTreeWidgetItem(m_recentGroup);
        item->setText(0, friend_info->_name);
        
        QPixmap icon = UserMgr::GetInstance()->GetCachedIcon(friend_info->_icon);
        if(icon.isNull()) {
            icon = QPixmap(":/images/head_default.png");
        }
        item->setIcon(0, QIcon(icon));
        
        item->setCheckState(0, Qt::Unchecked);
        item->setData(0, Qt::UserRole, friend_info->_uid);
        // 最近聊天中的 item 变化也需要同步到好友列表（反之亦然），这里简单处理
    }
}

void GroupChatDialog::on_search_text_changed(const QString& text)
{
    // 搜索逻辑
    for (int i = 0; i < m_friendsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* topItem = m_friendsTree->topLevelItem(i);
        bool anyVisible = false;
        for (int j = 0; j < topItem->childCount(); ++j) {
            QTreeWidgetItem* child = topItem->child(j);
            if (child->text(0).contains(text, Qt::CaseInsensitive)) {
                child->setHidden(false);
                anyVisible = true;
            } else {
                child->setHidden(true);
            }
        }
        topItem->setHidden(!anyVisible && !text.isEmpty());
    }
}

void GroupChatDialog::on_item_checked(QTreeWidgetItem* item, int column)
{
    if (!item || item->parent() == nullptr) return;

    int uid = item->data(0, Qt::UserRole).toInt();
    Qt::CheckState state = item->checkState(0);

    // 同步其他分组中相同好友的状态
    m_friendsTree->blockSignals(true);
    for (int i = 0; i < m_friendsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* topItem = m_friendsTree->topLevelItem(i);
        for (int j = 0; j < topItem->childCount(); ++j) {
            QTreeWidgetItem* child = topItem->child(j);
            if (child != item && child->data(0, Qt::UserRole).toInt() == uid) {
                child->setCheckState(0, state);
            }
        }
    }
    m_friendsTree->blockSignals(false);

    if (state == Qt::Checked) {
        if (!m_selectedFriends.contains(uid)) {
            auto user = UserMgr::GetInstance()->GetFriendById(uid);
            if (user) {
                m_selectedFriends[uid] = user;
                addSelectedFriend(user);
            }
        }
    } else {
        if (m_selectedFriends.contains(uid)) {
            m_selectedFriends.remove(uid);
            removeSelectedFriend(uid);
        }
    }
    updateSelectedCount();
}

void GroupChatDialog::addSelectedFriend(std::shared_ptr<UserInfo> user)
{
    QListWidgetItem* item = new QListWidgetItem(m_selectedList);
    item->setSizeHint(QSize(0, 50));
    SelectedFriendItem* widget = new SelectedFriendItem(user, m_selectedList);
    connect(widget, &SelectedFriendItem::sig_remove, this, &GroupChatDialog::on_remove_selected_item);
    m_selectedList->addItem(item);
    m_selectedList->setItemWidget(item, widget);
}

void GroupChatDialog::removeSelectedFriend(int uid)
{
    for (int i = 0; i < m_selectedList->count(); ++i) {
        QListWidgetItem* item = m_selectedList->item(i);
        SelectedFriendItem* widget = qobject_cast<SelectedFriendItem*>(m_selectedList->itemWidget(item));
        if (widget && widget->property("uid").toInt() == uid) {
            delete m_selectedList->takeItem(i);
            break;
        }
    }
}

void GroupChatDialog::on_remove_selected_item(int uid)
{
    // 取消勾选树中所有匹配该 uid 的节点（处理重复出现在不同分组的情况）
    for (int i = 0; i < m_friendsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* topItem = m_friendsTree->topLevelItem(i);
        for (int j = 0; j < topItem->childCount(); ++j) {
            QTreeWidgetItem* child = topItem->child(j);
            if (child->data(0, Qt::UserRole).toInt() == uid) {
                // 暂时阻塞信号，防止循环触发
                m_friendsTree->blockSignals(true);
                child->setCheckState(0, Qt::Unchecked);
                m_friendsTree->blockSignals(false);
            }
        }
    }
    
    // 手动调用移除逻辑，因为信号被阻塞了
    if (m_selectedFriends.contains(uid)) {
        m_selectedFriends.remove(uid);
        removeSelectedFriend(uid);
        updateSelectedCount();
    }
}

void GroupChatDialog::updateSelectedCount()
{
    m_countLabel->setText(QString("已选 %1 个联系人").arg(m_selectedFriends.size()));
}

void GroupChatDialog::on_confirm_clicked()
{
    // 检查是否选择了至少一个好友
    if (m_selectedFriends.size() < 1) {
        qDebug() << "[GroupChatDialog]: Please select at least one friend";
        return;
    }

    // 构建群聊名称（使用创建者和前几个成员的名字）
    auto self_info = UserMgr::GetInstance()->GetUserInfo();
    QString group_name = self_info->_name;
    
    int count = 0;
    for (auto it = m_selectedFriends.begin(); it != m_selectedFriends.end() && count < 2; ++it, ++count) {
        group_name += ", " + it.value()->_name;
    }
    if (m_selectedFriends.size() > 2) {
        group_name += "...";
    }
    group_name += " 的群聊";

    // 收集成员uid列表
    QVector<int> member_uids;
    for (auto it = m_selectedFriends.begin(); it != m_selectedFriends.end(); ++it) {
        member_uids.append(it.key());
    }

    qDebug() << "[GroupChatDialog]: Creating group chat - name:" << group_name << ", members:" << member_uids;

    // 发送创建群聊信号
    emit sig_create_group_chat(group_name, member_uids);
    
    this->accept();
}

void GroupChatDialog::on_cancel_clicked()
{
    this->reject();
}

// --- SelectedFriendItem 实现 ---
SelectedFriendItem::SelectedFriendItem(std::shared_ptr<UserInfo> user, QWidget* parent)
    : QWidget(parent), m_uid(user->_uid)
{
    this->setProperty("uid", m_uid);
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 10, 5);

    QLabel* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(35, 35);
    QPixmap pixmap(user->_icon);
    iconLabel->setPixmap(pixmap.scaled(35, 35, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setStyleSheet("border-radius: 17px;");

    QLabel* nameLabel = new QLabel(user->_name, this);
    nameLabel->setStyleSheet("font-size: 13px; color: #333333;");

    QPushButton* removeBtn = new QPushButton(this);
    removeBtn->setFixedSize(20, 20);
    removeBtn->setStyleSheet("QPushButton { border-image: url(:/images/close2.png); border: none; }");
    connect(removeBtn, &QPushButton::clicked, this, [this](){
        emit sig_remove(m_uid);
    });

    layout->addWidget(iconLabel);
    layout->addWidget(nameLabel);
    layout->addStretch();
    layout->addWidget(removeBtn);
}
