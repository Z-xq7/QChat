#ifndef CHATVIEW_H
#define CHATVIEW_H
#include <QScrollArea>
#include <QVBoxLayout>
#include <QTimer>
#include <QEvent>
#include <QStyleOption>
#include <QPainter>

class ChatView :public QWidget
{
    Q_OBJECT
public:
    ChatView(QWidget *parent = Q_NULLPTR);

    void appendChatItem(QWidget *item);                 //尾插
    void prependChatItem(QWidget *item);                //头插
    void insertChatItem(QWidget *before, QWidget *item);//中间插
    void removeAllItem();                               //移除所有的聊天信息item

    // 批量 append：包裹多次 appendChatItem，减少 repaint
    void beginBatchAppend();
    void endBatchAppend(bool scrollToBottom = true);

    // 批量 prepend：包裹多次 prependChatItem，prepend 后锁定滚动位置不跳动
    void beginBatchPrepend();
    void endBatchPrepend();

    QScrollArea* scrollArea() const { return m_pScrollArea; }

protected:
    //事件过滤器，检测鼠标是否进入当前区域，以及滚动条的显示
    bool eventFilter(QObject *o, QEvent *e) override;
    //让ChatView（继承自QWidget的自定义控件）具备样式表渲染能力。
    //因为纯QWidget的默认paintEvent是空实现，无法识别样式表（比如你设置的背景色、边框、圆角等），
    //而这段代码通过调用 Qt 的样式系统绘制控件的基础外观，从而让样式表生效。
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onVScrollBarMoved(int min, int max);

private:
    void initStyleSheet();

private:
    //QWidget *m_pCenterWidget;
    QVBoxLayout *m_pVl;
    QScrollArea *m_pScrollArea;
    bool isAppended;

    // 批量 prepend 时保存的滚动状态，用于 endBatchPrepend 后恢复视角
    int m_savedScrollValue   = 0;
    int m_savedContentHeight = 0;
};

#endif // CHATVIEW_H

