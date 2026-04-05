#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>
#include "global.h"
#include "userdata.h"
#include "ChatItemBase.h"
#include <QMap>
#include <QSet>
#include <QPixmap>
#include "chatsidepage.h"
#include "groupsidepage.h"
#include "groupmemberlistpage.h"
#include <QPropertyAnimation>

class GroupNoticeViewDialog;

namespace Ui {
class ChatPage;
}

class EmojiPopup;

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
    void AppendChatMsg(std::shared_ptr<ChatDataBase> msg, bool rsp=true);
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
    void DownloadFileFinished(std::shared_ptr<MsgInfo> msg_info, QString file_path);
    void FileTransferFailed(std::shared_ptr<MsgInfo> msg_info);

protected:
    //让ChatPage（继承自QWidget的自定义控件）具备样式表渲染能力。
    //因为纯QWidget的默认paintEvent是空实现，无法识别样式表（比如你设置的背景色、边框、圆角等），
    //而这段代码通过调用 Qt 的样式系统绘制控件的基础外观，从而让样式表生效。
    void paintEvent(QPaintEvent *event);

private slots:
    void on_send_btn_clicked();
    void on_receive_btn_clicked();
    //接收PictureBubble传回来的暂停信号
    void on_clicked_paused(std::shared_ptr<MsgInfo> msg_info);
    //接收PictureBubble传回来的继续信号
    void on_clicked_resume(std::shared_ptr<MsgInfo> msg_info);
    void on_view_picture(const QString& file_path, const QPixmap& preview_pix);
    void on_file_clicked(QString, ClickLbState);
    void on_emo_clicked(QString, ClickLbState);
    void on_emoji_selected(const QString& emoji);
    // 搜索聊天记录
    void slot_chat_search(const QString& text);
    void slot_group_update_notice(int thread_id, const QString& notice);
    // 更多设置按钮点击
    void on_more_clicked();
    // 视频通话按钮点击
    void on_video_chat_clicked();
    // 发起群聊按钮点击
    void on_group_chat_clicked();
    
    // 群聊侧边栏信号处理
    void on_group_view_members(int thread_id);
    void on_group_invite_friend(int thread_id);
    void on_group_clear_history(int thread_id);
    void on_group_dismiss(int thread_id);
    void on_group_exit(int thread_id);
    void on_group_report(int thread_id);
    void on_group_update_notice(int thread_id, const QString& notice);

    // 群公告按钮点击（查看公告）
    void on_group_notice_clicked();

private:
    // 初始化侧边栏
    void initSidePage();
    // 切换侧边栏显示
    void toggleSidePage();
    // 显示群成员列表
    void showGroupMemberList(int thread_id);
    // 显示群公告弹窗
    void showGroupNoticeDialog(int thread_id);
    Ui::ChatPage *ui;
    ChatSidePage* m_sidePage;
    GroupSidePage* m_groupSidePage;
    GroupMemberListPage* m_memberListPage;
    QPropertyAnimation* m_sideAnim;
    QMap<QString, QWidget*>  _bubble_map;
    EmojiPopup* _emoji_popup;

    std::shared_ptr<ChatThreadData> _chat_data;
    //管理未回复聊天信息，未读消息item(unique_id,与发送的一条条消息ChatItemBase关联)
    QMap<QString, ChatItemBase*> _unrsp_item_map;
    //管理已经回复的消息
    QHash<qint64, ChatItemBase*> _base_item_map;
    
    // 当前侧边栏类型: 0=私聊, 1=群聊
    int m_currentSideType;

    // 记录有待弹出公告的群聊 thread_id 集合（用户不在该群页面时收到公告更新，延迟弹出）
    QSet<int> m_pendingNoticeThreadIds;
    // 记录自己正在编辑公告的 thread_id（编辑完成收到 RSP 后不弹窗）
    int m_selfEditingNoticeThreadId;

// signals:
//     void sig_append_send_chat_msg(std::shared_ptr<TextChatData> msg);
};

#endif // CHATPAGE_H
