#include "bubbleframe.h"
#include <QPainter>
#include <QDebug>

const int WIDTH_SANJIAO  = 8;  //三角宽

BubbleFrame::BubbleFrame(ChatRole role, QWidget *parent)
    :QFrame(parent)
    ,m_role(role)
    ,m_margin(3)
{
    //创建一个水平Layout布局
    m_pHLayout = new QHBoxLayout();
    //根据role不同设置聊天气泡样式
    if(m_role == ChatRole::Self)
        m_pHLayout->setContentsMargins(m_margin, m_margin, WIDTH_SANJIAO + m_margin, m_margin);
    else
        m_pHLayout->setContentsMargins(WIDTH_SANJIAO + m_margin, m_margin, m_margin, m_margin);
    //应用该Layout
    this->setLayout(m_pHLayout);
}

void BubbleFrame::setMargin(int margin)
{
    Q_UNUSED(margin);
}

void BubbleFrame::setWidget(QWidget *w)
{
    if(m_pHLayout->count() > 0)
        return ;
    else{
        m_pHLayout->addWidget(w);
    }
}

void BubbleFrame::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);
    //不要画笔，即无边框
    painter.setPen(Qt::NoPen);
    if(m_role == ChatRole::Other)
    {
        //画气泡 - 使用浅蓝色渐变
        QLinearGradient gradient(0, 0, 0, this->height());
        gradient.setColorAt(0, QColor(200, 230, 240, 220)); // 浅蓝色
        gradient.setColorAt(1, QColor(176, 224, 230, 220)); // 深一点的浅蓝
        painter.setBrush(QBrush(gradient));
        QRect bk_rect = QRect(WIDTH_SANJIAO, 0, this->width()-WIDTH_SANJIAO, this->height());
        painter.drawRoundedRect(bk_rect,10,10); // 增加圆角效果
        //画小三角
        QPointF points[3] = {
            QPointF(bk_rect.x(), 12),
            QPointF(bk_rect.x(), 10+WIDTH_SANJIAO +2),
            QPointF(bk_rect.x()-WIDTH_SANJIAO, 10+WIDTH_SANJIAO-WIDTH_SANJIAO/2),
        };
        painter.drawPolygon(points, 3);
    }
    else
    {
        //画气泡 - 使用浅粉色渐变
        QLinearGradient gradient(0, 0, 0, this->height());
        gradient.setColorAt(0, QColor(255, 240, 245, 220)); // 浅粉色
        gradient.setColorAt(1, QColor(255, 228, 225, 220)); // 深一点的浅粉
        painter.setBrush(QBrush(gradient));
        //画气泡
        QRect bk_rect = QRect(0, 0, this->width()-WIDTH_SANJIAO, this->height());
        painter.drawRoundedRect(bk_rect,10,10); // 增加圆角效果
        //画三角
        QPointF points[3] = {
            QPointF(bk_rect.x()+bk_rect.width(), 12),
            QPointF(bk_rect.x()+bk_rect.width(), 12+WIDTH_SANJIAO +2),
            QPointF(bk_rect.x()+bk_rect.width()+WIDTH_SANJIAO, 10+WIDTH_SANJIAO-WIDTH_SANJIAO/2),
        };
        painter.drawPolygon(points, 3);
    }
    return QFrame::paintEvent(e);
}
