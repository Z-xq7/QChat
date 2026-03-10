#include "tcpmgr.h"
#include <QAbstractSocket>
#include "usermgr.h"
#include <QJsonObject>
#include <QJsonArray>
#include "userdata.h"
#include <QTimer>

TcpThread::TcpThread()
{
    _tcp_thread = new QThread();
    TcpMgr::GetInstance()->moveToThread(_tcp_thread);
    QObject::connect(_tcp_thread, &QThread::finished, _tcp_thread, &QObject::deleteLater);
    _tcp_thread->start();
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

        QDataStream stream(&_buffer, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_5_0);

        forever {
            //先解析头部
            if(!_b_recv_pending){
                // 检查缓冲区中的数据是否足够解析出一个消息头（消息ID + 消息长度）
                if (_buffer.size() < static_cast<int>(sizeof(quint16) * 2)) {
                    return; // 数据不够，等待更多数据
                }

                // 预读取消息ID和消息长度，但不从缓冲区中移除
                stream >> _message_id >> _message_len;

                //将buffer 中的前四个字节移除
                _buffer = _buffer.mid(sizeof(quint16) * 2);

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

        std::vector<std::shared_ptr<TextChatData>> chat_datas;
        for(const QJsonValue& data : jsonObj["chat_datas"].toArray()){
            auto send_uid = data["sender"].toInt();
            auto msg_id = data["msg_id"].toInt();
            auto thread_id = data["thread_id"].toInt();
            auto unique_id = data["unique_id"].toInt();
            auto msg_content = data["msg_content"].toString();
            auto status = data["status"].toInt();
            QString chat_time = data["chat_time"].toString();

            auto chat_data = std::make_shared<TextChatData>(msg_id,thread_id,ChatFormType::PRIVATE,
                ChatMsgType::TEXT,msg_content,send_uid, status, chat_time);

            chat_datas.push_back(chat_data);
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

    //注册聊天chatpage将图片消息发送出去传给服务器，服务器的应答
    _handlers.insert(ID_IMG_CHAT_MSG_RSP, [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len);
        qDebug() << "handle id is " << id << " data is " << data;
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            qDebug() << "Failed to create QJsonDocument.";
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "parse create private chat json parse failed " << err;
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "get create private chat failed, error is " << err;
            return;
        }

        qDebug() << "Receive create private chat rsp Success";

        //收到消息后转发给页面
        auto thread_id = jsonObj["thread_id"].toInt();
        auto unique_id = jsonObj["unique_id"].toString();
        auto unique_name = jsonObj["unique_name"].toString();

        auto sender = jsonObj["fromuid"].toInt();
        auto msg_id = jsonObj["message_id"].toInt();
        QString chat_time = jsonObj["chat_time"].toString();
        int status = jsonObj["status"].toInt();

        auto file_info = UserMgr::GetInstance()->GetTransFileByName(unique_name);

        auto chat_data = std::make_shared<ImgChatData>(file_info, unique_id, thread_id, ChatFormType::PRIVATE,
                                                       ChatMsgType::TEXT, sender, status, chat_time);

        //发送信号通知界面
        emit sig_chat_img_rsp(thread_id, chat_data);
    });

    //注册有消息的通知显示，接收对方发来的消息
    _handlers.insert(ID_NOTIFY_TEXT_CHAT_MSG_REQ, [this](ReqId id, int len, QByteArray data) {
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









