#ifndef PARALLELTOOLEXECUTOR_H
#define PARALLELTOOLEXECUTOR_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QFuture>
#include <QFutureWatcher>
#include <QMap>
#include <functional>
#include <memory>

// ============================================================
// ToolCallTask — 单个工具调用任务
// ============================================================
struct ToolCallTask {
    QString toolCallId;     // API 返回的 tool_call_id
    QString toolName;       // 工具名称
    QJsonObject args;       // 参数
    int index;              // 在原始数组中的索引（用于保持顺序）

    ToolCallTask() : index(0) {}
};

// ============================================================
// ToolCallResult — 单个工具调用结果
// ============================================================
struct ToolCallResult {
    QString toolCallId;
    QString toolName;
    QString result;         // 结果 JSON 字符串
    bool success;
    int elapsedMs;          // 执行耗时
    QString error;          // 失败时的错误信息

    ToolCallResult() : success(false), elapsedMs(0) {}

    QJsonObject toMessage() const;  // 转换为 API 所需的 tool 消息格式
};

// ============================================================
// ParallelToolExecutor — 并行工具执行器
//
// 职责：
//   1. 接收多个工具调用请求，并行执行（使用 QtConcurrent）
//   2. 管理并发度（默认最大4个并行）
//   3. 保持结果顺序与请求顺序一致
//   4. 提供统一的完成/错误信号
//   5. 支持取消正在执行的任务
//
// 与原有串行执行的区别：
//   - 串行：toolA → wait → toolB → wait → toolC
//   - 并行：toolA + toolB + toolC → waitAll → 统一返回
// ============================================================
class ParallelToolExecutor : public QObject {
    Q_OBJECT
public:
    explicit ParallelToolExecutor(QObject* parent = nullptr);
    ~ParallelToolExecutor();

    // ---- 配置 ----
    void setMaxConcurrency(int max) { _maxConcurrency = qMax(1, max); }
    int maxConcurrency() const { return _maxConcurrency; }

    // ---- 执行接口 ----
    // 提交一批工具调用，立即开始并行执行
    void executeAll(const QJsonArray& toolCalls);

    // 是否正在执行
    bool isRunning() const { return _running; }

    // 取消所有正在执行的任务（尽可能中断）
    void cancelAll();

    // ---- 结果获取 ----
    // 获取最近一次执行的结果（按原始顺序）
    QVector<ToolCallResult> lastResults() const { return _lastResults; }

    // 获取执行统计
    int totalElapsedMs() const { return _totalElapsedMs; }
    int successCount() const;
    int failCount() const;

signals:
    // 单个工具开始/完成（用于实时更新 UI）
    void sig_tool_started(const QString& toolCallId,
                          const QString& toolName,
                          int index);

    void sig_tool_finished(const QString& toolCallId,
                           const QString& toolName,
                           bool success,
                           int elapsedMs,
                           int index);

    // 全部完成
    void sig_all_finished(const QVector<ToolCallResult>& results,
                          int totalElapsedMs);

    // 执行取消
    void sig_cancelled();

    // 执行错误（整体错误，非单个工具错误）
    void sig_error(const QString& error);

private:
    int _maxConcurrency = 4;
    bool _running = false;
    bool _cancelled = false;

    QVector<ToolCallTask> _pendingTasks;
    QVector<ToolCallResult> _lastResults;
    int _totalElapsedMs = 0;

    // 执行单个工具（在后台线程中运行）
    ToolCallResult executeSingle(const ToolCallTask& task);

    // 使用 FutureWatcher 管理并发
    QMap<QString, QFutureWatcher<ToolCallResult>*> _watchers;

    void processNextBatch();
    void onSingleFinished(const QString& toolCallId);

    int _completedCount = 0;
    QDateTime _startTime;
};

#endif // PARALLELTOOLEXECUTOR_H
