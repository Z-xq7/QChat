#ifndef RAGRETRIEVER_H
#define RAGRETRIEVER_H

#include <QObject>
#include <QString>
#include "knowledgebase.h"

// ============================================================
// RagRetriever — RAG 检索增强生成
//
// 职责：
//   1. 将用户查询转换为知识库检索
//   2. 整合检索结果到 AI 提示词中
//   3. 管理上下文窗口，避免超出 token 限制
//   4. 支持多轮对话中的知识库引用
// ============================================================
class RagRetriever : public QObject {
    Q_OBJECT
public:
    explicit RagRetriever(KnowledgeBase* kb, QObject* parent = nullptr);

    // 检索并生成增强的提示词
    QString augmentPrompt(
        const QString& userQuery,
        const QString& originalSystemPrompt = "",
        int maxChunks = 3,
        int maxCharsPerChunk = 800
    );

    // 检索并返回格式化的上下文
    QString retrieveContext(
        const QString& query,
        int maxChunks = 3,
        int maxCharsPerChunk = 800
    );

    // 检查是否有相关文档
    bool hasRelevantDocuments(const QString& query, float threshold = 0.5f);

    // 获取检索统计
    struct RetrievalStats {
        int totalQueries;
        int successfulQueries;
        float avgScore;
        int totalCharsRetrieved;
    };
    RetrievalStats getStats() const;

    // 配置
    void setMinScore(float score) { _minScore = score; }
    void setMaxContextLength(int length) { _maxContextLength = length; }

signals:
    void sig_retrieval_started(const QString& query);
    void sig_retrieval_completed(const QString& query, int chunksFound, float avgScore);

private:
    KnowledgeBase* _kb;
    float _minScore = 0.5f;
    int _maxContextLength = 2000;

    mutable RetrievalStats _stats;

    // 格式化检索结果为提示词
    QString formatRetrievedChunks(
        const QVector<QPair<DocumentChunk, float>>& chunks,
        int maxCharsPerChunk
    ) const;

    // 截断文本到指定长度
    QString truncateText(const QString& text, int maxLength) const;
};

#endif // RAGRETRIEVER_H
