#include "tcpmgr.h"
#include <QAbstractSocket>
#include "usermgr.h"
#include <QJsonObject>
#include <QJsonArray>
#include "userdata.h"
#include <QTimer>
#include <QFile>
#include "filetcpmgr.h"
#include <QStandardPaths>
#include "videocallmanager.h"
#include <QFileIconProvider>

TcpThread::TcpThread()
{
    _tcp_thread = new QThread();
    TcpMgr::GetInstance()->moveToThread(_tcp_thread);
    QObject::connect(_tcp_thread, &QThread::finished, _tcp_thread, &QObject::deleteLater);
    _tcp_thread->start();
    qDebug() << "[TcpThread]: Init Success!";
}

TcpThread::~TcpThread()
{
    _tcp_thread->quit();
}

TcpMgr::~TcpMgr(){
}

TcpMgr::TcpMgr():_host(""),_port(0),_b_recv_pending(false),_message_id(0),_message_len(0),_bytes_send(0),_pending(false)
{
    registerMetaType();
    //当socket连接成功后的处理逻辑
    QObject::connect(&_socket, &QTcpSocket::connected, this, [&]() {
        qDebug() << "[TcpMgr]: Connected to server!";
        // 连接建立后发送消息
        emit sig_con_success(true);
    });

    //当socket有读就绪时处理逻辑
    QObject::connect(&_socket, &QTcpSocket::readyRead, this, [&]() {
        // 当有数据可读时，读取所有数据
        // 读取所有数据并追加到缓冲区
        _buffer.append(_socket.readAll());

        forever {
            //先解析头部
            if(!_b_recv_pending){
                // 检查缓冲区中的数据是否足够解析出一个消息头（消息ID + 消息长度）
                if (_buffer.size() < static_cast<int>(sizeof(quint16) * 2)) {
                    return; // 数据不够，等待更多数据
                }

                // ✅ 每次都重新创建stream
                QDataStream stream(_buffer);
                stream.setVersion(QDataStream::Qt_5_0);

                // 预读取消息ID和消息长度，但不从缓冲区中移除
                stream >> _message_id >> _message_len;

                //将buffer 中的前四个字节移除
                //_buffer = _buffer.mid(sizeof(quint16) * 2);
                _buffer.remove(0, sizeof(quint16) * 2);  // 使用remove代替mid赋值

                // 输出读取的数据
                qDebug() << "[TcpMgr]: Message ID:" << _message_id << ", Length:" << _message_len;

            }

            //buffer剩余长读是否满足消息体长度，不满足则退出继续等待接受
            if(_buffer.size() < _message_len){
                _b_recv_pending = true;
                return;
            }

            _b_recv_pending = false;
            // 读取消息体
            QByteArray messageBody = _buffer.mid(0, _message_len);
            qDebug() << "[TcpMgr]: receive body msg is " << messageBody ;

            _buffer = _buffer.mid(_message_len);
            handleMsg(ReqId(_message_id),_message_len,messageBody);
        }

    });

    //socket发生异常的处理逻辑
    QObject::connect(&_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
        this, [&](QAbstractSocket::SocketError socketError) {
        Q_UNUSED(socketError)
        qDebug() << "[TcpMgr]: Error:" << _socket.errorString();
    });

    // sokcet处理连接断开
    QObject::connect(&_socket, &QTcpSocket::disconnected, this, [&]() {
        qDebug() << "[TcpMgr]: Disconnected from server.";
        //并且发送通知到界面
        emit sig_connection_closed();
    });

    //连接发送信号用来发送
    QObject::connect(this, &TcpMgr::sig_send_data, this, &TcpMgr::slot_send_data);

    //连接发送信号
    QObject::connect(&_socket, &QTcpSocket::bytesWritten, this, [this](qint64 bytes) {
        //更新发送数据
        _bytes_send += bytes;
        //未发送完整
        if (_bytes_send < _current_block.size()) {
            //继续发送
            auto data_to_send = _current_block.mid(_bytes_send);
            _socket.write(data_to_send);
            return;
        }

        //发送完全，则查看队列是否为空
        if (_send_queue.isEmpty()) {
            //队列为空，说明已经将所有数据发送完成，将pending设置为false，这样后续要发送数据时可以继续发送
            _current_block.clear();
            _pending = false;
            _bytes_send = 0;
            return;
        }

        //队列不为空，则取出队首元素
        _current_block = _send_queue.dequeue();
        _bytes_send = 0;
        _pending = true;
        qint64 w2 = _socket.write(_current_block);
        qDebug() << "[TcpMgr]: Dequeued and write() returned" << w2;
    });

    //关闭socket
    connect(this, &TcpMgr::sig_close, this, &TcpMgr::slot_tcp_close);

    //注册消息处理
    initHandlers();

    qDebug() << "[TcpMgr]: Init Success!";
}

void TcpMgr::initializeVideoCallConnections()
{
    // 连接视频通话相关信号到VideoCallManager
    connect(this, &TcpMgr::sig_call_incoming,
            VideoCallManager::GetInstance().get(), &VideoCallManager::handleCallIncoming, Qt::UniqueConnection);
    connect(this, &TcpMgr::sig_accept_call,
            VideoCallManager::GetInstance().get(), &VideoCallManager::handleAcceptCall, Qt::UniqueConnection);
    connect(this, &TcpMgr::sig_call_accepted,
            VideoCallManager::GetInstance().get(), &VideoCallManager::handleCallAccept, Qt::UniqueConnection);
    connect(this, &TcpMgr::sig_call_rejected,
            VideoCallManager::GetInstance().get(), &VideoCallManager::handleCallReject, Qt::UniqueConnection);
    connect(this, &TcpMgr::sig_call_hangup,
            VideoCallManager::GetInstance().get(), &VideoCallManager::handleCallHangup, Qt::UniqueConnection);

    qDebug() << "[TcpMgr]: VideoCallManager connections initialized";
}

void TcpMgr::registerMetaType() {
    // 注册所有自定义类型
    qRegisterMetaType<ServerInfo>("ServerInfo");
    qRegisterMetaType<SearchInfo>("SearchInfo");
    qRegisterMetaType<std::shared_ptr<SearchInfo>>("std::shared_ptr<SearchInfo>");

    qRegisterMetaType<AddFriendApply>("AddFriendApply");
    qRegisterMetaType<std::shared_ptr<AddFriendApply>>("std::shared_ptr<AddFriendApply>");

    qRegisterMetaType<ApplyInfo>("ApplyInfo");

    qRegisterMetaType<std::shared_ptr<AuthInfo>>("std::shared_ptr<AuthInfo>");

    qRegisterMetaType<AuthRsp>("AuthRsp");
    qRegisterMetaType<std::shared_ptr<AuthRsp>>("std::shared_ptr<AuthRsp>");

    qRegisterMetaType<UserInfo>("UserInfo");

    qRegisterMetaType<std::vector<std::shared_ptr<TextChatData>>>("std::vector<std::shared_ptr<TextChatData>>");

    qRegisterMetaType<std::vector<std::shared_ptr<ChatThreadInfo>>>("std::vector<std::shared_ptr<ChatThreadInfo>>");

    qRegisterMetaType<std::shared_ptr<ChatThreadData>>("std::shared_ptr<ChatThreadData>");
    qRegisterMetaType<ReqId>("ReqId");
    qRegisterMetaType<std::shared_ptr<ImgChatData>>("std::shared_ptr<ImgChatData>");
    qRegisterMetaType<std::vector<std::shared_ptr<ChatDataBase>>>("std::vector<std::shared_ptr<ChatDataBase>>");
    
    // 注册视频通话相关类型
    qRegisterMetaType<VideoCallState>("VideoCallState");
}

void TcpMgr::CloseConnection(){
    emit sig_close();
}

void TcpMgr::SendData(ReqId reqId, QByteArray data)
{
    emit sig_send_data(reqId, data);
}

void TcpMgr::initHandlers()
{
    //注册获取登录回包逻辑（跳转到聊天页面）
    _handlers.insert(ReqId::ID_CHAT_LOGIN_RSP, [this](ReqId id,int len,QByteArray data){
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle id is: " << id << " data is: " << data << " ---";
        //将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        //检查转换是否成功
        if(jsonDoc.isNull()){
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument";
            return;
        }
        //转换为QJsonObject
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: Login failed,err is Json Parse Err" << err;
            emit sig_login_failed(err);
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "[TcpMgr]: Login Failed, err is " << err ;
            emit sig_login_failed(err);
            return;
        }

        //接收服务器传回的个人信息
        auto uid = jsonObj["uid"].toInt();
        auto name = jsonObj["name"].toString();
        auto nick = jsonObj["nick"].toString();
        auto icon = jsonObj["icon"].toString();
        auto sex = jsonObj["sex"].toInt();
        //auto desc = jsonObj["desc"].toString();

        //设置个人信息和token
        //auto user_info = std::make_shared<UserInfo>(uid, name, nick, icon, sex,"",desc);
        auto user_info = std::make_shared<UserInfo>(uid, name, nick, icon, sex,"");
        UserMgr::GetInstance()->SetUserInfo(user_info);
        UserMgr::GetInstance()->SetToken(jsonObj["token"].toString());

        //如果有好友申请或好友信息，则加载一下
        if(jsonObj.contains("apply_list")){
            UserMgr::GetInstance()->AppendApplyList(jsonObj["apply_list"].toArray());
        }
        //如果有好友，则添加好友列表
        if (jsonObj.contains("friend_list")) {
            UserMgr::GetInstance()->AppendFriendList(jsonObj["friend_list"].toArray());
        }

        //登录成功，跳转到聊天页面
        emit sig_switch_chatdlg();
    });

    //注册获取搜索好友回包逻辑（跳转到添加好友）
    _handlers.insert(ReqId::ID_SEARCH_USER_RSP, [this](ReqId id,int len,QByteArray data){
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle id is: " << id << " data is: " << data << " ---";
        //将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        //检查转换是否成功
        if(jsonDoc.isNull()){
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument";
            return;
        }
        //转换为QJsonObject
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: Search User failed,err is Json Parse Err" << err;
            emit sig_user_search(nullptr);
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "[TcpMgr]: Search User Failed, err is " << err ;
            emit sig_user_search(nullptr);
            return;
        }

        //构造服务器返回的搜索到的好友信息
        auto search_info = std::make_shared<SearchInfo>(jsonObj["uid"].toInt(),
            jsonObj["name"].toString(),jsonObj["nick"].toString(),jsonObj["desc"].toString(),
                jsonObj["sex"].toInt(),jsonObj["icon"].toString());
        qDebug() << "[TcpMgr]: ---------- SearchInfo: server back icon is: " << jsonObj["icon"].toString() << " ------------";

        emit sig_user_search(search_info);
    });

    //注册获取发送好友申请回包逻辑
    _handlers.insert(ReqId::ID_ADD_FRIEND_RSP, [this](ReqId id,int len,QByteArray data){
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle id is: " << id << " data is: " << data << " ---";
        //将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        //检查转换是否成功
        if(jsonDoc.isNull()){
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument";
            return;
        }
        //转换为QJsonObject
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: Add Friend RSP failed,err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "[TcpMgr]: Add Friend RSP Failed, err is " << err ;
            return;
        }

        qDebug()<< "[TcpMgr]: Add Friend RSP success !";
    });

    //注册收到好友申请逻辑
    _handlers.insert(ReqId::ID_NOTIFY_ADD_FRIEND_REQ, [this](ReqId id,int len,QByteArray data){
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle id is: " << id << " data is: " << data << " ---";
        //将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        //检查转换是否成功
        if(jsonDoc.isNull()){
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument";
            return;
        }
        //转换为QJsonObject
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: NOTIFY_ADD_FRIEND REQ failed,err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "[TcpMgr]: NOTIFY_ADD_FRIEND REQ Failed, err is " << err ;
            return;
        }

        //解析服务器发来的好友申请
        int from_uid = jsonObj["applyuid"].toInt();
        QString name = jsonObj["name"].toString();
        QString desc = jsonObj["desc"].toString();
        QString icon = jsonObj["icon"].toString();
        QString nick = jsonObj["nick"].toString();
        int sex = jsonObj["sex"].toInt();

        //构造服务器发来的好友申请
        auto apply_info = std::make_shared<AddFriendApply>(from_uid,name,desc,icon,nick,sex);

        //显示好友申请
        emit sig_friend_apply(apply_info);

        qDebug()<< "[TcpMgr]: --- NOTIFY_ADD_FRIEND REQ success ! ---";
    });

    //注册通知用户好友申请被对方通过逻辑
    _handlers.insert(ReqId::ID_NOTIFY_AUTH_FRIEND_REQ, [this](ReqId id,int len,QByteArray data){
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle id is: " << id << " data is: " << data << " ---";
        //将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        //检查转换是否成功
        if(jsonDoc.isNull()){
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument";
            return;
        }
        //转换为QJsonObject
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: AUTH_FRIEND failed,err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "[TcpMgr]: AUTH_FRIEND Failed, err is " << err ;
            return;
        }

        //解析服务器发来的同意好友申请信息
        int from_uid = jsonObj["fromuid"].toInt();
        QString name = jsonObj["name"].toString();
        QString icon = jsonObj["icon"].toString();
        QString nick = jsonObj["nick"].toString();
        int sex = jsonObj["sex"].toInt();

        std::vector<std::shared_ptr<TextChatData>> chat_datas;
        for (const QJsonValue& data : jsonObj["chat_datas"].toArray()) {
            auto send_uid = data["sender"].toInt();
            auto msg_id = data["msg_id"].toInt();
            auto thread_id = data["thread_id"].toInt();
            auto unique_id = data["unique_id"].toInt();
            auto msg_content = data["msg_content"].toString();
            //这里status直接设为已读2
            auto chat_data = std::make_shared<TextChatData>(msg_id, thread_id, ChatFormType::PRIVATE,
                ChatMsgType::TEXT, msg_content, send_uid, 2);
            chat_datas.push_back(chat_data);
        }

        //构造服务器发来的同意好友信息
        auto auth_info = std::make_shared<AuthInfo>(from_uid,name,nick,icon,sex);
        auth_info->SetChatDatas(chat_datas);

        //将新加的好友显示出来
        emit sig_add_auth_friend(auth_info);

        qDebug()<< "[TcpMgr]: AUTH_FRIEND success !";
    });

    //注册通知用户成功审核好友申请逻辑
    _handlers.insert(ReqId::ID_AUTH_FRIEND_RSP, [this](ReqId id,int len,QByteArray data){
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle id is: " << id << " data is: " << data << " ---";
        //将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        //检查转换是否成功
        if(jsonDoc.isNull()){
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument";
            return;
        }
        //转换为QJsonObject
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: AUTH_FRIEND_RSP failed,err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "[TcpMgr]: AUTH_FRIEND_RSP Failed, err is " << err ;
            return;
        }

        //解析服务器发来的成功审核好友申请,对方的申请信息
        QString name = jsonObj["name"].toString();
        QString icon = jsonObj["icon"].toString();
        QString nick = jsonObj["nick"].toString();
        int sex = jsonObj["sex"].toInt();
        auto uid = jsonObj["uid"].toInt();

        std::vector<std::shared_ptr<TextChatData>> chat_datas;
        for (const QJsonValue& data : jsonObj["chat_datas"].toArray()) {
            auto send_uid = data["sender"].toInt();
            auto msg_id = data["msg_id"].toInt();
            auto thread_id = data["thread_id"].toInt();
            auto unique_id = data["unique_id"].toInt();
            auto msg_content = data["msg_content"].toString();
            //这里status直接设为已读2
            auto chat_data = std::make_shared<TextChatData>(msg_id, thread_id, ChatFormType::PRIVATE,
                ChatMsgType::TEXT, msg_content, send_uid, 2);
            chat_datas.push_back(chat_data);
        }

        //构造服务器发来的成功审核好友信息
        auto rsp = std::make_shared<AuthRsp>(uid,name,nick,icon,sex);
        rsp->SetChatDatas(chat_datas);

        //将新加的好友显示出来
        emit sig_auth_rsp(rsp);

        qDebug()<< "[TcpMgr]: AUTH_FRIEND_RSP success !";
    });

    //异地登录或连接异常通知下线
    _handlers.insert(ID_NOTIFY_OFF_LINE_REQ,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: Notify Chat Msg Failed, err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: Notify Chat Msg Failed, err is " << err;
            return;
        }

        auto uid = jsonObj["uid"].toInt();
        qDebug() << "[TcpMgr]: --- Receive offline Notify Success, uid is " << uid <<" ---";
        //异地登录发送通知到界面
        emit sig_notify_offline();
    });

    //注册心跳信息服务器回包
    _handlers.insert(ID_HEART_BEAT_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: HEART_BEAT_RSP Failed, err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: HEART_BEAT_RSP Failed, err is " << err;
            return;
        }

        qDebug() << "[TcpMgr]: --- Receive HEART_BEAT_RSP Success ---";
    });

    //注册请求聊天列表的服务器回包
    _handlers.insert(ID_LOAD_CHAT_THREAD_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument.";
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: chat thread json parse failed " << err;
            return;
        }
        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: get chat thread rsp failed, error is " << err;
            return;
        }

        qDebug() << "[TcpMgr]: --- Receive chat thread rsp Success ---";

        auto thread_array = jsonObj["threads"].toArray();
        std::vector<std::shared_ptr<ChatThreadInfo>> chat_threads;
        for (const QJsonValue& value : thread_array) {
            auto cti = std::make_shared<ChatThreadInfo>();
            cti->_thread_id = value["thread_id"].toInt();
            cti->_type = value["type"].toString();
            cti->_user1_id = value["user1_id"].toInt();
            cti->_user2_id = value["user2_id"].toInt();
            chat_threads.push_back(cti);
        }
        bool load_more = jsonObj["load_more"].toBool();
        int next_last_id = jsonObj["next_last_id"].toInt();
        //发送信号通知界面
        emit sig_load_chat_thread(load_more, next_last_id, chat_threads);
    });

    //注册接收到服务器传来的创建新私人对话的回包
    _handlers.insert(ID_CREATE_PRIVATE_CHAT_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: parse create private chat json parse failed " << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: get create private chat failed, error is " << err;
            return;
        }

        qDebug() << "[TcpMgr]: --- Receive create private chat rsp Success ---";

        int uid = jsonObj["uid"].toInt();
        int other_id = jsonObj["other_id"].toInt();
        int thread_id = jsonObj["thread_id"].toInt();

        //发送信号通知界面
        emit sig_create_private_chat(uid, other_id, thread_id);
    });

    //注册向服务器请求查询当前聊天对话内容的请求，服务器的回包
    _handlers.insert(ID_LOAD_CHAT_MSG_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: parse Load Chat Msg Rsp json parse failed " << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: get Load Chat Msg Rsp failed, error is " << err;
            return;
        }

        qDebug() << "[TcpMgr]: --- Receive Load Chat Msg Rsp Success ---";

        int thread_id = jsonObj["thread_id"].toInt();
        int last_msg_id = jsonObj["last_msg_id"].toInt();
        bool load_more = jsonObj["load_more"].toBool();

        std::vector<std::shared_ptr<ChatDataBase>> chat_datas;
        for(const QJsonValue& data : jsonObj["chat_datas"].toArray()){
            auto send_uid = data["sender"].toInt();
            auto msg_id = data["msg_id"].toInt();
            auto thread_id = data["thread_id"].toInt();
            auto unique_id = data["unique_id"].toInt();
            auto msg_content = data["msg_content"].toString();
            auto status = data["status"].toInt();
            QString chat_time = data["chat_time"].toString();
            int recv_id = data["receiver"].toInt();
            int msg_type = data["msg_type"].toInt();

            if (msg_type == int(ChatMsgType::TEXT)) {
                auto chat_data = std::make_shared<TextChatData>(msg_id, thread_id, ChatFormType::PRIVATE,
                        ChatMsgType::TEXT, msg_content, send_uid, status, chat_time);
                chat_datas.push_back(chat_data);
                continue;
            }

            if (msg_type == int(ChatMsgType::PIC)) {
                auto uid = UserMgr::GetInstance()->GetUid();
                QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
                QString img_path_str = storageDir + "/user/" + QString::number(uid) + "/chatimg/" + QString::number(send_uid);
                QString img_path = img_path_str + "/" + msg_content;
                //文件不存在，则创建空白图片占位，同时组织数据准备发送
                if (QFile::exists(img_path) == false) {
                    CreatePlaceholderImgMsgL(img_path_str, msg_content,
                                             msg_id, thread_id, send_uid, recv_id, status, chat_time,
                                             chat_datas);
                    continue;
                }
                //如果文件存在
                //如果文件存在则直接构建MsgInfo
                // 获取文件大小
                QFileInfo fileInfo(img_path);
                qint64 file_size = fileInfo.size();
                //从文件路径加载QPixmap
                QPixmap pixmap(img_path);
                //如果图片加载失败，也是创建占位符，然后组织发送
                if (pixmap.isNull()) {
                    CreatePlaceholderImgMsgL(img_path_str, msg_content,
                                             msg_id, thread_id, send_uid, recv_id, status, chat_time,
                                             chat_datas);
                    continue;
                }

                //说明图片加载正确，构建真实图片
                auto  file_info = std::make_shared<MsgInfo>(MsgType::IMG_MSG, img_path_str,
                                                           pixmap, msg_content, file_size, "");
                file_info->_msg_id = msg_id;
                file_info->_sender = send_uid;
                file_info->_receiver = recv_id;
                file_info->_thread_id = thread_id;
                //设置文件传输的类型
                file_info->_transfer_type = TransferType::Download;
                //设置文件传输状态
                file_info->_transfer_state = TransferState::None;
                //放入chat_datas列表
                auto chat_data = std::make_shared<ImgChatData>(file_info,"", thread_id, ChatFormType::PRIVATE,
                                                               ChatMsgType::PIC, send_uid, status, chat_time);
                chat_datas.push_back(chat_data);

                continue;
            }

            if (msg_type == int(ChatMsgType::FILE)) {
                auto uid = UserMgr::GetInstance()->GetUid();
                QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
                QString file_path_str = storageDir + "/user/" + QString::number(uid) + "/chatfile/" + QString::number(send_uid);
                
                QString unique_name = msg_content;
                QString origin_name = msg_content;
                // 解析 unique_name|origin_name
                int pos = msg_content.indexOf('|');
                if (pos != -1) {
                    unique_name = msg_content.left(pos);
                    origin_name = msg_content.mid(pos + 1);
                }

                QString file_path = file_path_str + "/" + unique_name;
                
                QFileInfo fileInfo(file_path);
                qint64 file_size = 0;
                QPixmap pix;
                TransferState transfer_state = TransferState::None;
                QString text_or_url = file_path_str; // 默认使用目录，用于下载
                
                if (QFile::exists(file_path)) {
                    file_size = fileInfo.size();
                    QFileIconProvider provider;
                    pix = provider.icon(fileInfo).pixmap(40, 40);
                    transfer_state = TransferState::Completed;
                    text_or_url = file_path; // 已完成，使用全路径
                } else {
                    // 文件不存在，使用默认图标并标记为需要下载
                    pix = QPixmap(":/images/file.png");
                    transfer_state = TransferState::Downloading;
                    text_or_url = file_path_str; // 未完成，使用目录用于下载
                }

                auto file_info = std::make_shared<MsgInfo>(MsgType::FILE_MSG, text_or_url,
                                                           pix, unique_name, file_size, "", origin_name); 
                file_info->_msg_id = msg_id;
                file_info->_sender = send_uid;
                file_info->_receiver = recv_id;
                file_info->_thread_id = thread_id;
                file_info->_transfer_type = TransferType::Download;
                file_info->_transfer_state = transfer_state;

                auto chat_data = std::make_shared<FileChatData>(file_info, "", thread_id, ChatFormType::PRIVATE,
                                                               ChatMsgType::FILE, send_uid, status, chat_time);
                chat_datas.push_back(chat_data);

                // 如果本地没有且不是自己发的，则发起下载请求
                if (transfer_state == TransferState::Downloading && send_uid != uid) {
                    UserMgr::GetInstance()->AddTransFile(unique_name, file_info);
                    QJsonObject jsonObj_send;
                    jsonObj_send["name"] = unique_name;
                    jsonObj_send["seq"] = file_info->_seq;
                    jsonObj_send["trans_size"] = "0";
                    jsonObj_send["total_size"] = "0";
                    jsonObj_send["token"] = UserMgr::GetInstance()->GetToken();
                    jsonObj_send["sender_id"] = send_uid;
                    jsonObj_send["receiver_id"] = uid;
                    jsonObj_send["message_id"] = msg_id;
                    jsonObj_send["uid"] = uid;

                    QDir chatfileDir(file_path_str);
                    if (!chatfileDir.exists()) {
                        chatfileDir.mkpath(".");
                    }

                    QJsonDocument doc(jsonObj_send);
                    FileTcpMgr::GetInstance()->SendData(ID_FILE_CHAT_DOWN_REQ, doc.toJson());
                }
                continue;
            }
        }

        //发送信号通知界面
        emit sig_load_chat_msg(thread_id,last_msg_id,load_more,chat_datas);
    });

    //注册聊天chatpage将文本消息发送出去传给服务器，服务器的应答
    _handlers.insert(ID_TEXT_CHAT_MSG_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle id is " << id << " data is " << data << " ---";
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: Chat Msg Rsp Failed, err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: Chat Msg Rsp Failed, err is " << err;
            return;
        }

        qDebug() << "[TcpMgr]: --- Receive Text Chat Rsp Success ---" ;

        //ui设置送达等标记 todo...
        //收到消息后转发给页面
        auto thread_id = jsonObj["thread_id"].toInt();
        auto sender = jsonObj["fromuid"].toInt();

        std::vector<std::shared_ptr<TextChatData>> chat_datas;
        for(const QJsonValue& data : jsonObj["chat_datas"].toArray()){
            auto msg_id = data["message_id"].toInt();
            auto unique_id = data["unique_id"].toString();
            auto msg_content = data["content"].toString();
            QString chat_time = data["chat_time"].toString();
            int status = data["status"].toInt();
            auto chat_data = std::make_shared<TextChatData>(msg_id,unique_id, thread_id, ChatFormType::PRIVATE,
                                                            ChatMsgType::TEXT, msg_content, sender, status, chat_time);
            chat_datas.push_back(chat_data);
        }

        //发送信号通知界面
        emit sig_chat_msg_rsp(thread_id, chat_datas);
    });

    //注册聊天将文件消息发送出去传给服务器，服务器的应答
    _handlers.insert(ID_FILE_CHAT_MSG_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: handle id is " << id << " data is " << data;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull()) return;
        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error")) return;
        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) return;

        auto thread_id = jsonObj["thread_id"].toInt();
        auto unique_id = jsonObj["unique_id"].toString();
        auto unique_name = jsonObj["unique_name"].toString();
        auto sender = jsonObj["fromuid"].toInt();
        auto msg_id = jsonObj["message_id"].toInt();
        QString chat_time = jsonObj["chat_time"].toString();
        int status = jsonObj["status"].toInt();
        auto receiver = jsonObj["touid"].toInt();

        auto file_info = UserMgr::GetInstance()->GetTransFileByName(unique_name);
        if (!file_info) return;

        file_info->_msg_id = msg_id;
        file_info->_thread_id = thread_id;
        file_info->_transfer_type = TransferType::Upload;
        file_info->_transfer_state = TransferState::Uploading;
        file_info->_sender = sender;
        file_info->_receiver = receiver;

        auto chat_data = std::make_shared<FileChatData>(file_info, unique_id, thread_id, ChatFormType::PRIVATE,
                                                       ChatMsgType::FILE, sender, status, chat_time);
        chat_data->SetMsgId(msg_id);

        emit sig_chat_file_rsp(thread_id, chat_data);

        // 开始发送文件块
        QFile file(file_info->_text_or_url);
        if (!file.open(QIODevice::ReadOnly)) return;
        file.seek(file_info->_current_size);
        auto buffer = file.read(MAX_FILE_LEN);
        QString base64Data = buffer.toBase64();
        
        QJsonObject file_obj;
        file_obj["name"] = file_info->_unique_name;
        file_obj["unique_id"] = unique_id;
        file_obj["seq"] = file_info->_seq;
        file_info->_current_size = buffer.size() + (file_info->_seq - 1) * MAX_FILE_LEN;
        file_obj["trans_size"] = QString::number(file_info->_current_size);
        file_obj["total_size"] = QString::number(file_info->_total_size);
        file_obj["token"] = UserMgr::GetInstance()->GetToken();
        file_obj["md5"] = file_info->_md5;
        file_obj["uid"] = UserMgr::GetInstance()->GetUid();
        file_obj["data"] = base64Data;
        file_obj["message_id"] = msg_id;
        file_obj["receiver"] = receiver;
        file_obj["sender"] = sender;
        file_obj["last"] = (file_info->_current_size >= file_info->_total_size) ? 1 : 0;

        QJsonDocument doc_file(file_obj);
        FileTcpMgr::GetInstance()->SendData(ReqId::ID_FILE_INFO_SYNC_REQ, doc_file.toJson(QJsonDocument::Compact));
    });

    //注册通知用户接收到对方发来的文件消息逻辑
    _handlers.insert(ID_NOTIFY_FILE_CHAT_MSG_REQ, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: handle id is " << id << " data is " << data;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull()) return;
        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error")) return;
        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) return;

        auto from_uid = jsonObj["fromuid"].toInt();
        auto thread_id = jsonObj["thread_id"].toInt();
        auto msg_id = jsonObj["message_id"].toInt();
        auto msg_content = jsonObj["name"].toString();
        
        QString unique_name = msg_content;
        QString origin_name = msg_content;
        int pos = msg_content.indexOf('|');
        if (pos != -1) {
            unique_name = msg_content.left(pos);
            origin_name = msg_content.mid(pos + 1);
        }

        auto total_size = jsonObj["total_size"].toString().toULongLong();
        auto chat_time = jsonObj["chat_time"].toString();
        auto status = jsonObj["status"].toInt();
        auto md5 = jsonObj["md5"].toString();

        auto uid = UserMgr::GetInstance()->GetUid();
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString file_dir = storageDir + "/user/" + QString::number(uid) + "/chatfile/" + QString::number(from_uid);

        QFileIconProvider provider;
        QPixmap pix = provider.icon(QFileInfo(origin_name)).pixmap(40, 40);

        auto file_info = std::make_shared<MsgInfo>(MsgType::FILE_MSG, file_dir, pix, unique_name, total_size, md5, origin_name);
        file_info->_msg_id = msg_id;
        file_info->_sender = from_uid;
        file_info->_receiver = uid;
        file_info->_thread_id = thread_id;
        file_info->_transfer_type = TransferType::Download;
        file_info->_transfer_state = TransferState::Downloading;
        
        // 将文件信息加入管理
        UserMgr::GetInstance()->AddTransFile(unique_name, file_info);

        auto chat_data = std::make_shared<FileChatData>(file_info, "", thread_id, ChatFormType::PRIVATE,
                                                       ChatMsgType::FILE, from_uid, status, chat_time);
        emit sig_file_chat_msg(chat_data);

        // 自动发起下载请求
        QJsonObject jsonObj_send;
        jsonObj_send["name"] = unique_name;
        jsonObj_send["seq"] = file_info->_seq;
        jsonObj_send["trans_size"] = "0";
        jsonObj_send["total_size"] = QString::number(total_size);
        jsonObj_send["token"] = UserMgr::GetInstance()->GetToken();
        jsonObj_send["sender_id"] = from_uid;
        jsonObj_send["receiver_id"] = uid;
        jsonObj_send["message_id"] = msg_id;
        jsonObj_send["uid"] = uid;

        QDir chatfileDir(storageDir + "/user/" + QString::number(uid) + "/chatfile/" + QString::number(from_uid));
        if (!chatfileDir.exists()) {
            chatfileDir.mkpath(".");
        }

        QJsonDocument doc(jsonObj_send);
        FileTcpMgr::GetInstance()->SendData(ID_FILE_CHAT_DOWN_REQ, doc.toJson());
    });

    //注册聊天将图片消息发送出去传给服务器，服务器的应答
    _handlers.insert(ID_IMG_CHAT_MSG_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: parse IMG_CHAT_MSG_RSP json parse failed " << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: get IMG_CHAT_MSG_RSP failed, error is " << err;
            return;
        }

        qDebug() << "[TcpMgr]: Receive IMG_CHAT_MSG_RSP Success";

        //收到消息后转发给页面
        auto thread_id = jsonObj["thread_id"].toInt();
        auto unique_id = jsonObj["unique_id"].toString();
        auto unique_name = jsonObj["unique_name"].toString();

        auto sender = jsonObj["fromuid"].toInt();
        auto msg_id = jsonObj["message_id"].toInt();
        QString chat_time = jsonObj["chat_time"].toString();
        int status = jsonObj["status"].toInt();
        auto text_or_url = jsonObj["text_or_url"].toString();
        auto receiver = jsonObj["touid"].toInt();

        auto file_info = UserMgr::GetInstance()->GetTransFileByName(unique_name);

        //如果未找到文件对应的信息则返回
        if (!file_info) {
            return;
        }
        //设置消息id和会话id
        file_info->_msg_id = msg_id;
        file_info->_thread_id = thread_id;
        //设置文件传输的类型
        file_info->_transfer_type = TransferType::Upload;
        //设置文件传输状态
        file_info->_transfer_state = TransferState::Uploading;
        //设置发送者和接收者
        file_info->_sender = sender;
        file_info->_receiver = receiver;

        auto chat_data = std::make_shared<ImgChatData>(file_info, unique_id, thread_id, ChatFormType::PRIVATE,
                                                       ChatMsgType::PIC, sender, status, chat_time);

        //更新msg_id,因为最开始构造的chat_data中ImgChatData的msg_id为空
        chat_data->SetMsgId(msg_id);

        //发送信号通知聊天界面
        emit sig_chat_img_rsp(thread_id, chat_data);

        //管理消息，添加序列号到正在发送集合
        file_info->_flighting_seqs.insert(file_info->_seq);

        //发送消息
        QFile file(file_info->_text_or_url);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "[TcpMgr]: Could not open file:" << file.errorString();
            return;
        }

        file.seek(file_info->_current_size);
        auto buffer = file.read(MAX_FILE_LEN);
        qDebug() << "buffer is " << buffer;
        //将文件内容转换为base64编码
        QString base64Data = buffer.toBase64();
        QJsonObject file_obj;
        file_obj["name"] = file_info->_unique_name;
        file_obj["unique_id"] = unique_id;
        file_obj["seq"] = file_info->_seq;
        file_info->_current_size = buffer.size() + (file_info->_seq - 1) * MAX_FILE_LEN;
        file_obj["trans_size"] = QString::number(file_info->_current_size);
        file_obj["total_size"] = QString::number(file_info->_total_size);
        file_obj["token"] = UserMgr::GetInstance()->GetToken();
        file_obj["md5"] = file_info->_md5;
        file_obj["uid"] = UserMgr::GetInstance()->GetUid();
        file_obj["data"] = base64Data;
        file_obj["message_id"] = msg_id;
        file_obj["receiver"] = receiver;
        file_obj["sender"] = sender;

        if (buffer.size() + (file_info->_seq - 1) * MAX_FILE_LEN >= file_info->_total_size) {
            file_obj["last"] = 1;
        }
        else {
            file_obj["last"] = 0;
        }

        //发送文件  todo 留作以后收到服务器返回消息后再发送
        QJsonDocument doc_file(file_obj);
        QByteArray fileData = doc_file.toJson(QJsonDocument::Compact);

        //发送消息给ResourceServer
        FileTcpMgr::GetInstance()->SendData(ReqId::ID_FILE_INFO_SYNC_REQ, fileData);
    });

    //注册有消息的通知显示，接收对方发来的消息
    _handlers.insert(ID_NOTIFY_TEXT_CHAT_MSG_REQ, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle id is " << id << " data is " << data << " ---";
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to create QJsonDocument";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "[TcpMgr]: Notify Chat Msg Failed, err is Json Parse Err" << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: Notify Chat Msg Failed, err is " << err;
            return;
        }

        qDebug() << "[TcpMgr]: --- Receive Text Chat Notify Success ---";

        auto thread_id = jsonObj["thread_id"].toInt();
        auto sender = jsonObj["fromuid"].toInt();

        std::vector<std::shared_ptr<TextChatData>> chat_datas;
        // 遍历 QJsonArray 并输出每个元素
        for (const QJsonValue& data : jsonObj["chat_datas"].toArray()) {
            int msg_id = data["message_id"].toInt();
            QString unique_id = data["unique_id"].toString();
            QString content = data["content"].toString();
            QString chat_time = data["chat_time"].toString();
            int status = data["status"].toInt();

            auto chat_data = std::make_shared<TextChatData>(msg_id, unique_id, thread_id, ChatFormType::PRIVATE,
                                                            ChatMsgType::TEXT, content, sender, status, chat_time);
            chat_datas.push_back(chat_data);
        }

        emit sig_text_chat_msg(chat_datas);
    });

    _handlers.insert(ID_NOTIFY_IMG_CHAT_MSG_REQ, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]:handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]:Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        qDebug() << "[TcpMgr]:receive notify img chat msg req success" ;


        //收到消息后转发给页面
        auto thread_id = jsonObj["thread_id"].toInt();
        auto sender_id = jsonObj["sender_id"].toInt();
        auto message_id = jsonObj["message_id"].toInt();
        auto receiver_id = jsonObj["receiver_id"].toInt();
        auto img_name = jsonObj["img_name"].toString();
        auto total_size_str = jsonObj["total_size"].toString();
        auto total_size = total_size_str.toLongLong();
        auto uid = UserMgr::GetInstance()->GetUid();

        //客户端存储聊天记录，按照如下格式存储C:\Users\hp\AppData\Roaming\qchat\chatimg\uid, uid为对方uid
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString img_path_str = storageDir + "/user/" + QString::number(uid) + "/chatimg/" + QString::number(sender_id);
        auto file_info = UserMgr::GetInstance()->GetTransFileByName(img_name);
        //正常情况是找不到的，所以这里初始化一个文件信息放入UserMgr中管理
        if (!file_info) {
            //预览图先默认空白，md5为空
            file_info = std::make_shared<MsgInfo>(MsgType::IMG_MSG, img_path_str, CreateLoadingPlaceholder(200, 200), img_name, total_size, "");
            UserMgr::GetInstance()->AddTransFile(img_name, file_info);
        }

        file_info->_msg_id = message_id;
        file_info->_sender = sender_id;
        file_info->_receiver = receiver_id;
        file_info->_thread_id = thread_id;
        //设置文件传输的类型
        file_info->_transfer_type = TransferType::Download;
        //设置文件传输状态
        file_info->_transfer_state = TransferState::Downloading;

        auto img_chat_data_ptr = std::make_shared<ImgChatData>(file_info, "",
                thread_id, ChatFormType::PRIVATE, ChatMsgType::PIC, sender_id, MsgStatus::READED);

        //通知接收对方发来的图片消息
        emit sig_img_chat_msg(img_chat_data_ptr);

        //组织请求，准备下载
        QJsonObject jsonObj_send;
        jsonObj_send["name"] = img_name;
        jsonObj_send["seq"] = file_info->_seq;
        jsonObj_send["trans_size"] = "0";
        jsonObj_send["total_size"] = QString::number(file_info->_total_size);
        jsonObj_send["token"] = UserMgr::GetInstance()->GetToken();
        jsonObj_send["sender_id"] = sender_id;
        jsonObj_send["receiver_id"] = receiver_id;
        jsonObj_send["message_id"] = message_id;
        jsonObj_send["uid"] = uid;
        //客户端存储聊天记录，按照如下格式存储C:\Users\hp\AppData\Roaming\qchat\chatimg\uid, uid为对方uid
        QDir chatimgDir(img_path_str);
        jsonObj["client_path"] = img_path_str;
        if (!chatimgDir.exists()) {
            chatimgDir.mkpath(".");  // 创建当前路径
        }

        QJsonDocument doc(jsonObj_send);
        auto send_data = doc.toJson();
        FileTcpMgr::GetInstance()->SendData(ID_IMG_CHAT_DOWN_REQ, send_data);
    });
    
    // 注册视频通话邀请请求处理
    _handlers.insert(ID_CALL_INVITE_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle call invite rsp, id: " << id << " data: " << data << " ---";
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to parse CALL_INVITE_RSP JSON";
            return;
        }
        
        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error")) {
            qDebug() << "[TcpMgr]: CALL_INVITE_RSP JSON Parse Err";
            return;
        }
        
        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: CALL_INVITE_RSP Failed, err: " << err;
            // todo...可以在这里处理错误，例如通知用户对方不在线等
            return;
        }
        
        // 如果需要通知UI层邀请发送成功，可以发出信号
        qDebug() << "[TcpMgr]: Call invite sent successfully";
    });
    
    // 注册来电通知处理
    _handlers.insert(ID_CALL_INCOMING_NOTIFY, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle call incoming notify, id: " << id << " data: " << data << " ---";
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to parse CALL_INCOMING_NOTIFY JSON";
            return;
        }
        
        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error") || jsonObj["error"].toInt() != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: CALL_INCOMING_NOTIFY Parse Err or Failed";
            return;
        }
        
        int caller_uid = jsonObj["caller_uid"].toInt();
        QString call_id = jsonObj["call_id"].toString();
        QString caller_nick = jsonObj["caller_nick"].toString();
        
        // 发送信号到UI层处理来电
        emit sig_call_incoming(caller_uid, call_id, caller_nick);
        qDebug() << "[TcpMgr]: Incoming call from user" << caller_uid << ", nick:" << caller_nick;
    });

    // 注册通话接受服务器回包
    _handlers.insert(ID_CALL_ACCEPT_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle call accept notify, id: " << id << " data: " << data << " ---";
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to parse CALL_ACCEPT_NOTIFY JSON";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error") || jsonObj["error"].toInt() != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: CALL_ACCEPT_RSP Parse Err or Failed";
            return;
        }

        QString call_id = jsonObj["call_id"].toString();
        QString room_id = jsonObj["room_id"].toString();
        QString turn_ws_url = jsonObj["turn_ws_url"].toString();
        QJsonArray ice_servers = jsonObj["ice_servers"].toArray();

        // 发送信号到UI层处理通话接受
        emit sig_accept_call(call_id, room_id, turn_ws_url, ice_servers);
        qDebug() << "[TcpMgr]: Call accepted, room:" << room_id;
    });
    
    // 注册通话接受通知处理
    _handlers.insert(ID_CALL_ACCEPT_NOTIFY, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle call accept notify, id: " << id << " data: " << data << " ---";
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to parse CALL_ACCEPT_NOTIFY JSON";
            return;
        }
        
        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error") || jsonObj["error"].toInt() != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: CALL_ACCEPT_NOTIFY Parse Err or Failed";
            return;
        }
        
        QString call_id = jsonObj["call_id"].toString();
        QString room_id = jsonObj["room_id"].toString();
        QString turn_ws_url = jsonObj["turn_ws_url"].toString();
        QJsonArray ice_servers = jsonObj["ice_servers"].toArray();
        
        // 发送信号到UI层处理通话接受
        emit sig_call_accepted(call_id, room_id, turn_ws_url, ice_servers);
        qDebug() << "[TcpMgr]: Call accepted, room:" << room_id;
    });

    // 注册通话接受服务器回包
    _handlers.insert(ID_CALL_REJECT_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle call REJECT_RSP, id: " << id << " data: " << data << " ---";
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to parse CALL_REJECT_RSP JSON";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error") || jsonObj["error"].toInt() != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: CALL_REJECT_RSP Parse Err or Failed";
            return;
        }

        qDebug() << "[TcpMgr]: Reject Call Success!";
    });
    
    // 注册通话拒绝通知处理
    _handlers.insert(ID_CALL_REJECT_NOTIFY, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle call reject notify, id: " << id << " data: " << data << " ---";
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to parse CALL_REJECT_NOTIFY JSON";
            return;
        }
        
        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error") || jsonObj["error"].toInt() != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: CALL_REJECT_NOTIFY Parse Err or Failed";
            return;
        }
        
        QString call_id = jsonObj["call_id"].toString();
        QString reason = jsonObj["reason"].toString();
        
        // 发送信号到UI层处理通话拒绝
        emit sig_call_rejected(call_id, reason);
        qDebug() << "[TcpMgr]: Call rejected, reason:" << reason;
    });
    
    // 注册通话挂断通知处理
    _handlers.insert(ID_CALL_HANGUP_NOTIFY, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "[TcpMgr]: --- handle call hangup notify, id: " << id << " data: " << data << " ---";
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull()) {
            qDebug() << "[TcpMgr]: Failed to parse CALL_HANGUP_NOTIFY JSON";
            return;
        }
        
        QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error") || jsonObj["error"].toInt() != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr]: CALL_HANGUP_NOTIFY Parse Err or Failed";
            return;
        }
        
        QString call_id = jsonObj["call_id"].toString();
        
        // 发送信号到UI层处理通话挂断
        emit sig_call_hangup(call_id);
        qDebug() << "[TcpMgr]: Call hangup received";
    });
}

void TcpMgr::handleMsg(ReqId id, int len, QByteArray data)
{
    auto find_iter =  _handlers.find(id);
    if(find_iter == _handlers.end()){
        qDebug()<< "[TcpMgr]: not found id ["<< id << "] to handle";
        return ;
    }

    find_iter.value()(id,len,data);
}

void TcpMgr::CreatePlaceholderImgMsgL(QString img_path_str, QString msg_content, int msg_id,
            int thread_id, int send_uid, int recv_id, int status, QString chat_time,
            std::vector<std::shared_ptr<ChatDataBase> > &chat_datas)
{
    //如果加载失败，则使用占位符使图片变为空白，并且md5为空
    auto  file_info = std::make_shared<MsgInfo>(MsgType::IMG_MSG, img_path_str,
        CreateLoadingPlaceholder(200, 200), msg_content, 0, "");
    file_info->_msg_id = msg_id;
    file_info->_sender = send_uid;
    file_info->_receiver = recv_id;
    file_info->_thread_id = thread_id;
    //设置文件传输的类型
    file_info->_transfer_type = TransferType::Download;
    //设置文件传输状态
    file_info->_transfer_state = TransferState::Downloading;
    file_info->_rsp_size = file_info->_current_size;
    //放入chat_datas列表
    auto chat_data = std::make_shared<ImgChatData>(file_info, "", thread_id, ChatFormType::PRIVATE,
        ChatMsgType::PIC, send_uid, status, chat_time);
    chat_datas.push_back(chat_data);
    //加入下载列表，并且发送下载请求
    UserMgr::GetInstance()->AddTransFile(msg_content, file_info);

    QJsonObject jsonObj_send;
    jsonObj_send["message_id"] = file_info->_msg_id;
    QJsonDocument doc(jsonObj_send);
    auto send_data = doc.toJson();
    // 从服务器获取文件大小，然后请求下载
    FileTcpMgr::GetInstance()->SendData(ID_IMG_CHAT_DOWN_INFO_SYNC_REQ, send_data);
}

void TcpMgr::slot_tcp_close() {
    _socket.close();
}

void TcpMgr::slot_tcp_connect(std::shared_ptr<ServerInfo> si)
{
    qDebug()<< "[TcpMgr]: receive tcp connect signal";
    // 尝试连接到服务器
    qDebug() << "[TcpMgr]: Connecting to chat server...";
    _host = si->_chat_host;
    _port = static_cast<uint16_t>(si->_chat_port.toUInt());
    _socket.connectToHost(_host, _port);
}

void TcpMgr::slot_send_data(ReqId reqId, QByteArray dataBytes)
{
    uint16_t id = reqId;

    // 计算长度（使用网络字节序转换）
    quint16 len = static_cast<quint16>(dataBytes.length());

    // 创建一个QByteArray用于存储要发送的所有数据
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    // 设置数据流使用网络字节序
    out.setByteOrder(QDataStream::BigEndian);

    // 写入ID和长度
    out << id << len;

    // 添加字符串数据
    block.append(dataBytes);

    //判断是否正在发送
    if(_pending){
        //放入发送队列直接返回，因为目前有数据正在发送
        _send_queue.enqueue(block);
        return;
    }
    //没有正在发送，把这个包设为当前块，重置计数，并写进去
    _current_block = block;     //保存当前正在发送的block
    _bytes_send = 0;            //归零
    _pending = true;            //标记正在发送

    // 发送数据
    _socket.write(_current_block);
    qDebug() << "[TcpMgr]: tcp mgr send byte data is " << block ;
}
