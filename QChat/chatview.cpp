#include "chatview.h"
#include <QScrollBar>
#include <QPalette>

ChatView::ChatView(QWidget *parent):QWidget(parent),isAppended(false){
    //创建Layout布局
    QVBoxLayout *pMainLayout = new QVBoxLayout();
    //添加进chatview
    this->setLayout(pMainLayout);
    //设置Layout内边距为0
    pMainLayout->setContentsMargins(0, 0, 0, 0);

    //创建滚动条
    m_pScrollArea = new QScrollArea();
    m_pScrollArea->setObjectName("chat_area");
    //将滚动条加到Layout中
    pMainLayout->addWidget(m_pScrollArea);

    //创建一个Widget
    QWidget *w = new QWidget(this);
    w->setObjectName("chat_bg");
    //自动填充背景
    w->setAutoFillBackground(true);

    //再新建一个Layout
    QVBoxLayout *pVLayout_1 = new QVBoxLayout();
    //在Layout里新加一个Widget
    QWidget *w2 = new QWidget();
    // w2->setStyleSheet("background-color: rgb(230,255,255);");
    pVLayout_1->addWidget(w2, 100000);
    //将这个新建的Layout添加到之前创建的Widget中
    w->setLayout(pVLayout_1);
    //将之前创建的Widget添加到滚动条布局中
    m_pScrollArea->setWidget(w);

    //到此为止整体布局为：ChatView->Layout(pMainLayout)->ScrollArea(m_pScrollArea)->Widget(w)->Layout(pLayout_1)->Widget

    //关闭垂直滚动条
    m_pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    //获取垂直滚动条
    QScrollBar *pVScrollBar = m_pScrollArea->verticalScrollBar();
    //当滚动条范围发生变化时触发槽函数
    connect(pVScrollBar, &QScrollBar::rangeChanged,this, &ChatView::onVScrollBarMoved);

    //把垂直ScrollBar放到上边 而不是原来的并排
    QHBoxLayout *pHLayout_2 = new QHBoxLayout();
    pHLayout_2->addWidget(pVScrollBar, 0, Qt::AlignRight);
    pHLayout_2->setContentsMargins(0, 0, 0, 0);
    m_pScrollArea->setLayout(pHLayout_2);
    pVScrollBar->setHidden(true);

    //允许ScrollArea内部件自主设置大小
    m_pScrollArea->setWidgetResizable(true);
    m_pScrollArea->installEventFilter(this);
    initStyleSheet();
}

void ChatView::appendChatItem(QWidget *item)
{
    QVBoxLayout *vl = qobject_cast<QVBoxLayout *>(m_pScrollArea->widget()->layout());
    vl->insertWidget(vl->count()-1, item);
    isAppended = true;
}

void ChatView::prependChatItem(QWidget *item)
{

}

void ChatView::insertChatItem(QWidget *before, QWidget *item)
{

}

void ChatView::removeAllItem()
{
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(m_pScrollArea->widget()->layout());

    int count = layout->count();

    for (int i = 0; i < count - 1; ++i) {
        QLayoutItem *item = layout->takeAt(0); // 始终从第一个控件开始删除
        if (item) {
            if (QWidget *widget = item->widget()) {
                delete widget;
            }
            delete item;
        }
    }
}

bool ChatView::eventFilter(QObject *o, QEvent *e)
{
    /*if(e->type() == QEvent::Resize && o == )
    {
    }
    else */if(e->type() == QEvent::Enter && o == m_pScrollArea)
    {
        m_pScrollArea->verticalScrollBar()->setHidden(m_pScrollArea->verticalScrollBar()->maximum() == 0);
    }
    else if(e->type() == QEvent::Leave && o == m_pScrollArea)
    {
        m_pScrollArea->verticalScrollBar()->setHidden(true);
    }
    return QWidget::eventFilter(o, e);
}

void ChatView::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ChatView::onVScrollBarMoved(int min, int max)
{
    if(isAppended) //添加item可能调用多次
    {
        QScrollBar *pVScrollBar = m_pScrollArea->verticalScrollBar();
        //设置滚动区域
        pVScrollBar->setSliderPosition(pVScrollBar->maximum());
        //500毫秒内可能调用多次
        QTimer::singleShot(500, [this]()
        {
            isAppended = false;
        });
    }
}

void ChatView::initStyleSheet()
{

}
