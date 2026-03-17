#ifndef TCPMGR_H
#define TCPMGR_H
#include <QTcpSocket>
#include "singleton.h"
#include "global.h"
#include <functional>
#include <QObject>
#include "userdata.h"
#include <memory>
#include <QQueue>
#include <QByteArray>
#include <QThread>

class TcpThread:public std::enable_shared_from_this<TcpThread> {
public:
    TcpThread();
    ~TcpThread();
private:
    QThread* _tcp_thread;
};

class TcpMgr:public QObject,public Singleton<TcpMgr>,public std::enable_shared_from_this<TcpMgr>
{
    //用于接收、发送信号的宏
    Q_OBJECT
    friend class Singleton<TcpMgr>;
public:
    ~TcpMgr();
    // 用于连接VideoCallManager的初始化方法
    void initializeVideoCallConnections();
    //关闭tcp连接
    void CloseConnection();
    //向服务器发送数据（封装sig_send_data信号）
    void SendData(ReqId reqId, QByteArray data);

private:
    TcpMgr();
    //注册元类型（用于信号和槽的跨线程信息传输）
    void registerMetaType();
    //注册tcp请求处理
    void initHandlers();
    //接收请求后遍历注册的处理逻辑，进行处理
    void handleMsg(ReqId id,int len,QByteArray data);
    //创建图片加载
    void CreatePlaceholderImgMsgL(QString img_path_str, QString msg_content,
                                  int msg_id, int thread_id, int send_uid, int recv_id, int status, QString chat_time,
                                  std::vector<std::shared_ptr<ChatDataBase>>& chat_datas);

    QTcpSocket _socket;
    QString _host;
    uint16_t _port;
    bool _b_recv_pending;
    quint16 _message_id;
    quint16 _message_len;
    QByteArray _buffer;
    QMap<ReqId,std::function<void(ReqId id,int len,QByteArray data)>> _handlers;
    //发送队列
    QQueue<QByteArray> _send_queue;
    //正在发送的包
    QByteArray _current_block;
    //当前已发送的字节数
    qint64 _bytes_send;
    //是否正在发送
    bool _pending;


public slots:
    //关闭tcp连接
    void slot_tcp_close();
    //tcp连接成功后触发
    void slot_tcp_connect(std::shared_ptr<ServerInfo> si);
    //数据发送成功后调用
    void slot_send_data(ReqId reqId, QByteArray data);

signals:
    //关闭连接信号
    void sig_close();
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
    //异地登录通知下线
    void sig_notify_offline();
    //连接异常通知连接关闭
    void sig_connection_closed();
    //加载聊天线程
    void sig_load_chat_thread(bool load_more,int next_last_id,std::vector<std::shared_ptr<ChatThreadInfo>> thread_list);
    //通知界面添加聊天item
    void sig_create_private_chat(int uid, int other_id, int thread_id);
    //加载聊天界面chatpage的聊天对话消息
    void sig_load_chat_msg(int thread_id, int last_msg_id, bool load_more, std::vector<std::shared_ptr<ChatDataBase>> chat_datas);
    //发送文本消息后服务器回传接收到消息的信号后的通知
    void sig_chat_msg_rsp(int thread_id, std::vector<std::shared_ptr<TextChatData>> chat_datas);
    //发送图片消息后服务器回传接收到消息的信号后的通知
    void sig_chat_img_rsp(int thread_id, std::shared_ptr<ImgChatData>chat_data);
    //聊天文本变化->接收对方发来的文本消息
    void sig_text_chat_msg(std::vector<std::shared_ptr<TextChatData>> msg_list);
    //聊天图片变化->接收对方发来的图片消息
    void sig_img_chat_msg(std::shared_ptr<ImgChatData> msg_list);
    
    // 视频通话相关信号
    void sig_call_incoming(int caller_uid, const QString& call_id, const QString& caller_name);
    //接受对方视频请求
    void sig_accept_call(const QString& call_id, const QString& room_id, const QString& turn_ws_url, const QJsonArray& ice_servers);
    //视频请求被对方接受
    void sig_call_accepted(const QString& call_id, const QString& room_id, const QString& turn_ws_url, const QJsonArray& ice_servers);
    void sig_call_rejected(const QString& call_id, const QString& reason);
    void sig_call_hangup(const QString& call_id);
};

#endif // TCPMGR_H
