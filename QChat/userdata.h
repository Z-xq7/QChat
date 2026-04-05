#ifndef USERDATA_H
#define USERDATA_H
#include <QString>
#include <memory>
#include <vector>
#include <QJsonArray>
#include <QJsonObject>
#include "global.h"

class SearchInfo
{
public:
    SearchInfo(int uid,QString name,QString nick,QString desc,int sex,QString icon);
    SearchInfo() = default;

    int _uid;
    QString _name;
    QString _nick;
    QString _desc;
    int _sex;
    QString _icon;
};
Q_DECLARE_METATYPE(SearchInfo)
Q_DECLARE_METATYPE(std::shared_ptr<SearchInfo>)

class AddFriendApply{
public:
    AddFriendApply(int from_uid,QString name,QString desc,QString icon,QString nick,int sex);
    AddFriendApply() = default;

    int _from_uid;
    QString _name;
    QString _desc;
    QString _icon;
    QString _nick;
    int _sex;
};
Q_DECLARE_METATYPE(std::shared_ptr<AddFriendApply>)

struct ApplyInfo{
    ApplyInfo(int uid,QString name,QString desc,QString icon,QString nick,int sex,int status):
        _uid(uid),_name(name),_desc(desc),_icon(icon),_nick(nick),_sex(sex),_status(status){}

    ApplyInfo(std::shared_ptr<AddFriendApply> addinfo):
        _uid(addinfo->_from_uid),_name(addinfo->_name),_desc(addinfo->_desc),_icon(addinfo->_icon),
        _nick(addinfo->_nick),_sex(addinfo->_sex),_status(0){}

    ApplyInfo() = default;

    void SetIcon(QString head){
        _icon = head;
    }

    int _uid;
    QString _name;
    QString _desc;
    QString _icon;
    QString _nick;
    int _sex;
    int _status;
};

struct TextChatData;
struct AuthInfo {
    AuthInfo(int uid, QString name,
             QString nick, QString icon, int sex):
        _uid(uid), _name(name), _nick(nick), _icon(icon),
        _sex(sex),_thread_id(0){}

    AuthInfo() = default;

    void SetChatDatas(std::vector<std::shared_ptr<TextChatData>> _chat_datas);

    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    int _thread_id;
    std::vector<std::shared_ptr<TextChatData>> _chat_datas;
};
Q_DECLARE_METATYPE(std::shared_ptr<AuthInfo>)

struct AuthRsp {
    AuthRsp() = default;
    AuthRsp(int peer_uid, QString peer_name,
            QString peer_nick, QString peer_icon, int peer_sex)
        :_uid(peer_uid),_name(peer_name),_nick(peer_nick),
        _icon(peer_icon),_sex(peer_sex),_thread_id(0){}

    void SetChatDatas(std::vector<std::shared_ptr<TextChatData>> _chat_datas);

    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    int _thread_id;
    std::vector<std::shared_ptr<TextChatData>> _chat_datas;
};
Q_DECLARE_METATYPE(std::shared_ptr<AuthRsp>)

//用户个人信息
struct UserInfo {
    UserInfo(int uid, QString name, QString nick, QString icon, int sex, QString last_msg = "", QString desc=""):
        _uid(uid),_name(name),_nick(nick),_icon(icon),_sex(sex),_desc(desc){}

    UserInfo(std::shared_ptr<AuthInfo> auth):
        _uid(auth->_uid),_name(auth->_name),_nick(auth->_nick),
        _icon(auth->_icon),_sex(auth->_sex),_desc(""){}

    UserInfo(int uid, QString name, QString icon):
        _uid(uid), _name(name),_nick(_name), _icon(icon),
        _sex(0),_desc(""){}

    UserInfo(std::shared_ptr<AuthRsp> auth):
        _uid(auth->_uid),_name(auth->_name),_nick(auth->_nick),
        _icon(auth->_icon),_sex(auth->_sex){}

    UserInfo(std::shared_ptr<SearchInfo> search_info):
        _uid(search_info->_uid),_name(search_info->_name),_nick(search_info->_nick),
        _icon(search_info->_icon),_sex(search_info->_sex){}

    UserInfo() = default;

    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    QString _desc;
};
Q_DECLARE_METATYPE(std::shared_ptr<UserInfo>)

class ChatDataBase {
public:
    ChatDataBase() = default;

    ChatDataBase(int msg_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type,
                 QString content,int _send_uid,int status, QString chat_time);
    ChatDataBase(QString unique_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type,
                 QString content, int send_uid,int status, QString chat_time);
    ChatDataBase(int msg_id, QString unique_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type,
                 QString content, int send_uid, int status, QString chat_time);

    int GetMsgId() { return _msg_id; }
    int GetThreadId() { return _thread_id; }
    ChatFormType GetFormType() { return _form_type; }
    ChatMsgType GetMsgType() { return _msg_type; }
    QString GetContent() { return _content; }
    int GetSendUid() { return _send_uid; }
    QString GetMsgContent(){return _content;}
    void SetUniqueId(int unique_id);
    QString GetUniqueId();
    int GetStatus() { return _status; }
    void SetMsgId(int msg_id) { _msg_id = msg_id; }
    void SetStatus(int status) { _status = status; }

    virtual ~ChatDataBase() {}  // 添加虚析构函数

public:
    //群聊还是私聊
    ChatFormType _form_type;

protected:
    //客户端本地唯一标识
    QString _unique_id;
    //消息id
    int _msg_id;
    //会话id
    int _thread_id;
    //文本信息为0，图片为1，文件为2
    ChatMsgType _msg_type;
    //消息内容
    QString _content;
    //发送者id
    int _send_uid;
    //消息状态
    int _status;
    //发送时间
    QString _chat_time;
};
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<ChatDataBase>>)

class TextChatData : public ChatDataBase {
public:
    TextChatData(int msg_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type,  QString content,
                 int send_uid, int status, QString chat_time=""):
        ChatDataBase(msg_id, thread_id, form_type, msg_type, content, send_uid, status, chat_time){}

    TextChatData(QString unique_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type, QString content,
                 int send_uid, int status, QString chat_time=""):
        ChatDataBase(unique_id, thread_id, form_type, msg_type, content, send_uid, status, chat_time){}

    TextChatData(int msg_id, QString unique_id, int thread_id, ChatFormType form_type, ChatMsgType msg_type, QString content,
                 int send_uid, int status, QString chat_time=""):
        ChatDataBase(msg_id, unique_id, thread_id, form_type, msg_type, content, send_uid, status, chat_time){}

    TextChatData() = default;

    ~TextChatData() override{}
};
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<TextChatData>>)

class ImgChatData : public ChatDataBase {
public:
    ImgChatData(std::shared_ptr<MsgInfo> msg_info, QString unique_id,
                int thread_id, ChatFormType form_type, ChatMsgType msg_type,
                int send_uid, int status, QString chat_time = ""):
        ChatDataBase(unique_id,thread_id, form_type, msg_type, msg_info->_text_or_url,
                     send_uid, status, chat_time), _msg_info(msg_info)
    {
        _msg_id = _msg_info->_msg_id;
    }

    ~ImgChatData() override {}

    std::shared_ptr<MsgInfo> _msg_info;
};
Q_DECLARE_METATYPE(std::shared_ptr<ImgChatData>)

class FileChatData : public ChatDataBase {
public:
    FileChatData(std::shared_ptr<MsgInfo> msg_info, QString unique_id,
                int thread_id, ChatFormType form_type, ChatMsgType msg_type,
                int send_uid, int status, QString chat_time = ""):
        ChatDataBase(unique_id, thread_id, form_type, msg_type, msg_info->_text_or_url,
                     send_uid, status, chat_time), _msg_info(msg_info)
    {
        _msg_id = _msg_info->_msg_id;
    }

    ~FileChatData() override {}

    std::shared_ptr<MsgInfo> _msg_info;
};
Q_DECLARE_METATYPE(std::shared_ptr<FileChatData>)

//聊天列表信息
struct ChatThreadInfo{
    ChatThreadInfo() = default;
    ChatThreadInfo(int thread_id,QString type,int user1_id,int user2_id,QString group_name = ""):
        _thread_id(thread_id),_type(type),_user1_id(user1_id),_user2_id(user2_id),_group_name(group_name){}

    int _thread_id;
    QString _type;
    int _user1_id;
    int _user2_id;
    QString _group_name;  // 群聊名称，私聊时为空
};
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<ChatThreadInfo>>)

// 群成员信息
struct GroupMemberInfo {
    int _uid;
    QString _name;
    QString _icon;
    int _role;  // 0=普通成员, 1=管理员, 2=创建者

    GroupMemberInfo() : _uid(0), _name(""), _icon(""), _role(0) {}
    GroupMemberInfo(int uid, const QString& name, const QString& icon, int role)
        : _uid(uid), _name(name), _icon(icon), _role(role) {}
};
Q_DECLARE_METATYPE(std::shared_ptr<GroupMemberInfo>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<GroupMemberInfo>>)

//客户端本地存储的聊天线程数据结构
class ChatThreadData {
public:
    ChatThreadData(int other_id, int thread_id, int last_msg_id):
        _other_id(other_id), _thread_id(thread_id), _last_msg_id(last_msg_id), _is_group(false){}

    ChatThreadData() = default;

    //添加消息到缓存map中
    void AddMsg(std::shared_ptr<ChatDataBase> msg);
    //将在未回复map中的消息转到缓存消息map中
    void MoveMsg(std::shared_ptr<ChatDataBase> msg);
    //更新文件上传进度条
    void UpdateProgress(std::shared_ptr<MsgInfo> msg);
    void SetLastMsgId(int msg_id);
    int GetLastMsgId();
    void SetOtherId(int other_id);
    int  GetOtherId();
    QString GetGroupName();
    void SetGroupName(const QString& name);
    QString GetNotice() const { return _notice; }
    void SetNotice(const QString& notice) { _notice = notice; }
    bool IsGroup() const { return _is_group; }
    void SetIsGroup(bool is_group) { _is_group = is_group; }
    QString GetGroupIcon() const { return _group_icon; }
    void SetGroupIcon(const QString& icon) { _group_icon = icon; }
    QString GetGroupDesc() const { return _group_desc; }
    void SetGroupDesc(const QString& desc) { _group_desc = desc; }
    int GetOwnerId() const { return _owner_id; }
    void SetOwnerId(int owner_id) { _owner_id = owner_id; }
    int GetMemberCount() const { return _member_count; }
    void SetMemberCount(int count) { _member_count = count; }
    // 获取群成员列表
    std::vector<std::shared_ptr<GroupMemberInfo>> GetGroupMembers() const { return _group_members; }
    void SetGroupMembers(const std::vector<std::shared_ptr<GroupMemberInfo>>& members) { _group_members = members; }
    void AddGroupMember(std::shared_ptr<GroupMemberInfo> member) { _group_members.push_back(member); }
    QMap<int, std::shared_ptr<ChatDataBase>> GetMsgMap();
    int  GetThreadId();
    void SetThreadId(int thread_id);
    QMap<int, std::shared_ptr<ChatDataBase>>&  GetMsgMapRef();
    void AppendMsg(int msg_id, std::shared_ptr<ChatDataBase> base_msg);
    QString GetLastMsg();
    void SetLastMsg(const QString& msg);
    // 未读消息计数
    void IncrementUnreadCount();
    void SetUnreadCount(int count);
    int GetUnreadCount() const { return _unread_count; }
    QMap<QString, std::shared_ptr<ChatDataBase>>& GetMsgUnRspRef();
    void AppendUnRspMsg(QString unique_id, std::shared_ptr<ChatDataBase> base_msg);
    std::shared_ptr<ChatDataBase> GetChatDataBase(int msg_id);

private:
    //如果是私聊，则为对方的id；如果是群聊，则为0
    int _other_id;
    int _last_msg_id;
    int _thread_id;
    QString _last_msg;
    // 未读消息计数
    int _unread_count = 0;
    //是否是群聊
    bool _is_group;
    //群聊信息,成员列表
    std::vector<std::shared_ptr<GroupMemberInfo>> _group_members;
    //群聊名称
    QString _group_name;
    //群聊头像
    QString _group_icon;
    //群聊描述
    QString _group_desc;
    //群主uid
    int _owner_id = 0;
    //群成员数量
    int _member_count = 0;
    //群公告
    QString _notice;
    //缓存消息map，抽象为基类，因为会有图片等其他类型消息
    QMap<int, std::shared_ptr<ChatDataBase>>  _msg_map;
    //缓存未回复的消息（已发送，但还未收到服务器回应的消息）
    QMap<QString, std::shared_ptr<ChatDataBase>>  _msg_unrsp_map;
};

#endif // USERDATA_H
