#ifndef TCPMGR_H
#define TCPMGR_H
#include <QTcpSocket>
#include "singleton.h"
#include "global.h"
#include <functional>
#include <QObject>
#include "userdata.h"


class TcpMgr:public QObject,public Singleton<TcpMgr>,public std::enable_shared_from_this<TcpMgr>
{
    //用于接收、发送信号的宏
    Q_OBJECT
    friend class Singleton<TcpMgr>;
public:
    ~TcpMgr();
    //关闭tcp连接
    void CloseConnection();

private:
    TcpMgr();
    //注册tcp请求处理
    void initHandlers();
    //接收请求后遍历注册的处理逻辑，进行处理
    void handleMsg(ReqId id,int len,QByteArray data);

    QTcpSocket _socket;
    QString _host;
    uint16_t _port;
    bool _b_recv_pending;
    quint16 _message_id;
    quint16 _message_len;
    QByteArray _buffer;
    QMap<ReqId,std::function<void(ReqId id,int len,QByteArray data)>> _handlers;

public slots:
    //tcp连接成功后触发
    void slot_tcp_connect(ServerInfo);
    //数据发送成功后调用
    void slot_send_data(ReqId reqId, QByteArray data);

signals:
    //连接成功信号
    void sig_con_success(bool bsuccess);
    //发送数据成功
    void sig_send_data(ReqId reqId, QByteArray data);
    //切换到聊天界面
    void sig_switch_chatdlg();
    //登录失败
    void sig_login_failed(int);
    //搜索用户
    void sig_user_search(std::shared_ptr<SearchInfo>);
    //申请添加好友
    void sig_friend_apply(std::shared_ptr<AddFriendApply>);
    //好友申请被同意
    void sig_add_auth_friend(std::shared_ptr<AuthInfo>);
    //同意别人的好友申请
    void sig_auth_rsp(std::shared_ptr<AuthRsp>);
    //聊天文本变化->接收对方发来消息
    void sig_text_chat_msg(std::shared_ptr<TextChatMsg> msg);
    //异地登录通知下线
    void sig_notify_offline();
    //连接异常通知连接关闭
    void sig_connection_closed();
    //加载聊天线程
    void sig_load_chat_thread(bool load_more,int next_last_id,std::vector<std::shared_ptr<ChatThreadInfo>> thread_list);
    //通知界面添加聊天item
    void sig_create_private_chat(int uid, int other_id, int thread_id);
};

#endif // TCPMGR_H
