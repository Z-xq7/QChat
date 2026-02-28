#include "chatitembase.h"

ChatItemBase::ChatItemBase(ChatRole role, QWidget *parent):QWidget(parent),m_role(role)
{
    //设置用户名字
    m_pNameLabel = new QLabel();
    m_pNameLabel->setObjectName("chat_user_name");
    QFont font("Microsoft YaHei");
    font.setPointSize(9);
    m_pNameLabel->setFont(font);
    m_pNameLabel->setFixedHeight(20);

    //设置头像图标
    m_pIconLabel    = new QLabel();
    m_pIconLabel->setScaledContents(true);
    m_pIconLabel->setFixedSize(40, 40);

    //设置消息框
    m_pBubble = new QWidget();

    //定义网格布局
    QGridLayout *pGLayout = new QGridLayout();
    //设置控件间垂直间距
    pGLayout->setVerticalSpacing(3);
    //设置控件间水平间距
    pGLayout->setHorizontalSpacing(3);
    //设置边界
    pGLayout->setContentsMargins(3, 3, 3, 3);

    //设置弹簧
    QSpacerItem*pSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    //判断是谁发的消息，来区分消息位置
    if(m_role == ChatRole::Self)
    {
        //设置名字边界
        m_pNameLabel->setContentsMargins(0,0,8,0);
        //设置名字右对齐
        m_pNameLabel->setAlignment(Qt::AlignRight);
        //设置名字在网格布局中的位置->第0行第1列，占了一行一列
        pGLayout->addWidget(m_pNameLabel, 0,1, 1,1);
        //设置头像在网格布局中的位置->第0行第2列，占了两行一列
        pGLayout->addWidget(m_pIconLabel, 0, 2, 2,1, Qt::AlignTop);
        //设置弹簧在网格布局中的位置
        pGLayout->addItem(pSpacer, 1, 0, 1, 1);
        //设置聊天气泡在网格布局中的位置
        pGLayout->addWidget(m_pBubble, 1,1, 1,1);
        //设置拉伸比例
        pGLayout->setColumnStretch(0, 2);
        pGLayout->setColumnStretch(1, 3);
    }else{
        m_pNameLabel->setContentsMargins(8,0,0,0);
        m_pNameLabel->setAlignment(Qt::AlignLeft);
        pGLayout->addWidget(m_pIconLabel, 0, 0, 2,1, Qt::AlignTop);
        pGLayout->addWidget(m_pNameLabel, 0,1, 1,1);
        pGLayout->addWidget(m_pBubble, 1,1, 1,1);
        pGLayout->addItem(pSpacer, 2, 2, 1, 1);
        pGLayout->setColumnStretch(1, 3);
        pGLayout->setColumnStretch(2, 2);
    }
    //将网格布局添加到该布局中
    this->setLayout(pGLayout);
}

void ChatItemBase::setUserName(const QString &name)
{
    m_pNameLabel->setText(name);
}

void ChatItemBase::setUserIcon(const QPixmap &icon)
{
    m_pIconLabel->setPixmap(icon);
}

void ChatItemBase::setWidget(QWidget *w)
{
    //网格布局
    QGridLayout *pGLayout = (qobject_cast<QGridLayout *>)(this->layout());
    //替换聊天气泡
    pGLayout->replaceWidget(m_pBubble, w);
    //内存管理，手动删除被替换的聊天气泡，避免内存泄漏
    delete m_pBubble;
    m_pBubble = w;
}
