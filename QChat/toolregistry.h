#ifndef TOOLREGISTRY_H
#define TOOLREGISTRY_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <memory>
#include "itoolagent.h"

// ============================================================
// ToolRegistry — Agent 工具注册中心（单例）
//
// 职责：
//   1. 统一注册/注销 IAgentTool 实例
//   2. 按名称分发执行请求
//   3. 为 LLM API 动态生成 tools 定义数组
//   4. 提供工具元数据查询（名称、描述、风险等级）
//
// 使用方式：
//   // 注册工具（在 AIPage 构造函数中调用）
//   ToolRegistry::instance().registerTool(std::make_shared<SendMessageTool>());
//
//   // 执行工具
//   QString result = ToolRegistry::instance().execute("send_message", args);
//
//   // 获取 API 工具定义
//   QJsonArray toolDefs = ToolRegistry::instance().buildApiToolsDef();
// ============================================================

class ToolRegistry : public QObject {
    Q_OBJECT
public:
    // 单例访问
    static ToolRegistry& instance();

    // 注册一个工具（重复注册同名工具会覆盖旧的）
    void registerTool(std::shared_ptr<IAgentTool> tool);

    // 注销工具
    void unregisterTool(const QString& toolName);

    // 检查工具是否已注册
    bool hasTool(const QString& toolName) const;

    // 获取已注册工具数量
    int toolCount() const;

    // 获取所有已注册工具名称
    QStringList toolNames() const;

    // 执行工具 —— 找不到工具时返回包含 error 字段的 JSON 字符串
    QString execute(const QString& toolName, const QJsonObject& args);

    // 获取工具的风险等级（供 SafetyGuard 使用）
    IAgentTool::RiskLevel riskLevel(const QString& toolName) const;

    // 构建供 Doubao/OpenAI API 使用的 tools 定义数组
    QJsonArray buildApiToolsDef() const;

    // 获取所有工具的描述文本（注入 system prompt 使用）
    QString buildToolsDescription() const;

signals:
    // 工具执行前（toolName, args）
    void sig_tool_before_execute(const QString& toolName, const QJsonObject& args);

    // 工具执行完成（toolName, result）
    void sig_tool_after_execute(const QString& toolName, const QString& result);

    // 工具执行异常（toolName, errorMessage）
    void sig_tool_error(const QString& toolName, const QString& errorMsg);

private:
    // 私有构造（单例）
    explicit ToolRegistry(QObject* parent = nullptr);
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    // 工具表：name → IAgentTool 实例
    QMap<QString, std::shared_ptr<IAgentTool>> _tools;
};

#endif // TOOLREGISTRY_H
