#include "ragretriever.h"
#include <QDebug>

RagRetriever::RagRetriever(KnowledgeBase* kb, QObject* parent)
    : QObject(parent)
    , _kb(kb)
    , _stats{0, 0, 0.0f, 0}
{
}

QString RagRetriever::augmentPrompt(
    const QString& userQuery,
    const QString& originalSystemPrompt,
    int maxChunks,
    int maxCharsPerChunk)
{
    emit sig_retrieval_started(userQuery);

    // 检索相关文档
    auto results = _kb->search(userQuery, maxChunks, _minScore);

    if (results.isEmpty()) {
        emit sig_retrieval_completed(userQuery, 0, 0.0f);
        // 没有检索到相关内容，返回原始提示词
        return originalSystemPrompt.isEmpty()
            ? "你是一个 helpful 的 AI 助手。"
            : originalSystemPrompt;
    }

    // 计算平均分
    float totalScore = 0.0f;
    for (const auto& [chunk, score] : results) {
        totalScore += score;
    }
    float avgScore = totalScore / results.size();

    // 更新统计
    _stats.totalQueries++;
    _stats.successfulQueries++;
    _stats.avgScore = (_stats.avgScore * (_stats.successfulQueries - 1) + avgScore) / _stats.successfulQueries;

    // 格式化检索结果
    QString context = formatRetrievedChunks(results, maxCharsPerChunk);
    _stats.totalCharsRetrieved += context.length();

    emit sig_retrieval_completed(userQuery, results.size(), avgScore);

    // 构建增强的提示词
    QString augmentedPrompt;

    if (!originalSystemPrompt.isEmpty()) {
        augmentedPrompt = originalSystemPrompt + "\\n\\n";
    }

    augmentedPrompt += "=== 参考文档 ===\\n";
    augmentedPrompt += context;
    augmentedPrompt += "\\n================\\n\\n";
    augmentedPrompt += "基于以上参考文档回答用户问题。如果参考文档中没有相关信息，请明确告知。";

    return augmentedPrompt;
}

QString RagRetriever::retrieveContext(
    const QString& query,
    int maxChunks,
    int maxCharsPerChunk)
{
    auto results = _kb->search(query, maxChunks, _minScore);

    if (results.isEmpty()) {
        return "";
    }

    return formatRetrievedChunks(results, maxCharsPerChunk);
}

bool RagRetriever::hasRelevantDocuments(const QString& query, float threshold)
{
    auto results = _kb->search(query, 1, threshold);
    return !results.isEmpty();
}

RagRetriever::RetrievalStats RagRetriever::getStats() const
{
    return _stats;
}

QString RagRetriever::formatRetrievedChunks (
    const QVector<QPair<DocumentChunk, float>>& chunks,
    int maxCharsPerChunk) const
{
    QStringList formatted;

    for (int i = 0; i < chunks.size(); ++i) {
        const auto& [chunk, score] = chunks[i];

        QString text = truncateText(chunk.content, maxCharsPerChunk);

        QString entry = QString("[文档%1 | 相关度: %2%]\\n%3")
            .arg(i + 1)
            .arg(static_cast<int>(score * 100))
            .arg(text);

        formatted.append(entry);
    }

    return formatted.join("\\n\\n");
}

QString RagRetriever::truncateText(const QString& text, int maxLength) const
{
    if (text.length() <= maxLength) {
        return text;
    }

    // 在句子边界截断
    QString truncated = text.left(maxLength);

    // 查找最后一个句子结束符
    int lastPeriod = truncated.lastIndexOf(QChar(0x3002));  // 。
    int lastNewline = truncated.lastIndexOf(QChar('\n'));
    int lastDot = truncated.lastIndexOf(QChar('.'));

    int cutPos = qMax(qMax(lastPeriod, lastNewline), lastDot);
    if (cutPos > maxLength * 0.7) {  // 如果找到的位置不太靠前
        truncated = truncated.left(cutPos + 1);
    }

    return truncated + "...";
}
