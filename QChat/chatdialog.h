#ifndef CHATDIALOG_H
#define CHATDIALOG_H

#include <QDialog>
#include <QAction>
#include <QList>
#include "global.h"
#include "loadingdialog.h"
#include "StateWidget.h"
#include "adduseritem.h"
#include "userdata.h"
#include <QListWidgetItem>
#include <QTimer>
namespace Ui {
class ChatDialog;
}

class ChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChatDialog(QWidget *parent = nullptr);
    ~ChatDialog();

    //添加聊天用户列表(已废弃)
    //void addChatUserList();
    //加载聊天列表
    void loadChatList();
    //加载聊天信息
    void loadChatMsg();
    //清楚侧边栏状态
    void ClearLabelState(StateWidget* lb);
    //更新聊天列表信息
    void SetSelectChatItem(int thread_id = 0);
    //更新聊天页面信息
    void SetSelectChatPage(int thread_id = 0);
    //加载更多的聊天好友列表(已废弃)
    void LoadMoreChatUser();
    //加载更多的好友信息列表
    void LoadMoreConUser();
    //更新聊天界面显示消息
    void UpdateChatMsg(std::vector<std::shared_ptr<TextChatData>> msgdata);
    //显示加载对话框
    void showLoadingDlg(bool show);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void handleGlobalMousePress(QMouseEvent* event);

private:
    Ui::ChatDialog *ui;

    void ShowSearch(bool bsearch = false);
    void AddLBGroup(StateWidget* lb);

    ChatUIMode _mode;
    ChatUIMode _state;
    bool _b_loading;
    QList<StateWidget*> _lb_list;
    //管理聊天列表
    QMap<int, QListWidgetItem*> _chat_items_added;
    //chat_thread_id与item的映射关系
    QMap<int, QListWidgetItem*> _chat_thread_items;
    //当前的聊天编号
    int _cur_chat_uid;
    //记录上次的widget状态
    QWidget*_last_widget;
    //心跳定时器
    QTimer* _timer;
    //加载对话框指针
    LoadingDialog* _loading_dialog;
    //
    std::shared_ptr<ChatThreadData> _cur_load_chat;

private slots:
    //加载未显示的聊天列表
    void slot_loading_chat_user();
    //加载未显示的好友列表
    void slot_loading_contact_user();
    void slot_side_chat();
    void slot_side_contact();
    void slot_side_settings();
    void slot_text_changed(const QString& str);

public slots:
    void slot_apply_friend(std::shared_ptr<AddFriendApply> apply);
    //申请添加好友被对方同意成功
    void slot_add_auth_firend(std::shared_ptr<AuthInfo> auth_info);
    //同意添加对方为好友
    void slot_auth_rsp(std::shared_ptr<AuthRsp> auth_rsp);
    //由搜索跳转到聊天界面
    void slot_jump_chat_item(std::shared_ptr<SearchInfo> si);
    //由好友信息跳转到聊天页面
    void slot_jump_chat_item_from_infopage(std::shared_ptr<UserInfo> user_info);
    //点击好友信息item显示好友信息页面
    void slot_friend_info_page(std::shared_ptr<UserInfo> user_info);
    //点击添加好友跳转到
    void slot_switch_apply_friend_page();
    //点击聊天列表后显示聊天页面
    void slot_item_clicked(QListWidgetItem* item);
    //将聊天消息缓存在本地
    void slot_append_send_chat_msg(std::shared_ptr<TextChatData> msgdata);
    //对方发来消息通知
    void slot_text_chat_msg(std::shared_ptr<TextChatMsg> msg);
    //加载聊天列表
    void slot_load_chat_thread(bool load_more,int next_last_id,std::vector<std::shared_ptr<ChatThreadInfo>> thread_list);
    //从friendinfopage新创建聊天item
    void slot_create_private_chat(int uid, int other_id, int thread_id);
};

#endif // CHATDIALOG_H
