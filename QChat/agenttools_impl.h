#ifndef TOOLS_SEND_MESSAGE_H
#define TOOLS_SEND_MESSAGE_H

#include "itoolagent.h"
#include "userdata.h"
#include <memory>

// ============================================================
// SendMessageTool — 给好友发送文本消息
// ============================================================
class SendMessageTool : public IAgentTool {
public:
    QString name() const override { return "send_message"; }

    QString description() const override {
        return "给指定好友发送文本消息，支持按昵称或用户名模糊匹配好友";
    }

    QJsonObject schema() const override;

    QString execute(const QJsonObject& args) override;

    RiskLevel riskLevel() const override { return RiskLevel::Medium; }

private:
    std::shared_ptr<UserInfo> findFriendByName(const QString& name);
};

// ============================================================
// CheckOnlineTool — 查询好友在线状态
// ============================================================
class CheckOnlineTool : public IAgentTool {
public:
    QString name() const override { return "check_online"; }

    QString description() const override {
        return "查询指定好友当前是否在线，返回在线/离线状态";
    }

    QJsonObject schema() const override;

    QString execute(const QJsonObject& args) override;

private:
    std::shared_ptr<UserInfo> findFriendByName(const QString& name);
};

// ============================================================
// GetFriendInfoTool — 获取好友详细信息
// ============================================================
class GetFriendInfoTool : public IAgentTool {
public:
    QString name() const override { return "get_friend_info"; }

    QString description() const override {
        return "获取指定好友的详细信息，包括UID、昵称、用户名、个性签名、在线状态";
    }

    QJsonObject schema() const override;

    QString execute(const QJsonObject& args) override;

private:
    std::shared_ptr<UserInfo> findFriendByName(const QString& name);
};

// ============================================================
// ListFriendsTool — 列出所有好友
// ============================================================
class ListFriendsTool : public IAgentTool {
public:
    QString name() const override { return "list_friends"; }

    QString description() const override {
        return "列出当前用户的所有好友，包含昵称和在线状态";
    }

    QJsonObject schema() const override;

    QString execute(const QJsonObject& args) override;
};

// ============================================================
// GetChatHistoryTool — 获取聊天记录
// ============================================================
class GetChatHistoryTool : public IAgentTool {
public:
    QString name() const override { return "get_chat_history"; }

    QString description() const override {
        return "获取与指定好友最近的聊天记录，可指定获取条数（默认10条）";
    }

    QJsonObject schema() const override;

    QString execute(const QJsonObject& args) override;

private:
    std::shared_ptr<UserInfo> findFriendByName(const QString& name);
};

#endif // TOOLS_SEND_MESSAGE_H
