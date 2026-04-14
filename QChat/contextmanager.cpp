#include "contextmanager.h"
#include <QJsonDocument>
#include <QDebug>
#include <cmath>

// ============================================================
// 构造
// ============================================================
ContextManager::ContextManager(QObject* parent) : QObject(parent),
    _tokenBudget(DEFAULT_TOKEN_BUDGET),
    _keepRecentCount(DEFAULT_KEEP_RECENT),
    _compactionCount(0)
{
}

// ============================================================
// 配置
// ============================================================
void ContextManager::setTokenBudget(int tokens)
{
    _tokenBudget = qMax(1000, tokens);
}

void ContextManager::setKeepRecentCount(int count)
{
    _keepRecentCount = qMax(4, count);
}

// ============================================================
// Token 估算（offline，不调用 API）
// 策略：每 2.5 个字符 ≈ 1 token（中英混合场景的折中估算）
// ============================================================
int ContextManager::estimateTokens(const QString& text)
{
    if (text.isEmpty()) return 0;
    return static_cast<int>(std::ceil(text.size() / 2.5));
}

int ContextManager::calcTotalTokens(const QJsonArray& msgs) const
{
    int total = 0;
    for (const QJsonValue& v : msgs) {
        QJsonObject obj = v.toObject();

        // role 字段本身也计入
        total += estimateTokens(obj["role"].toString());

        // content 字段
        total += estimateTokens(obj["content"].toString());

        // tool_calls 字段（序列化后估算）
        if (obj.contains("tool_calls")) {
            QString tc = QJsonDocument(obj["tool_calls"].toArray()).toJson(QJsonDocument::Compact);
            total += estimateTokens(tc);
        }
    }
    // 每条消息的结构开销（约 4 tokens per message）
    total += msgs.size() * 4;
    return total;
}

int ContextManager::estimatedTotalTokens() const
{
    return calcTotalTokens(_messages);
}

bool ContextManager::isOverBudget() const
{
    return estimatedTotalTokens() > _tokenBudget;
}

// ============================================================
// 消息管理
// ============================================================
void ContextManager::setSystemMessage(const QString& content)
{
    QJsonObject sysMsg;
    sysMsg["role"]    = "system";
    sysMsg["content"] = content;

    if (!_messages.isEmpty() && _messages[0].toObject()["role"].toString() == "system") {
        // 更新已有 system 消息
        _messages[0] = sysMsg;
    } else {
        // 插入到最前
        QJsonArray newMsgs;
        newMsgs.append(sysMsg);
        for (const QJsonValue& v : _messages) {
            newMsgs.append(v);
        }
        _messages = newMsgs;
    }
}

void ContextManager::addMessage(const QJsonObject& msg)
{
    _messages.append(msg);

    // 检查是否需要压缩
    int used   = estimatedTotalTokens();
    int thresh = static_cast<int>(_tokenBudget * COMPACT_THRESHOLD_PCT / 100.0);

    if (used >= thresh) {
        qDebug() << "[ContextManager] Token used:" << used
                 << "/ budget:" << _tokenBudget
                 << "  threshold:" << thresh << "  -> compacting...";

        // 接近上限时发出预警信号
        emit sig_context_near_limit(used, _tokenBudget);

        compact();
    }
}

void ContextManager::clearHistory()
{
    if (_messages.isEmpty()) return;

    // 保留 system 消息
    if (_messages[0].toObject()["role"].toString() == "system") {
        QJsonArray sysOnly;
        sysOnly.append(_messages[0]);
        _messages = sysOnly;
    } else {
        _messages = QJsonArray();
    }
}

void ContextManager::reset()
{
    _messages = QJsonArray();
    _compactionCount = 0;
}

// ============================================================
// 上下文压缩
//
// 策略（无需 LLM API 的本地摘要）：
//   1. 找出 system 消息以外的所有消息
//   2. 保留最近 _keepRecentCount 条
//   3. 对更早的消息：
//      a. 过滤掉 tool / tool_calls 消息（中间过程，对摘要无价值）
//      b. 将剩余 user / assistant 消息串联成一段摘要文本
//      c. 作为新的 system 辅助消息插入（紧跟在原 system 之后）
// ============================================================
void ContextManager::compact()
{
    // 分离 system 消息 和 普通消息
    QJsonObject systemMsg;
    bool        hasSystem = false;
    QJsonArray  normalMsgs;

    for (const QJsonValue& v : _messages) {
        QJsonObject obj = v.toObject();
        if (!hasSystem && obj["role"].toString() == "system") {
            systemMsg = obj;
            hasSystem = true;
        } else {
            normalMsgs.append(v);
        }
    }

    // 普通消息不足以裁剪
    if (normalMsgs.size() <= _keepRecentCount) return;

    // 分组：旧消息（待摘要） vs 保留的最近消息
    int dropCount = normalMsgs.size() - _keepRecentCount;
    QJsonArray toSummarize;
    QJsonArray toKeep;

    for (int i = 0; i < normalMsgs.size(); ++i) {
        if (i < dropCount)
            toSummarize.append(normalMsgs[i]);
        else
            toKeep.append(normalMsgs[i]);
    }

    int tokensBefore = estimatedTotalTokens();

    // 生成摘要
    QString summary = summarizeMessages(toSummarize);
    QJsonObject summaryMsg;
    summaryMsg["role"]    = "system";
    summaryMsg["content"] = QString("[早期对话摘要] %1").arg(summary);

    // 重组 _messages：system → summaryMsg → toKeep
    QJsonArray newMsgs;
    if (hasSystem)       newMsgs.append(systemMsg);
    if (!summary.isEmpty()) newMsgs.append(summaryMsg);
    for (const QJsonValue& v : toKeep) newMsgs.append(v);

    _messages = newMsgs;
    _compactionCount++;

    int tokensAfter = estimatedTotalTokens();
    int saved = tokensBefore - tokensAfter;

    qDebug() << "[ContextManager] compacted #" << _compactionCount
             << "  dropped:" << dropCount
             << "  tokens:" << tokensBefore << "->" << tokensAfter
             << "  saved:" << saved;

    emit sig_context_compacted(dropCount, saved);
}

// ============================================================
// 本地摘要生成（不调用 API，仅串联 user/assistant 消息）
// ============================================================
QString ContextManager::summarizeMessages(const QJsonArray& msgs) const
{
    QStringList parts;
    for (const QJsonValue& v : msgs) {
        QJsonObject obj = v.toObject();
        QString role    = obj["role"].toString();
        QString content = obj["content"].toString();

        // 跳过 tool 中间过程消息（工具调用结果不是人类可读的摘要素材）
        if (role == "tool" || content.isEmpty()) continue;

        if (role == "user") {
            parts << QString("用户：%1").arg(content);
        } else if (role == "assistant") {
            parts << QString("助手：%1").arg(content);
        }
    }

    if (parts.isEmpty()) return QString();

    // 截断超长摘要（最多 500 字符）
    QString summary = parts.join(" | ");
    if (summary.size() > 500) {
        summary = summary.left(497) + "...";
    }
    return summary;
}
