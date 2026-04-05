#include "groupsidepage.h"
#include "groupnoticedialog.h"
#include <QScrollArea>
#include <QStyleOption>
#include <QPainter>
#include <QPaintEvent>
#include "usermgr.h"

GroupSidePage::GroupSidePage(QWidget *parent) : QWidget(parent)
    , _current_role(0)
    , m_groupNameLabel(nullptr)
    , m_groupIdLabel(nullptr)
    , m_memberCountLabel(nullptr)
    , m_noticeBtn(nullptr)
    , m_nicknameBtn(nullptr)
    , m_dismissExitBtn(nullptr)
{
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    connect(m_searchTimer, &QTimer::timeout, this, &GroupSidePage::on_search_timeout);
    initUi();
}

void GroupSidePage::initUi()
{
    this->setObjectName("GroupSidePage");
    this->setFixedWidth(280);
    this->setStyleSheet("#GroupSidePage { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                        "stop:0 rgba(230,250,255, 150), "
                        "stop:1 rgba(250,235,255, 150)); }");


    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 创建滚动区域
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->viewport()->setStyleSheet("background-color: #f5f5f5;");
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background-color: #f5f5f5; }"
        "QScrollBar:vertical { background-color: transparent; width: 6px; margin: 0px; }"
        "QScrollBar::handle:vertical { background-color: #c0c0c0; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background-color: #a0a0a0; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background-color: transparent; }"
    );

    // 创建内容容器
    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                                 "stop:0 rgba(230,250,255, 150), "
                                 "stop:1 rgba(250,235,255, 150));");
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(8, 15, 8, 15); // 稍微缩小边距
    contentLayout->setSpacing(15);

    // 1. 群聊信息区域
    contentLayout->addWidget(createGroupInfoSection());

    // 2. 群成员区域
    contentLayout->addWidget(createMembersSection());

    // 3. 设置区域
    contentLayout->addWidget(createSettingsSection());

    // 4. 管理区域
    contentLayout->addWidget(createManageSection());

    contentLayout->addStretch();

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
}

QWidget* GroupSidePage::createGroupInfoSection()
{
    QWidget* section = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // 群聊头像和名称
    QWidget* headerWidget = new QWidget(section);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(7); // 增加间距

    // 群聊头像 (使用多个头像拼接的效果)
    QLabel* iconLabel = new QLabel(headerWidget);
    iconLabel->setFixedSize(50, 50);
    iconLabel->setStyleSheet("background-color: #e0e0e0; border-radius: 5px;");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setText("👥");
    QFont font = iconLabel->font();
    font.setPointSize(20);
    iconLabel->setFont(font);
    headerLayout->addWidget(iconLabel);

    // 群聊名称和ID
    QVBoxLayout* nameLayout = new QVBoxLayout();
    nameLayout->setSpacing(2); // 减小行间距
    nameLayout->setContentsMargins(0, 0, 0, 0);
    
    m_groupNameLabel = new QLabel(headerWidget);
    m_groupNameLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #333333;");
    m_groupNameLabel->setText("群聊名称");
    // 增加最大宽度到 180，确保能显示更多文字
    m_groupNameLabel->setMaximumWidth(180); 
    m_groupNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_groupNameLabel->setWordWrap(true); 
    nameLayout->addWidget(m_groupNameLabel);

    m_groupIdLabel = new QLabel(headerWidget);
    m_groupIdLabel->setStyleSheet("font-size: 12px; color: #999999;");
    m_groupIdLabel->setText("🔒 群号: 0");
    nameLayout->addWidget(m_groupIdLabel);

    // 将名称布局添加到主布局，并设置伸缩因子为 1，使其占据中间所有空间
    headerLayout->addLayout(nameLayout, 1);

    // 分享按钮
    QPushButton* shareBtn = new QPushButton("分享", headerWidget);
    shareBtn->setFixedSize(50, 26);
    shareBtn->setStyleSheet(
        "QPushButton { background-color: #ffffff; border: 1px solid #dcdcdc; "
        "border-radius: 13px; font-size: 11px; color: #666666; }"
        "QPushButton:hover { background-color: #f0f0f0; }"
    );
    headerLayout->addWidget(shareBtn);

    layout->addWidget(headerWidget);

    return section;
}

QWidget* GroupSidePage::createMembersSection()
{
    QWidget* section = new QWidget(this);
    section->setStyleSheet("background-color: #ffffff; border-radius: 8px;");
    QVBoxLayout* layout = new QVBoxLayout(section);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 标题行
    QWidget* titleWidget = new QWidget(section);
    QHBoxLayout* titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(5); // 限制间距

    QLabel* titleLabel = new QLabel("群聊成员", titleWidget);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #333333;");
    titleLayout->addWidget(titleLabel);

    titleLayout->addStretch();

    // 成员数量显示（纯文本，不可点击）
    m_memberCountLabel = new QLabel(titleWidget);
    m_memberCountLabel->setStyleSheet("font-size: 12px; color: #999999;");
    m_memberCountLabel->setText("当前0名群成员");
    // 限制宽度防止溢出
    m_memberCountLabel->setMaximumWidth(110); 
    titleLayout->addWidget(m_memberCountLabel);

    layout->addWidget(titleWidget);

    // 成员头像列表
    QWidget* membersWidget = new QWidget(section);
    QHBoxLayout* membersLayout = new QHBoxLayout(membersWidget);
    membersLayout->setContentsMargins(0, 5, 0, 5);
    membersLayout->setSpacing(10);

    // 成员头像容器（用于动态显示成员头像）
    m_membersContainer = new QWidget(membersWidget);
    m_membersLayout = new QHBoxLayout(m_membersContainer);
    m_membersLayout->setContentsMargins(0, 0, 0, 0);
    m_membersLayout->setSpacing(10);
    membersLayout->addWidget(m_membersContainer);

    // 添加按钮 (+)
    QPushButton* addBtn = new QPushButton(membersWidget);
    addBtn->setFixedSize(40, 40);
    addBtn->setText("+");
    QFont btnFont = addBtn->font();
    btnFont.setPointSize(18);
    addBtn->setFont(btnFont);
    addBtn->setStyleSheet(
        "QPushButton { background-color: #f5f5f5; border: 1px dashed #cccccc; "
        "border-radius: 20px; color: #999999; }"
        "QPushButton:hover { background-color: #e8e8e8; }"
    );
    connect(addBtn, &QPushButton::clicked, this, &GroupSidePage::on_invite_clicked);
    membersLayout->addWidget(addBtn);

    // 移除按钮 (-) - 仅管理员/群主可见
    QPushButton* removeBtn = new QPushButton(membersWidget);
    removeBtn->setFixedSize(40, 40);
    removeBtn->setText("-");
    removeBtn->setFont(btnFont);
    removeBtn->setStyleSheet(
        "QPushButton { background-color: #f5f5f5; border: 1px dashed #cccccc; "
        "border-radius: 20px; color: #999999; }"
        "QPushButton:hover { background-color: #e8e8e8; }"
    );
    removeBtn->setObjectName("removeMemberBtn");
    membersLayout->addWidget(removeBtn);

    membersLayout->addStretch();
    layout->addWidget(membersWidget);

    // 点击查看全部成员
    QPushButton* viewAllBtn = new QPushButton(section);
    viewAllBtn->setCursor(Qt::PointingHandCursor);
    viewAllBtn->setStyleSheet(
        "QPushButton { background-color: transparent; border: none; text-align: center; color: #666666; }"
        "QPushButton:hover { color: #00c0ff; }"
    );
    viewAllBtn->setText("查看全部成员 >");
    connect(viewAllBtn, &QPushButton::clicked, this, &GroupSidePage::on_view_members_clicked);
    layout->addWidget(viewAllBtn);

    return section;
}

QWidget* GroupSidePage::createSettingsSection()
{
    QWidget* section = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // 群资料设置
    QPushButton* profileBtn = new QPushButton("群资料设置", section);
    profileBtn->setFixedHeight(45);
    profileBtn->setStyleSheet(
        "QPushButton { background-color: #ffffff; border-radius: 5px; font-size: 14px; color: #333333; text-align: left; padding-left: 15px; }"
        "QPushButton:hover { background-color: #f0f0f0; }"
    );
    profileBtn->setCursor(Qt::PointingHandCursor);
    connect(profileBtn, &QPushButton::clicked, this, &GroupSidePage::on_profile_settings_clicked);
    layout->addWidget(profileBtn);

    // 群公告
    QWidget* noticeContainer = new QWidget(section);
    QVBoxLayout* noticeContainerLayout = new QVBoxLayout(noticeContainer);
    noticeContainerLayout->setContentsMargins(0, 0, 0, 0);
    noticeContainerLayout->setSpacing(5);

    QLabel* noticeTitle = new QLabel("群公告", noticeContainer);
    noticeTitle->setStyleSheet("font-size: 12px; color: #999999;");
    noticeContainerLayout->addWidget(noticeTitle);

    m_noticeBtn = new QPushButton("未设置", noticeContainer);
    m_noticeBtn->setFixedHeight(45);
    m_noticeBtn->setStyleSheet(
        "QPushButton { background-color: #ffffff; border-radius: 5px; font-size: 14px; color: #333333; text-align: left; padding-left: 15px; }"
        "QPushButton:hover { background-color: #f0f0f0; }"
    );
    m_noticeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_noticeBtn, &QPushButton::clicked, this, &GroupSidePage::on_notice_clicked);
    noticeContainerLayout->addWidget(m_noticeBtn);

    layout->addWidget(noticeContainer);

    // 我的本群昵称
    QWidget* nicknameContainer = new QWidget(section);
    QVBoxLayout* nicknameContainerLayout = new QVBoxLayout(nicknameContainer);
    nicknameContainerLayout->setContentsMargins(0, 0, 0, 0);
    nicknameContainerLayout->setSpacing(5);

    QLabel* nicknameTitle = new QLabel("我的本群昵称", nicknameContainer);
    nicknameTitle->setStyleSheet("font-size: 12px; color: #999999;");
    nicknameContainerLayout->addWidget(nicknameTitle);

    m_nicknameBtn = new QPushButton("未设置", nicknameContainer);
    m_nicknameBtn->setFixedHeight(45);
    m_nicknameBtn->setStyleSheet(
        "QPushButton { background-color: #ffffff; border-radius: 5px; font-size: 14px; color: #333333; text-align: left; padding-left: 15px; }"
        "QPushButton:hover { background-color: #f0f0f0; }"
    );
    m_nicknameBtn->setCursor(Qt::PointingHandCursor);
    connect(m_nicknameBtn, &QPushButton::clicked, this, &GroupSidePage::on_nickname_clicked);
    nicknameContainerLayout->addWidget(m_nicknameBtn);

    layout->addWidget(nicknameContainer);

    // 群聊备注
    QWidget* remarkContainer = new QWidget(section);
    QVBoxLayout* remarkContainerLayout = new QVBoxLayout(remarkContainer);
    remarkContainerLayout->setContentsMargins(0, 0, 0, 0);
    remarkContainerLayout->setSpacing(5);

    QLabel* remarkTitle = new QLabel("群聊备注", remarkContainer);
    remarkTitle->setStyleSheet("font-size: 12px; color: #999999;");
    remarkContainerLayout->addWidget(remarkTitle);

    m_remarkBtn = new QPushButton("填写备注", remarkContainer);
    m_remarkBtn->setFixedHeight(45);
    m_remarkBtn->setStyleSheet(
        "QPushButton { background-color: #ffffff; border-radius: 5px; font-size: 14px; color: #333333; text-align: left; padding-left: 15px; }"
        "QPushButton:hover { background-color: #f0f0f0; }"
    );
    m_remarkBtn->setCursor(Qt::PointingHandCursor);
    connect(m_remarkBtn, &QPushButton::clicked, this, &GroupSidePage::on_remark_clicked);
    remarkContainerLayout->addWidget(m_remarkBtn);

    layout->addWidget(remarkContainer);

    // 发言权限
    QPushButton* speakBtn = new QPushButton("发言权限", section);
    speakBtn->setFixedHeight(45);
    speakBtn->setStyleSheet(
        "QPushButton { background-color: #ffffff; border-radius: 5px; font-size: 14px; color: #333333; text-align: left; padding-left: 15px; }"
        "QPushButton:hover { background-color: #f0f0f0; }"
    );
    speakBtn->setCursor(Qt::PointingHandCursor);
    connect(speakBtn, &QPushButton::clicked, this, &GroupSidePage::on_speak_permission_clicked);
    layout->addWidget(speakBtn);

    // 置顶和免打扰
    layout->addWidget(createSwitchItem("设为置顶", false, [this](bool checked) {
        emit sig_set_top(_group_data ? _group_data->GetThreadId() : 0, checked);
    }));

    layout->addWidget(createSwitchItem("消息免打扰", false, [this](bool checked) {
        emit sig_set_mute(_group_data ? _group_data->GetThreadId() : 0, checked);
    }));

    return section;
}

QWidget* GroupSidePage::createManageSection()
{
    QWidget* section = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // 删除聊天记录
    QPushButton* clearBtn = new QPushButton("删除聊天记录", section);
    clearBtn->setFixedHeight(45);
    clearBtn->setStyleSheet(
        "QPushButton { background-color: #ffffff; border-radius: 5px; font-size: 14px; color: #333333; }"
        "QPushButton:hover { background-color: #f0f0f0; }"
    );
    clearBtn->setCursor(Qt::PointingHandCursor);
    connect(clearBtn, &QPushButton::clicked, this, &GroupSidePage::on_clear_history_clicked);
    layout->addWidget(clearBtn);

    // 转让群聊 (仅群主可见)
    m_transferBtn = createButtonItem("转让群聊", "color: #333333;");
    m_transferBtn->setObjectName("transferBtn");
    connect(m_transferBtn, &QPushButton::clicked, this, &GroupSidePage::on_transfer_clicked);
    layout->addWidget(m_transferBtn);

    // 解散/退出群聊
    m_dismissExitBtn = createButtonItem("解散群聊", "color: #ff4d4d; font-weight: bold;");
    connect(m_dismissExitBtn, &QPushButton::clicked, this, &GroupSidePage::on_dismiss_exit_clicked);
    layout->addWidget(m_dismissExitBtn);

    // 举报
    QPushButton* reportBtn = new QPushButton("被骚扰了？举报该群", section);
    reportBtn->setStyleSheet(
        "QPushButton { background-color: transparent; border: none; color: #1e90ff; font-size: 12px; }"
        "QPushButton:hover { text-decoration: underline; }"
    );
    reportBtn->setCursor(Qt::PointingHandCursor);
    connect(reportBtn, &QPushButton::clicked, this, &GroupSidePage::on_report_clicked);
    layout->addWidget(reportBtn);

    return section;
}

void GroupSidePage::SetGroupData(std::shared_ptr<ChatThreadData> group_data)
{
    _group_data = group_data;
    if (!_group_data) return;

    // 更新UI
    if (m_groupNameLabel) {
        m_groupNameLabel->setText(_group_data->GetGroupName());
    }
    if (m_groupIdLabel) {
        m_groupIdLabel->setText(QString("🔒 群号: %1").arg(_group_data->GetThreadId()));
    }

    if (m_noticeBtn) {
        QString notice = _group_data->GetNotice();
        m_noticeBtn->setText(notice.isEmpty() ? "未设置" : notice);
    }

    // TODO: 设置其他数据 (昵称、备注等)

    updateUi();

    // 更新成员数量和头像显示
    const auto& members = _group_data->GetGroupMembers();
    int count = members.size();
    if (m_memberCountLabel) {
        m_memberCountLabel->setText(QString("当前%1名群成员").arg(count));
    }
    
    // 更新成员头像列表（显示前5个成员）
    if (m_membersLayout) {
        // 清除旧的头像
        QLayoutItem* child;
        while ((child = m_membersLayout->takeAt(0)) != nullptr) {
            if (child->widget()) {
                delete child->widget();
            }
            delete child;
        }
        
        // 显示前5个成员头像
        int displayCount = qMin(5, count);
        for (int i = 0; i < displayCount; ++i) {
            QLabel* avatarLabel = new QLabel(m_membersContainer);
            avatarLabel->setFixedSize(40, 40);
            avatarLabel->setStyleSheet("background-color: #e0e0e0; border-radius: 20px;");
            avatarLabel->setAlignment(Qt::AlignCenter);
            
            // 显示成员名称的第一个字或默认头像
            QString name = members[i]->_name;
            if (!name.isEmpty()) {
                avatarLabel->setText(name.left(1));
            } else {
                avatarLabel->setText("?");
            }
            
            m_membersLayout->addWidget(avatarLabel);
        }
    }

    // 获取当前用户角色
    _current_role = GetCurrentUserRole();

    // 根据角色更新UI
    updateUi();
}

int GroupSidePage::GetCurrentUserRole()
{
    if (!_group_data) return 0;

    int uid = UserMgr::GetInstance()->GetUid();
    const auto& members = _group_data->GetGroupMembers();
    
    for (const auto& member : members) {
        if (member->_uid == uid) {
            return member->_role;
        }
    }
    return 0;  // 默认普通成员
}

void GroupSidePage::updateUi()
{
    // 更新解散/退出按钮文字
    if (m_dismissExitBtn) {
        if (_current_role == 2) {
            m_dismissExitBtn->setText("解散群聊");
        } else {
            m_dismissExitBtn->setText("退出群聊");
        }
    }

    // 显示/隐藏转让群聊按钮 (仅群主可见)
    if (m_transferBtn) {
        m_transferBtn->setVisible(_current_role == 2);
    }

    // 显示/隐藏移除成员按钮 (管理员和群主可见)
    QPushButton* removeBtn = findChild<QPushButton*>("removeMemberBtn");
    if (removeBtn) {
        removeBtn->setVisible(_current_role >= 1);
    }
}

QWidget* GroupSidePage::createSwitchItem(const QString& text, bool checked, std::function<void(bool)> callback)
{
    QWidget* item = new QWidget(this);
    item->setFixedHeight(45);
    item->setStyleSheet("background-color: #ffffff; border-radius: 5px;");
    QHBoxLayout* layout = new QHBoxLayout(item);
    layout->setContentsMargins(12, 0, 12, 0);

    QLabel* label = new QLabel(text, item);
    label->setStyleSheet("font-size: 14px; color: #333333;");
    layout->addWidget(label);
    layout->addStretch();

    QPushButton* switchBtn = new QPushButton(item);
    switchBtn->setCheckable(true);
    switchBtn->setChecked(checked);
    switchBtn->setFixedSize(45, 22);
    switchBtn->setCursor(Qt::PointingHandCursor);
    switchBtn->setStyleSheet(
        "QPushButton { background-color: #dcdcdc; border-radius: 11px; }"
        "QPushButton:checked { background-color: #00c0ff; }"
    );
    
    if (callback) {
        connect(switchBtn, &QPushButton::toggled, this, [callback](bool checked) {
            callback(checked);
        });
    }
    
    layout->addWidget(switchBtn);

    return item;
}

QWidget* GroupSidePage::createNavItem(const QString& text, const QString& value, bool hasArrow)
{
    QWidget* item = new QWidget(this);
    item->setFixedHeight(45);
    item->setStyleSheet("background-color: #ffffff; border-radius: 5px;");
    QHBoxLayout* layout = new QHBoxLayout(item);
    layout->setContentsMargins(12, 0, 12, 0);

    QLabel* label = new QLabel(text, item);
    label->setStyleSheet("font-size: 14px; color: #333333;");
    layout->addWidget(label);
    layout->addStretch();

    if (!value.isEmpty()) {
        QLabel* valueLabel = new QLabel(value, item);
        valueLabel->setStyleSheet("font-size: 13px; color: #999999;");
        layout->addWidget(valueLabel);
    }

    if (hasArrow) {
        QLabel* arrow = new QLabel(item);
        arrow->setPixmap(QPixmap(":/images/arowright.png").scaled(12, 12, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        layout->addWidget(arrow);
    }

    return item;
}

QPushButton* GroupSidePage::createButtonItem(const QString& text, const QString& style)
{
    QPushButton* btn = new QPushButton(text, this);
    btn->setFixedHeight(45);
    QString baseStyle = "QPushButton { background-color: #ffffff; border-radius: 5px; font-size: 14px; border: none; }";
    btn->setStyleSheet(baseStyle + "QPushButton { " + style + " }");
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

void GroupSidePage::on_search_timeout()
{
    // TODO: 搜索功能
}

void GroupSidePage::on_view_members_clicked()
{
    if (_group_data) {
        emit sig_view_members(_group_data->GetThreadId());
    }
}

void GroupSidePage::on_invite_clicked()
{
    if (_group_data) {
        emit sig_invite_friend(_group_data->GetThreadId());
    }
}

void GroupSidePage::on_clear_history_clicked()
{
    if (_group_data) {
        emit sig_clear_history(_group_data->GetThreadId());
    }
}

void GroupSidePage::on_dismiss_exit_clicked()
{
    if (!_group_data) return;
    
    if (_current_role == 2) {
        emit sig_dismiss_group(_group_data->GetThreadId());
    } else {
        emit sig_exit_group(_group_data->GetThreadId());
    }
}

void GroupSidePage::on_report_clicked()
{
    if (_group_data) {
        emit sig_report_group(_group_data->GetThreadId());
    }
}

void GroupSidePage::on_transfer_clicked()
{
    if (_group_data) {
        emit sig_transfer_group(_group_data->GetThreadId());
    }
}

void GroupSidePage::on_notice_clicked()
{
    if (!_group_data) return;

    GroupNoticeDialog dlg(_group_data->GetNotice(), this);
    if (dlg.exec() == QDialog::Accepted) {
        QString new_notice = dlg.GetNotice();
        if (new_notice != _group_data->GetNotice()) {
            emit sig_update_notice(_group_data->GetThreadId(), new_notice);
        }
    }
}

void GroupSidePage::on_nickname_clicked()
{
    if (_group_data) {
        emit sig_edit_nickname(_group_data->GetThreadId());
    }
}

void GroupSidePage::on_speak_permission_clicked()
{
    if (_group_data) {
        emit sig_speak_permission(_group_data->GetThreadId());
    }
}

void GroupSidePage::on_open_settings_clicked()
{
    if (_group_data) {
        emit sig_open_settings(_group_data->GetThreadId());
    }
}

void GroupSidePage::on_profile_settings_clicked()
{
    if (_group_data) {
        // TODO: 打开群资料设置页面
    }
}

void GroupSidePage::on_remark_clicked()
{
    if (_group_data) {
        // TODO: 打开群聊备注编辑
    }
}

//让GroupSidePage具备样式表渲染能力。
//因为纯QWidget的默认paintEvent是空实现，无法识别样式表（比如你设置的背景色、边框、圆角等），
//而这段代码通过调用 Qt 的样式系统绘制控件的基础外观，从而让样式表生效。
void GroupSidePage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
