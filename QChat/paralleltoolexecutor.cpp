#include "paralleltoolexecutor.h"
#include "toolregistry.h"
#include <QtConcurrent>
#include <QDebug>
#include <QJsonDocument>

// ============================================================
// ToolCallResult 序列化
// ============================================================
QJsonObject ToolCallResult::toMessage() const
{
    QJsonObject msg;
    msg["role"] = "tool";
    msg["tool_call_id"] = toolCallId;
    msg["content"] = result;
    return msg;
}

// ============================================================
// ParallelToolExecutor 实现
// ============================================================
ParallelToolExecutor::ParallelToolExecutor(QObject* parent)
    : QObject(parent)
{
}

ParallelToolExecutor::~ParallelToolExecutor()
{
    cancelAll();
    for (auto* watcher : _watchers.values()) {
        delete watcher;
    }
    _watchers.clear();
}

// ------------------------------------------------------------
// 主执行入口
// ------------------------------------------------------------
void ParallelToolExecutor::executeAll(const QJsonArray& toolCalls)
{
    if (_running) {
        emit sig_error("已有任务正在执行，请先等待或取消");
        return;
    }

    if (toolCalls.isEmpty()) {
        emit sig_all_finished({}, 0);
        return;
    }

    // 重置状态
    _running = true;
    _cancelled = false;
    _completedCount = 0;
    _pendingTasks.clear();
    _lastResults.clear();
    _startTime = QDateTime::currentDateTime();

    // 解析所有工具调用
    for (int i = 0; i < toolCalls.size(); ++i) {
        QJsonObject tc = toolCalls[i].toObject();
        ToolCallTask task;
        task.toolCallId = tc["id"].toString();
        task.index = i;

        QJsonObject funcObj = tc["function"].toObject();
        task.toolName = funcObj["name"].toString();
        task.args = QJsonDocument::fromJson(
            funcObj["arguments"].toString().toUtf8()
        ).object();

        _pendingTasks.append(task);
    }

    // 预分配结果数组（保持顺序）
    _lastResults.resize(_pendingTasks.size());

    qDebug() << "[ParallelToolExecutor] Starting" << _pendingTasks.size()
             << "tasks with max concurrency" << _maxConcurrency;

    // 启动第一批任务
    processNextBatch();
}

// ------------------------------------------------------------
// 处理下一批任务（控制并发度）
// ------------------------------------------------------------
void ParallelToolExecutor::processNextBatch()
{
    if (_cancelled) return;

    int runningCount = 0;
    for (auto* w : _watchers.values()) {
        if (w->isRunning()) runningCount++;
    }

    int slotsAvailable = _maxConcurrency - runningCount;
    int started = 0;

    for (const auto& task : _pendingTasks) {
        // 跳过已在执行的
        if (_watchers.contains(task.toolCallId)) continue;

        // 启动新任务
        auto* watcher = new QFutureWatcher<ToolCallResult>(this);
        _watchers[task.toolCallId] = watcher;

        connect(watcher, &QFutureWatcher<ToolCallResult>::finished,
                this, [this, task]() {
                    onSingleFinished(task.toolCallId);
                });

        // 使用 QtConcurrent::run 在后台线程执行
        QFuture<ToolCallResult> future = QtConcurrent::run(
            [this, task]() { return executeSingle(task); }
        );
        watcher->setFuture(future);

        emit sig_tool_started(task.toolCallId, task.toolName, task.index);
        qDebug() << "[ParallelToolExecutor] Started task" << task.index
                 << ":" << task.toolName;

        started++;
        if (started >= slotsAvailable) break;
    }
}

// ------------------------------------------------------------
// 执行单个工具（在后台线程）
// ------------------------------------------------------------
ToolCallResult ParallelToolExecutor::executeSingle(const ToolCallTask& task)
{
    ToolCallResult result;
    result.toolCallId = task.toolCallId;
    result.toolName = task.toolName;

    QDateTime start = QDateTime::currentDateTime();

    // 检查取消标志
    if (_cancelled) {
        result.success = false;
        result.error = "任务被取消";
        result.elapsedMs = 0;
        return result;
    }

    // 调用 ToolRegistry 执行
    QString execResult = ToolRegistry::instance().execute(task.toolName, task.args);

    result.elapsedMs = static_cast<int>(start.msecsTo(QDateTime::currentDateTime()));

    // 解析结果判断是否成功
    QJsonObject resultObj = QJsonDocument::fromJson(execResult.toUtf8()).object();
    if (resultObj.contains("error")) {
        result.success = false;
        result.error = resultObj["error"].toString();
    } else {
        result.success = true;
    }
    result.result = execResult;

    return result;
}

// ------------------------------------------------------------
// 单个任务完成回调
// ------------------------------------------------------------
void ParallelToolExecutor::onSingleFinished(const QString& toolCallId)
{
    if (!_watchers.contains(toolCallId)) return;

    auto* watcher = _watchers[toolCallId];
    ToolCallResult result = watcher->future().result();

    // 保存结果到对应位置
    for (const auto& task : _pendingTasks) {
        if (task.toolCallId == toolCallId) {
            _lastResults[task.index] = result;
            break;
        }
    }

    _completedCount++;

    emit sig_tool_finished(result.toolCallId,
                           result.toolName,
                           result.success,
                           result.elapsedMs,
                           _completedCount - 1);

    qDebug() << "[ParallelToolExecutor] Finished task" << toolCallId
             << (result.success ? "✓" : "✗")
             << "in" << result.elapsedMs << "ms";

    // 清理 watcher
    _watchers.remove(toolCallId);
    watcher->deleteLater();

    // 检查是否全部完成
    if (_completedCount >= _pendingTasks.size()) {
        _totalElapsedMs = static_cast<int>(_startTime.msecsTo(QDateTime::currentDateTime()));
        _running = false;

        qDebug() << "[ParallelToolExecutor] All finished in" << _totalElapsedMs << "ms";
        emit sig_all_finished(_lastResults, _totalElapsedMs);
    } else if (!_cancelled) {
        // 继续启动下一批
        processNextBatch();
    }
}

// ------------------------------------------------------------
// 取消所有任务
// ------------------------------------------------------------
void ParallelToolExecutor::cancelAll()
{
    if (!_running) return;

    _cancelled = true;

    // 取消所有正在运行的 watcher
    for (auto* watcher : _watchers.values()) {
        if (watcher->isRunning()) {
            // QFutureWatcher 不支持强制取消，只能等待自然结束
            // 但 executeSingle 会检查 _cancelled 标志
        }
    }

    _running = false;
    emit sig_cancelled();
}

// ------------------------------------------------------------
// 统计
// ------------------------------------------------------------
int ParallelToolExecutor::successCount() const
{
    int count = 0;
    for (const auto& r : _lastResults) {
        if (r.success) count++;
    }
    return count;
}

int ParallelToolExecutor::failCount() const
{
    int count = 0;
    for (const auto& r : _lastResults) {
        if (!r.success) count++;
    }
    return count;
}
