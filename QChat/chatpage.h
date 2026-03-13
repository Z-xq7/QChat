#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>
#include "global.h"
#include "userdata.h"
#include "ChatItemBase.h"
#include <QMap>

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
    void SetChatData(std::shared_ptr<ChatThreadData> chat_data);
    //下载头像
    void LoadHeadIcon(QString avatarPath, QLabel* icon_label, QString file_name, QString req_type);
    //设置聊天中头像
    void SetChatIcon(ChatItemBase* pChatItem, int uid, QString icon,  QString req_type);
    //添加自己发送的消息
    void AppendChatMsg(std::shared_ptr<ChatDataBase> msg);
    //追加他人发来的消息
    void AppendOtherMsg(std::shared_ptr<ChatDataBase> msg);
    //清空聊天信息
    void clearItems();
    //更新聊天文本信息状态
    void UpdateChatStatus(std::shared_ptr<ChatDataBase> msg);
    //更新聊天图片信息状态
    void UpdateImgChatStatus(std::shared_ptr<ImgChatData> img_msg);
    //更新文件上传状态
    void UpdateFileProgress(std::shared_ptr<MsgInfo> msg_info);
    //文件图片下载完成
    void DownloadFileFinished(std::shared_ptr<MsgInfo> msg_info, QString file_path);

protected:
    //让ChatPage（继承自QWidget的自定义控件）具备样式表渲染能力。
    //因为纯QWidget的默认paintEvent是空实现，无法识别样式表（比如你设置的背景色、边框、圆角等），
    //而这段代码通过调用 Qt 的样式系统绘制控件的基础外观，从而让样式表生效。
    void paintEvent(QPaintEvent *event);

private slots:
    void on_send_btn_clicked();
    void on_receive_btn_clicked();
    //接收PictureBubble传回来的暂停信号
    void on_clicked_paused(QString unique_name, TransferType transfer_type);
    //接收PictureBubble传回来的继续信号
    void on_clicked_resume(QString unique_name, TransferType transfer_type);

private:
    Ui::ChatPage *ui;
    QMap<QString, QWidget*>  _bubble_map;

    std::shared_ptr<ChatThreadData> _chat_data;
    //管理未回复聊天信息，未读消息item(unique_id,与发送的一条条消息ChatItemBase关联)
    QMap<QString, ChatItemBase*> _unrsp_item_map;
    //管理已经回复的消息
    QHash<qint64, ChatItemBase*> _base_item_map;

// signals:
//     void sig_append_send_chat_msg(std::shared_ptr<TextChatData> msg);
};

#endif // CHATPAGE_H
