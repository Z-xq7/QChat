#include "chatsidepage.h"
#include <QScrollArea>
#include <QStyleOption>
#include <QPainter>
#include <QPaintEvent>
#include <QFrame>

ChatSidePage::ChatSidePage(QWidget *parent) : QWidget(parent)
{
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    connect(m_searchTimer, &QTimer::timeout, this, &ChatSidePage::on_search_timeout);
    initUi();
}

void ChatSidePage::initUi()
{
    this->setObjectName("ChatSidePage");
    this->setFixedWidth(280);
    this->setStyleSheet("#ChatSidePage { background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
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
    scrollArea->viewport()->setStyleSheet("background-color: transparent;");
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background-color: transparent; }"
        "QScrollBar:vertical { background-color: transparent; width: 6px; margin: 0px; }"
        "QScrollBar::handle:vertical { background-color: #c0c0c0; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background-color: #a0a0a0; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background-color: transparent; }"
    );

    // 创建内容容器
    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("background-color: transparent;");
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(15, 15, 15, 15);
    contentLayout->setSpacing(15);

    // ====== 通知设置区域（带开关）======
    {
        QWidget* section = new QWidget();
        QVBoxLayout* sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(10);

        sectionLayout->addWidget(createSwitchItem("消息免打扰", false));
        sectionLayout->addWidget(createSwitchItem("设为置顶", false));
        sectionLayout->addWidget(createSwitchItem("屏蔽此人", false));

        contentLayout->addWidget(section);
    }

    // ====== 资料区域 ======
    {
        QWidget* section = new QWidget();
        QVBoxLayout* sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(10);

        sectionLayout->addWidget(createCardButton("文件传输列表"));

        contentLayout->addWidget(section);
    }

    // ====== 搜索记录 ======
    {
        QWidget* searchContainer = new QWidget();
        QVBoxLayout* searchContainerLayout = new QVBoxLayout(searchContainer);
        searchContainerLayout->setContentsMargins(0, 0, 0, 0);
        searchContainerLayout->setSpacing(5);

        QLabel* searchTitle = new QLabel("搜索聊天记录", searchContainer);
        searchTitle->setStyleSheet("font-size: 12px; color: #999999;");
        searchContainerLayout->addWidget(searchTitle);

        m_searchEdit = new QLineEdit(searchContainer);
        m_searchEdit->setPlaceholderText("输入关键词搜索");
        m_searchEdit->setObjectName("side_search_edit");
        m_searchEdit->setFixedHeight(38);
        m_searchEdit->setStyleSheet(
            "QLineEdit#side_search_edit { background-color: #ffffff; border: 1px solid #dcdcdc; "
            "border-radius: 8px; padding: 0 12px; font-size: 13px; color: #333333; }"
            "QLineEdit#side_search_edit:focus { border-color: #00c0ff; }"
            "QLineEdit#side_search_edit::placeholder { color: #bbbbbb; }"
        );
        searchContainerLayout->addWidget(m_searchEdit);
        connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text){
            Q_UNUSED(text);
            m_searchTimer->start(300);
        });

        contentLayout->addWidget(searchContainer);
    }

    // ====== 危险操作区域 ======
    {
        QWidget* section = new QWidget();
        QVBoxLayout* sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(10);

        sectionLayout->addWidget(createCardButton("删除聊天记录"));

        // 删除好友 (红色按钮)
        QPushButton* deleteFriendBtn = new QPushButton("删除好友", section);
        deleteFriendBtn->setFixedHeight(45);
        deleteFriendBtn->setStyleSheet(
            "QPushButton { background-color: #ffffff; border-radius: 5px; font-size: 14px; "
            "color: #ff4d4d; font-weight: bold; border: none; }"
            "QPushButton:hover { background-color: #fff5f5; }"
        );
        deleteFriendBtn->setCursor(Qt::PointingHandCursor);
        sectionLayout->addWidget(deleteFriendBtn);

        contentLayout->addWidget(section);
    }

    // ====== 举报 ======
    QPushButton* reportBtn = new QPushButton("被骚扰了？举报该用户", contentWidget);
    reportBtn->setStyleSheet(
        "QPushButton { background-color: transparent; border: none; color: #1e90ff; font-size: 12px; }"
        "QPushButton:hover { text-decoration: underline; }"
    );
    reportBtn->setCursor(Qt::PointingHandCursor);
    contentLayout->addWidget(reportBtn);

    contentLayout->addStretch();

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
}

void ChatSidePage::on_search_timeout()
{
    emit sig_search_text_changed(m_searchEdit->text());
}

QWidget* ChatSidePage::createSwitchItem(const QString& text, bool checked)
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
    layout->addWidget(switchBtn);

    return item;
}

QPushButton* ChatSidePage::createCardButton(const QString& text)
{
    QPushButton* btn = new QPushButton(text);
    btn->setFixedHeight(45);
    btn->setStyleSheet(
        "QPushButton { background-color: #ffffff; border-radius: 5px; font-size: 14px; "
        "color: #333333; text-align: left; padding-left: 12px; border: none; }"
        "QPushButton:hover { background-color: #f0f0f0; }"
    );
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

//让ChatSidePage具备样式表渲染能力
void ChatSidePage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
