#ifndef ITOOLAGENT_H
#define ITOOLAGENT_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

// ============================================================
// IAgentTool — 所有 Agent 工具的统一接口
//
// 每个工具只需实现这三个纯虚函数，即可被 ToolRegistry 自动接管：
//   name()        → 工具标识符（LLM function calling 用到的 name 字段）
//   description() → 工具描述，会注入到 LLM system prompt
//   schema()      → 参数 JSON Schema（OpenAI functions parameters 格式）
//   execute()     → 工具执行逻辑，返回 JSON 字符串
// ============================================================
class IAgentTool {
public:
    virtual ~IAgentTool() = default;

    // 工具唯一标识名（与 LLM function_call.name 对应）
    virtual QString name() const = 0;

    // 人类可读的工具描述（注入 LLM prompt）
    virtual QString description() const = 0;

    // 参数 JSON Schema（OpenAI functions parameters 格式）
    virtual QJsonObject schema() const = 0;

    // 执行工具，返回 JSON 字符串结果
    virtual QString execute(const QJsonObject& args) = 0;

    // 风险等级（用于 P1 SafetyGuard 阶段）
    enum class RiskLevel {
        Safe,    // 只读操作（查询、列表）
        Medium,  // 写操作但可逆（发消息）
        High     // 不可逆的破坏性操作（预留）
    };
    virtual RiskLevel riskLevel() const {
        return RiskLevel::Safe;
    }

    // 构建供 Doubao API 使用的单个 tool 定义对象
    QJsonObject toApiToolDef() const {
        QJsonObject func;
        func["name"]        = name();
        func["description"] = description();
        func["parameters"]  = schema();

        QJsonObject tool;
        tool["type"]     = "function";
        tool["function"] = func;
        return tool;
    }
};

#endif // ITOOLAGENT_H
