#ifndef CHATUSERLIST_H
#define CHATUSERLIST_H
#include <QListWidget>
#include <QWheelEvent>
#include <QEvent>
#include <QScrollBar>
#include <QDebug>

class ChatUserList: public QListWidget
{
    Q_OBJECT
public:
    ChatUserList(QWidget *parent = nullptr);

protected:
    //事件过滤器，检测鼠标是否进入当前区域，以及滚动条的显示
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool _load_pending;

signals:
    void sig_loading_chat_user();

};

#endif
