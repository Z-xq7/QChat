#include "agenttools_impl.h"
#include "usermgr.h"
#include "tcpmgr.h"
#include "global.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDebug>

// ============================================================
// 公共辅助函数：按名称模糊查找好友（各工具类复用）
// 优先级：精确昵称 > 精确用户名 > 昵称包含 > 用户名包含
// ============================================================
static std::shared_ptr<UserInfo> findFriendByNameHelper(const QString& name)
{
    if (name.isEmpty()) return nullptr;
    auto friends = UserMgr::GetInstance()->GetAllFriends();

    for (auto& f : friends) { if (f->_nick == name) return f; }
    for (auto& f : friends) { if (f->_name == name) return f; }
    for (auto& f : friends) { if (f->_nick.contains(name, Qt::CaseInsensitive)) return f; }
    for (auto& f : friends) { if (f->_name.contains(name, Qt::CaseInsensitive)) return f; }
    return nullptr;
}

std::shared_ptr<UserInfo> SendMessageTool::findFriendByName(const QString& name)  { return findFriendByNameHelper(name); }
std::shared_ptr<UserInfo> CheckOnlineTool::findFriendByName(const QString& name)  { return findFriendByNameHelper(name); }
std::shared_ptr<UserInfo> GetFriendInfoTool::findFriendByName(const QString& name){ return findFriendByNameHelper(name); }
std::shared_ptr<UserInfo> GetChatHistoryTool::findFriendByName(const QString& name){ return findFriendByNameHelper(name); }


// ============================================================
// SendMessageTool
// ============================================================
QJsonObject SendMessageTool::schema() const
{
    QJsonObject pFriend; pFriend["type"] = "string"; pFriend["description"] = "好友的昵称或用户名";
    QJsonObject pContent; pContent["type"] = "string"; pContent["description"] = "要发送的消息内容";

    QJsonObject props;
    props["friend_name"] = pFriend;
    props["content"]     = pContent;

    QJsonArray required; required << "friend_name" << "content";

    QJsonObject s; s["type"] = "object"; s["properties"] = props; s["required"] = required;
    return s;
}

QString SendMessageTool::execute(const QJsonObject& args)
{
    QJsonObject result;
    QString friendName = args["friend_name"].toString().trimmed();
    QString content    = args["content"].toString().trimmed();

    if (friendName.isEmpty() || content.isEmpty()) {
        result["success"] = false;
        result["message"] = "参数错误：好友名称或消息内容不能为空";
        return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }

    auto friendInfo = findFriendByName(friendName);
    if (!friendInfo) {
        result["success"] = false;
        result["message"] = QString("找不到好友：%1，请确认名字是否正确").arg(friendName);
        return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }

    int toUid    = friendInfo->_uid;
    int threadId = UserMgr::GetInstance()->GetThreadIdByUid(toUid);

    if (threadId <= 0) {
        result["success"] = false;
        result["message"] = QString("与 %1 的聊天会话不存在，请先打开与该好友的聊天窗口").arg(friendInfo->_nick);
        return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }

    // 构造消息体（与 chatpage.cpp on_send_btn_clicked 格式一致）
    QJsonObject textItem;
    textItem["content"]   = content;
    textItem["unique_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QJsonArray textArray; textArray.append(textItem);

    QJsonObject msgJson;
    msgJson["fromuid"]    = UserMgr::GetInstance()->GetUid();
    msgJson["touid"]      = toUid;
    msgJson["thread_id"]  = threadId;
    msgJson["is_group"]   = false;
    msgJson["text_array"] = textArray;

    TcpMgr::GetInstance()->SendData(
        ReqId::ID_TEXT_CHAT_MSG_REQ,
        QJsonDocument(msgJson).toJson(QJsonDocument::Compact)
    );

    result["success"]  = true;
    result["message"]  = QString("消息已发送给 %1").arg(friendInfo->_nick);
    result["to_uid"]   = toUid;
    result["to_name"]  = friendInfo->_nick;
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

// ============================================================
// CheckOnlineTool
// ============================================================
QJsonObject CheckOnlineTool::schema() const
{
    QJsonObject pFriend; pFriend["type"] = "string"; pFriend["description"] = "好友的昵称或用户名";
    QJsonObject props; props["friend_name"] = pFriend;
    QJsonArray required; required << "friend_name";

    QJsonObject s; s["type"] = "object"; s["properties"] = props; s["required"] = required;
    return s;
}

QString CheckOnlineTool::execute(const QJsonObject& args)
{
    QJsonObject result;
    auto friendInfo = findFriendByName(args["friend_name"].toString().trimmed());

    if (!friendInfo) {
        result["found"]   = false;
        result["message"] = QString("找不到好友：%1").arg(args["friend_name"].toString());
        return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }

    bool online = UserMgr::GetInstance()->IsFriendOnline(friendInfo->_uid);
    result["found"]   = true;
    result["uid"]     = friendInfo->_uid;
    result["name"]    = friendInfo->_nick;
    result["online"]  = online;
    result["status"]  = online ? "在线" : "离线";
    result["message"] = QString("%1 当前%2").arg(friendInfo->_nick).arg(online ? "在线" : "不在线");
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

// ============================================================
// GetFriendInfoTool
// ============================================================
QJsonObject GetFriendInfoTool::schema() const
{
    QJsonObject pFriend; pFriend["type"] = "string"; pFriend["description"] = "好友的昵称或用户名";
    QJsonObject props; props["friend_name"] = pFriend;
    QJsonArray required; required << "friend_name";

    QJsonObject s; s["type"] = "object"; s["properties"] = props; s["required"] = required;
    return s;
}

QString GetFriendInfoTool::execute(const QJsonObject& args)
{
    QJsonObject result;
    auto friendInfo = findFriendByName(args["friend_name"].toString().trimmed());

    if (!friendInfo) {
        result["found"]   = false;
        result["message"] = QString("找不到好友：%1").arg(args["friend_name"].toString());
        return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }

    bool online = UserMgr::GetInstance()->IsFriendOnline(friendInfo->_uid);
    result["found"]       = true;
    result["uid"]         = friendInfo->_uid;
    result["username"]    = friendInfo->_name;
    result["nickname"]    = friendInfo->_nick;
    result["description"] = friendInfo->_desc;
    result["online"]      = online;
    result["status"]      = online ? "在线" : "离线";
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

// ============================================================
// ListFriendsTool
// ============================================================
QJsonObject ListFriendsTool::schema() const
{
    // 无参数
    QJsonObject props;
    QJsonObject s; s["type"] = "object"; s["properties"] = props;
    return s;
}

QString ListFriendsTool::execute(const QJsonObject& /*args*/)
{
    auto friends = UserMgr::GetInstance()->GetAllFriends();

    QJsonArray friendsArray;
    for (auto& f : friends) {
        bool online = UserMgr::GetInstance()->IsFriendOnline(f->_uid);
        QJsonObject item;
        item["uid"]      = f->_uid;
        item["username"] = f->_name;
        item["nickname"] = f->_nick;
        item["online"]   = online;
        item["status"]   = online ? "在线" : "离线";
        friendsArray.append(item);
    }

    QJsonObject result;
    result["total"]   = static_cast<int>(friends.size());
    result["friends"] = friendsArray;
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

// ============================================================
// GetChatHistoryTool
// ============================================================
QJsonObject GetChatHistoryTool::schema() const
{
    QJsonObject pFriend; pFriend["type"] = "string"; pFriend["description"] = "好友的昵称或用户名";
    QJsonObject pCount;  pCount["type"]  = "integer"; pCount["description"]  = "要获取的最近消息条数，默认为10";
    QJsonObject props; props["friend_name"] = pFriend; props["count"] = pCount;
    QJsonArray required; required << "friend_name";

    QJsonObject s; s["type"] = "object"; s["properties"] = props; s["required"] = required;
    return s;
}

QString GetChatHistoryTool::execute(const QJsonObject& args)
{
    QJsonObject result;
    auto friendInfo = findFriendByName(args["friend_name"].toString().trimmed());

    if (!friendInfo) {
        result["found"]   = false;
        result["message"] = QString("找不到好友：%1").arg(args["friend_name"].toString());
        return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }

    int count = args.contains("count") ? args["count"].toInt(10) : 10;
    count = qBound(1, count, 50); // 最多返回50条

    auto threadData = UserMgr::GetInstance()->GetChatThreadByUid(friendInfo->_uid);
    if (!threadData) {
        result["found"]   = true;
        result["message"] = "暂无聊天记录";
        result["history"] = QJsonArray();
        return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }

    auto msgMap = threadData->GetMsgMap();
    int myUid   = UserMgr::GetInstance()->GetUid();

    QList<std::shared_ptr<ChatDataBase>> msgList;
    for (auto it = msgMap.begin(); it != msgMap.end(); ++it) {
        msgList.append(it.value());
    }

    int startIdx = qMax(0, msgList.size() - count);
    QJsonArray history;
    for (int i = startIdx; i < msgList.size(); ++i) {
        auto msg = msgList[i];
        QJsonObject item;
        item["sender"]  = (msg->GetSendUid() == myUid) ? "我" : friendInfo->_nick;
        item["content"] = msg->GetContent();
        history.append(item);
    }

    result["found"]       = true;
    result["friend_name"] = friendInfo->_nick;
    result["history"]     = history;
    result["count"]       = history.size();
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}
