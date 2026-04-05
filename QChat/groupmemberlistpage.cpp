#include "groupmemberlistpage.h"
#include "usermgr.h"
#include <QStyleOption>
#include <QPainter>

// ==================== GroupMemberItem ====================

GroupMemberItem::GroupMemberItem(QWidget *parent) : QWidget(parent)
    , m_iconLabel(nullptr)
    , m_nameLabel(nullptr)
    , m_roleLabel(nullptr)
{
    setFixedHeight(60);
    setCursor(Qt::PointingHandCursor);
    
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(15, 5, 15, 5);
    layout->setSpacing(10);

    // 头像
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(40, 40);
    m_iconLabel->setStyleSheet("background-color: #e0e0e0; border-radius: 20px;");
    m_iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_iconLabel);

    // 名称
    m_nameLabel = new QLabel(this);
    m_nameLabel->setStyleSheet("font-size: 14px; color: #333333;");
    layout->addWidget(m_nameLabel);

    layout->addStretch();

    // 角色标签
    m_roleLabel = new QLabel(this);
    m_roleLabel->setStyleSheet(
        "font-size: 11px; color: #ff6600; background-color: #fff0e0; "
        "padding: 2px 8px; border-radius: 3px;"
    );
    layout->addWidget(m_roleLabel);
}

void GroupMemberItem::SetMemberData(const std::shared_ptr<GroupMemberInfo>& member)
{
    _member = member;
    if (!_member) return;

    // 设置名称
    m_nameLabel->setText(_member->_name);

    // 设置角色标签
    QString roleText;
    switch (_member->_role) {
        case 2:
            roleText = "群主";
            m_roleLabel->setStyleSheet(
                "font-size: 11px; color: #ff6600; background-color: #fff0e0; "
                "padding: 2px 8px; border-radius: 3px;"
            );
            break;
        case 1:
            roleText = "管理员";
            m_roleLabel->setStyleSheet(
                "font-size: 11px; color: #0099ff; background-color: #e6f5ff; "
                "padding: 2px 8px; border-radius: 3px;"
            );
            break;
        default:
            roleText = "";
            break;
    }
    m_roleLabel->setText(roleText);
    m_roleLabel->setVisible(!roleText.isEmpty());

    // TODO: 加载头像
    // m_iconLabel->setPixmap(...);
}

void GroupMemberItem::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (_member) {
        emit sig_item_clicked(_member->_uid);
    }
}

// ==================== GroupMemberListPage ====================

GroupMemberListPage::GroupMemberListPage(int thread_id, QWidget *parent)
    : QWidget(parent)
    , _thread_id(thread_id)
    , _current_role(0)
    , m_searchEdit(nullptr)
    , m_memberList(nullptr)
    , m_titleLabel(nullptr)
    , m_inviteBtn(nullptr)
    , m_removeBtn(nullptr)
{
    initUi();
    
    // 获取当前用户角色
    auto group_data = UserMgr::GetInstance()->GetGroupChat(thread_id);
    if (group_data) {
        int uid = UserMgr::GetInstance()->GetUid();
        for (const auto& member : group_data->GetGroupMembers()) {
            if (member->_uid == uid) {
                _current_role = member->_role;
                break;
            }
        }
    }
}

void GroupMemberListPage::initUi()
{
    this->setObjectName("GroupMemberListPage");
    this->setStyleSheet("#GroupMemberListPage { background-color: #f5f5f5; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 顶部标题栏
    QWidget* headerWidget = new QWidget(this);
    headerWidget->setFixedHeight(50);
    headerWidget->setStyleSheet("background-color: #ffffff; border-bottom: 1px solid #e0e0e0;");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(10, 0, 10, 0);

    // 返回按钮
    QPushButton* backBtn = new QPushButton("<", headerWidget);
    backBtn->setFixedSize(30, 30);
    backBtn->setStyleSheet(
        "QPushButton { background-color: transparent; border: none; font-size: 18px; color: #333333; }"
        "QPushButton:hover { color: #0099ff; }"
    );
    connect(backBtn, &QPushButton::clicked, this, &GroupMemberListPage::on_back_clicked);
    headerLayout->addWidget(backBtn);

    // 标题
    m_titleLabel = new QLabel("群聊成员", headerWidget);
    m_titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #333333;");
    headerLayout->addWidget(m_titleLabel);

    headerLayout->addStretch();

    // 邀请按钮
    m_inviteBtn = new QPushButton("+ 邀请", headerWidget);
    m_inviteBtn->setFixedSize(60, 30);
    m_inviteBtn->setStyleSheet(
        "QPushButton { background-color: #0099ff; color: white; border-radius: 15px; "
        "font-size: 12px; border: none; }"
        "QPushButton:hover { background-color: #007acc; }"
    );
    connect(m_inviteBtn, &QPushButton::clicked, this, &GroupMemberListPage::on_invite_clicked);
    headerLayout->addWidget(m_inviteBtn);

    mainLayout->addWidget(headerWidget);

    // 搜索框
    QWidget* searchWidget = new QWidget(this);
    searchWidget->setFixedHeight(60);
    searchWidget->setStyleSheet("background-color: #ffffff;");
    QHBoxLayout* searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(15, 10, 15, 10);

    m_searchEdit = new QLineEdit(searchWidget);
    m_searchEdit->setPlaceholderText("搜索");
    m_searchEdit->setFixedHeight(35);
    m_searchEdit->setStyleSheet(
        "QLineEdit { background-color: #f5f5f5; border: 1px solid #e0e0e0; "
        "border-radius: 17px; padding-left: 15px; font-size: 13px; }"
        "QLineEdit:focus { border-color: #0099ff; }"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &GroupMemberListPage::on_search_text_changed);
    searchLayout->addWidget(m_searchEdit);

    mainLayout->addWidget(searchWidget);

    // 成员列表
    m_memberList = new QListWidget(this);
    m_memberList->setStyleSheet(
        "QListWidget { background-color: #ffffff; border: none; outline: none; }"
        "QListWidget::item { border-bottom: 1px solid #f0f0f0; }"
        "QListWidget::item:hover { background-color: #f5f5f5; }"
        "QListWidget::item:selected { background-color: #e6f5ff; }"
    );
    m_memberList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_memberList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainLayout->addWidget(m_memberList);

    // 底部操作栏 (仅管理员/群主显示移除按钮)
    QWidget* footerWidget = new QWidget(this);
    footerWidget->setFixedHeight(60);
    footerWidget->setStyleSheet("background-color: #ffffff; border-top: 1px solid #e0e0e0;");
    QHBoxLayout* footerLayout = new QHBoxLayout(footerWidget);
    footerLayout->setContentsMargins(15, 10, 15, 10);

    m_removeBtn = new QPushButton("移除成员", footerWidget);
    m_removeBtn->setFixedHeight(40);
    m_removeBtn->setStyleSheet(
        "QPushButton { background-color: #ff4d4d; color: white; border-radius: 5px; "
        "font-size: 14px; border: none; }"
        "QPushButton:hover { background-color: #ff3333; }"
        "QPushButton:disabled { background-color: #cccccc; }"
    );
    m_removeBtn->setVisible(false);  // 默认隐藏，根据角色显示
    footerLayout->addWidget(m_removeBtn);

    footerLayout->addStretch();

    mainLayout->addWidget(footerWidget);
}

void GroupMemberListPage::SetMembers(const std::vector<std::shared_ptr<GroupMemberInfo>>& members)
{
    _all_members = members;
    _filtered_members = members;
    
    // 更新标题
    if (m_titleLabel) {
        m_titleLabel->setText(QString("群聊成员 %1").arg(members.size()));
    }
    
    updateMemberList();
}

void GroupMemberListPage::updateMemberList()
{
    m_memberList->clear();

    for (const auto& member : _filtered_members) {
        QListWidgetItem* item = new QListWidgetItem(m_memberList);
        item->setSizeHint(QSize(m_memberList->viewport()->width() - 5, 60));

        GroupMemberItem* memberItem = new GroupMemberItem(m_memberList);
        memberItem->SetMemberData(member);
        connect(memberItem, &GroupMemberItem::sig_item_clicked, 
                this, &GroupMemberListPage::on_member_item_clicked);

        m_memberList->addItem(item);
        m_memberList->setItemWidget(item, memberItem);
    }
}

void GroupMemberListPage::filterMembers(const QString& keyword)
{
    _filtered_members.clear();

    if (keyword.isEmpty()) {
        _filtered_members = _all_members;
    } else {
        for (const auto& member : _all_members) {
            QString name = member->_name;
            if (name.contains(keyword, Qt::CaseInsensitive)) {
                _filtered_members.push_back(member);
            }
        }
    }

    updateMemberList();
}

void GroupMemberListPage::on_search_text_changed(const QString& text)
{
    filterMembers(text);
}

void GroupMemberListPage::on_back_clicked()
{
    emit sig_back();
}

void GroupMemberListPage::on_invite_clicked()
{
    emit sig_invite_friend(_thread_id);
}

void GroupMemberListPage::on_member_item_clicked(int uid)
{
    emit sig_member_clicked(uid);
}

//让GroupMemberListPage具备样式表渲染能力。
//因为纯QWidget的默认paintEvent是空实现，无法识别样式表，
//而这段代码通过调用 Qt 的样式系统绘制控件的基础外观，从而让样式表生效。
void GroupMemberListPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
