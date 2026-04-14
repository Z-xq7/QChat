#ifndef SAFETYGUARD_H
#define SAFETYGUARD_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QStringList>
#include "itoolagent.h"

// ============================================================
// SafetyResult — 安全检查结果
// ============================================================
struct SafetyResult {
    enum class Level {
        Pass,       // 通过
        Warn,       // 警告（仍可执行，但记录日志）
        Block       // 拦截（拒绝执行）
    };

    Level   level;
    QString reason;     // 拦截/警告原因
    QString category;   // 规则分类（prompt_injection / risk_tool / content_violation）

    SafetyResult() : level(Level::Pass) {}
    SafetyResult(Level l, const QString& r, const QString& cat = QString())
        : level(l), reason(r), category(cat) {}

    bool passed()  const { return level == Level::Pass; }
    bool blocked() const { return level == Level::Block; }
    bool warned()  const { return level == Level::Warn; }
};

// ============================================================
// SafetyGuard — Agent 安全护栏
//
// 职责：
//   1. Prompt 注入检测：识别用户输入中的越权指令模式
//      （忽略上述指令、扮演角色、输出系统 prompt 等）
//   2. 工具风险评估：High 风险工具必须用户明确确认后方可执行
//   3. 内容过滤：拦截敏感内容（恶意/违规文本）
//   4. 频率限制：防止短时间内大量工具调用（基础防护）
//
// 集成方式：
//   - 在 AIPage.onSendMessage() 中调用 checkUserInput()
//   - 在 AIPage.executeToolCalls() 中调用 checkToolCall()
//   - 若返回 Block，立即终止并向用户显示警告
//   - 若返回 Warn，执行但在 UI 显示警告提示
// ============================================================
class SafetyGuard : public QObject {
    Q_OBJECT
public:
    explicit SafetyGuard(QObject* parent = nullptr);

    // ---- 主要检查接口 ----

    // 检查用户输入（发送前调用）
    SafetyResult checkUserInput(const QString& input);

    // 检查工具调用（executeToolCalls 中调用）
    SafetyResult checkToolCall(const QString& toolName,
                               const QJsonObject& args,
                               IAgentTool::RiskLevel riskLevel);

    // ---- 配置 ----

    // 设置每分钟最大工具调用次数（默认 20）
    void setMaxCallsPerMinute(int max) { _maxCallsPerMinute = max; }

    // 是否启用 Prompt 注入检测（默认开启）
    void setPromptInjectionDetection(bool enabled) { _promptInjectionEnabled = enabled; }

    // 是否启用频率限制（默认开启）
    void setRateLimitEnabled(bool enabled) { _rateLimitEnabled = enabled; }

    // ---- 统计 ----
    int totalBlocked() const { return _totalBlocked; }
    int totalWarned()  const { return _totalWarned; }

    // 重置频率计数器（新会话开始时调用）
    void resetRateCounter();

signals:
    // 检测到安全风险时发出
    void sig_safety_event(const QString& category,
                          const QString& reason,
                          SafetyResult::Level level);

private:
    // Prompt 注入检测
    SafetyResult detectPromptInjection(const QString& input);

    // 内容过滤
    SafetyResult checkContent(const QString& input);

    // 频率限制检查
    SafetyResult checkRateLimit();

    // 注入模式列表（正则表达式）
    QStringList _injectionPatterns;

    // 敏感内容关键词
    QStringList _sensitiveKeywords;

    // 频率限制
    int     _maxCallsPerMinute;
    int     _callCountInWindow;
    qint64  _windowStartMs;
    bool    _rateLimitEnabled;
    bool    _promptInjectionEnabled;

    // 统计
    int _totalBlocked;
    int _totalWarned;
};

#endif // SAFETYGUARD_H
