#ifndef CONTEXTMANAGER_H
#define CONTEXTMANAGER_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// ============================================================
// ContextManager — Agent 对话上下文管理器
//
// 职责：
//   1. 维护 messages 数组，持有所有对话轮次（含 tool/tool_calls）
//   2. 追踪 Token 消耗（用字符数估算，1 Token ≈ 1.5 中文字符 / 4 英文字符）
//   3. 当 Token 超预算时，自动执行「滑动窗口」裁剪：
//      - 保留 system 消息（始终位于第一条）
//      - 保留最近 KEEP_RECENT 条普通消息
//      - 对被裁剪的历史消息，调用 summarize() 生成摘要并插入为 system 辅助消息
//   4. 对外透明：AIPage 只需调用 addMessage() / messages()，
//      无需关心上下文管理细节
//
// Token 估算策略（offline，无需 API）：
//   estimatedTokens(text) = ceil(text.size() / 2.5)
//   工具结果、tool_calls 消息也计入总量
// ============================================================
class ContextManager : public QObject {
    Q_OBJECT
public:
    explicit ContextManager(QObject* parent = nullptr);

    // ---- 配置 ----
    // 最大 Token 预算（超出后触发裁剪）
    void setTokenBudget(int tokens);
    int tokenBudget() const { return _tokenBudget; }

    // 超出预算时保留的最近消息条数（不含 system 消息）
    void setKeepRecentCount(int count);
    int keepRecentCount() const { return _keepRecentCount; }

    // ---- 消息管理 ----
    // 添加一条消息（内部自动检查并触发裁剪）
    void addMessage(const QJsonObject& msg);

    // 获取当前完整 messages 数组（用于传给 API）
    QJsonArray messages() const { return _messages; }

    // 清除所有消息（保留 system 消息）
    void clearHistory();

    // 重置，包括 system 消息也清除
    void reset();

    // 设置 / 更新 system 消息（始终保持在第一条）
    void setSystemMessage(const QString& content);

    // ---- Token 统计 ----
    // 估算当前所有消息的 Token 总量
    int estimatedTotalTokens() const;

    // 估算单条文本的 Token 数
    static int estimateTokens(const QString& text);

    // 是否已超预算（可用于 UI 提示）
    bool isOverBudget() const;

    // 当前压缩次数（diagnostic 用）
    int compactionCount() const { return _compactionCount; }

signals:
    // 触发上下文压缩时发出（携带被丢弃的消息条数）
    void sig_context_compacted(int droppedCount, int savedTokens);

    // 上下文接近预算时发出（剩余 Token < 20%）
    void sig_context_near_limit(int usedTokens, int budget);

private:
    // 执行上下文裁剪
    void compact();

    // 将被裁剪的历史消息转换为摘要文本（简单串联，不调用 API）
    QString summarizeMessages(const QJsonArray& msgs) const;

    // 计算整个 messages 数组的估算 Token 总量
    int calcTotalTokens(const QJsonArray& msgs) const;

    QJsonArray _messages;       // 完整对话历史

    int _tokenBudget;           // Token 上限（默认 6000）
    int _keepRecentCount;       // 保留最近几条（默认 20）
    int _compactionCount;       // 已执行压缩次数（调试用）

    static constexpr int DEFAULT_TOKEN_BUDGET     = 6000;
    static constexpr int DEFAULT_KEEP_RECENT      = 20;
    // 触发压缩的阈值（占预算的百分比，0-100）
    static constexpr int COMPACT_THRESHOLD_PCT    = 85;
};

#endif // CONTEXTMANAGER_H
