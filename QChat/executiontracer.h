#ifndef EXECUTIONTRACER_H
#define EXECUTIONTRACER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QDateTime>
#include <QList>

// ============================================================
// TraceStep — 单步执行记录
// ============================================================
struct TraceStep {
    enum class Status {
        Pending,    // 等待执行
        Running,    // 执行中
        Success,    // 成功
        Failed      // 失败
    };

    int         stepIndex;      // 步骤序号（从 1 开始）
    QString     toolName;       // 工具名称
    QJsonObject args;           // 输入参数
    QString     result;         // 执行结果（JSON 字符串）
    QString     errorMsg;       // 错误信息（失败时填充）
    Status      status;
    QDateTime   startTime;
    QDateTime   endTime;
    int         elapsedMs;      // 执行耗时（毫秒）

    TraceStep() : stepIndex(0), status(Status::Pending), elapsedMs(0) {}

    // 是否成功
    bool isSuccess() const { return status == Status::Success; }

    // 状态图标（供 UI 使用）
    QString statusIcon() const {
        switch (status) {
            case Status::Pending: return "○";
            case Status::Running: return "⏳";
            case Status::Success: return "✓";
            case Status::Failed:  return "✗";
        }
        return "?";
    }

    // 工具名 → 中文显示名映射
    static QString displayName(const QString& toolName) {
        static const QMap<QString, QString> nameMap = {
            {"send_message",    "发送消息"},
            {"check_online",    "查询在线状态"},
            {"get_friend_info", "获取好友信息"},
            {"list_friends",    "列出所有好友"},
            {"get_chat_history","获取聊天记录"}
        };
        return nameMap.value(toolName, toolName);
    }
};

// ============================================================
// ExecutionTracer — Agent 执行链追踪器
//
// 职责：
//   1. 记录每一轮工具调用的完整执行链（TraceStep 列表）
//   2. 追踪每步耗时
//   3. 维护每个"用户请求"的执行会话（一次对话轮次包含多个 TraceStep）
//   4. 发出实时进度信号供 UI 面板渲染
//   5. 提供格式化的执行摘要文本（用于 debug 面板）
//
// 集成方式：
//   - 在 AIPage 中持有 ExecutionTracer* _tracer
//   - executeToolCalls() 中调用 beginStep() / endStep()
//   - 连接信号 sig_step_started / sig_step_finished 更新 UI
// ============================================================
class ExecutionTracer : public QObject {
    Q_OBJECT
public:
    explicit ExecutionTracer(QObject* parent = nullptr);

    // ---- 会话管理 ----
    // 开始新的执行会话（每次用户发送消息时调用）
    void beginSession(const QString& userQuery);

    // 结束当前会话
    void endSession();

    // 是否有活跃会话
    bool hasActiveSession() const { return _sessionActive; }

    // ---- 步骤管理 ----
    // 开始一个工具执行步骤，返回 stepIndex
    int beginStep(const QString& toolName, const QJsonObject& args);

    // 标记步骤成功完成
    void endStepSuccess(int stepIndex, const QString& result);

    // 标记步骤失败
    void endStepFailed(int stepIndex, const QString& errorMsg);

    // ---- 数据查询 ----
    // 获取当前会话所有步骤
    QList<TraceStep> currentSteps() const { return _currentSteps; }

    // 获取指定步骤
    const TraceStep* getStep(int stepIndex) const;

    // 当前会话步骤总数
    int stepCount() const { return _currentSteps.size(); }

    // 当前会话用户查询
    QString userQuery() const { return _userQuery; }

    // 生成执行摘要文本（供 UI 面板 / debug 显示）
    QString formatSummary() const;

    // 生成单步骤的 HTML 描述（供 UI 渲染）
    QString formatStepHtml(const TraceStep& step) const;

    // 生成完整执行链 HTML（供执行面板整体渲染）
    QString formatFullHtml() const;

signals:
    // 新会话开始
    void sig_session_started(const QString& userQuery);

    // 会话结束（携带总步骤数和总耗时）
    void sig_session_ended(int totalSteps, int totalElapsedMs);

    // 步骤开始（stepIndex, toolName）
    void sig_step_started(int stepIndex, const QString& toolName, const QJsonObject& args);

    // 步骤完成（stepIndex, success, result/errorMsg）
    void sig_step_finished(int stepIndex, bool success, const QString& resultOrError);

private:
    QList<TraceStep> _currentSteps;
    QString          _userQuery;
    bool             _sessionActive;
    QDateTime        _sessionStart;
    int              _nextStepIndex;
};

#endif // EXECUTIONTRACER_H
