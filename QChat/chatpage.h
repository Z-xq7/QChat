#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>
#include "global.h"
#include "userdata.h"

namespace Ui {
class ChatPage;
}

class ChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage();

    //设置当前聊天用户信息
    void SetUserInfo(std::shared_ptr<UserInfo> user_info);
    //添加消息
    void AppendChatMsg(std::shared_ptr<TextChatData> msg);

protected:
    //让ChatPage（继承自QWidget的自定义控件）具备样式表渲染能力。
    //因为纯QWidget的默认paintEvent是空实现，无法识别样式表（比如你设置的背景色、边框、圆角等），
    //而这段代码通过调用 Qt 的样式系统绘制控件的基础外观，从而让样式表生效。
    void paintEvent(QPaintEvent *event);

private slots:
    void on_send_btn_clicked();

private:
    Ui::ChatPage *ui;

    std::shared_ptr<UserInfo> _user_info;

signals:
    void sig_append_send_chat_msg(std::shared_ptr<TextChatData> msg);
};

#endif // CHATPAGE_H
