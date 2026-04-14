#ifndef AGENTMEMORY_H
#define AGENTMEMORY_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

// ============================================================
// MemoryEntry — 单条记忆条目
// ============================================================
struct MemoryEntry {
    qint64 id;              // 唯一ID（时间戳+随机数）
    QString type;           // "fact" | "preference" | "context" | "summary"
    QString content;        // 记忆内容文本
    QString source;         // 来源：用户输入/工具结果/LLM总结
    QDateTime timestamp;    // 创建时间
    int accessCount;        // 访问次数（用于权重排序）
    // Note: embedding 不持久化存储，用时动态计算以减少存储开销

    MemoryEntry()
        : id(0), accessCount(0) {}

    QJsonObject toJson() const;           // 存储时调用（不含embedding）
    static MemoryEntry fromJson(const QJsonObject& obj);
};

// ============================================================
// AgentMemory — 长期记忆管理器
//
// 职责：
//   1. 存储跨会话的长期记忆（用户偏好、重要事实、上下文摘要）
//   2. 基于关键词/相似度检索相关记忆
//   3. 自动清理过期/低频记忆
//   4. 与 ContextManager 配合：短期上下文 + 长期记忆 = 完整上下文
// ============================================================
class AgentMemory : public QObject {
    Q_OBJECT
public:
    explicit AgentMemory(QObject* parent = nullptr);
    ~AgentMemory();

    // ---- 记忆存储 ----
    qint64 addMemory(const QString& type,
                     const QString& content,
                     const QString& source = "user");

    // 从对话中自动提取并存储记忆
    void extractAndStoreFromDialog(const QString& userMsg,
                                    const QString& assistantReply);

    // ---- 记忆检索 ----
    // 关键词匹配（简单快速）
    QVector<MemoryEntry> searchByKeyword(const QString& keyword,
                                          int maxResults = 5) const;

    // 相似度检索（基于简单词频向量，无需外部Embedding API）
    QVector<MemoryEntry> searchBySimilarity(const QString& query,
                                             int maxResults = 5) const;

    // 综合检索：关键词 + 相似度，去重后按相关性排序
    // maxResults: 最多返回几条记忆
    // maxTotalChars: 返回的记忆内容总字符数上限（用于控制Token消耗，默认800字符约200 token）
    QVector<MemoryEntry> retrieveRelevantMemories(const QString& query,
                                                   int maxResults = 5,
                                                   int maxTotalChars = 800) const;

    // ---- 记忆管理 ----
    void removeMemory(qint64 id);
    void clearMemories();
    void clearByType(const QString& type);

    // 自动清理：删除超过 N 天未访问的记忆
    void cleanupOldMemories(int daysThreshold = 30);

    // 清理低质量记忆：删除疑问句、太短的片段等
    void cleanupLowQualityMemories();

    // ---- 持久化 ----
    bool saveToFile(const QString& filePath) const;
    bool loadFromFile(const QString& filePath);

    // ---- 统计 ----
    int memoryCount() const;
    int memoryCountByType(const QString& type) const;
    QString stats() const;

    // ---- 配置 ----
    void setMaxMemories(int max) { _maxMemories = max; }
    void setAutoExtract(bool enabled) { _autoExtract = enabled; }

signals:
    // 记忆变更信号（供 UI 监听）
    void sig_memory_added(qint64 id, const QString& type, const QString& content);
    void sig_memory_removed(qint64 id);
    void sig_memory_retrieved(const QVector<MemoryEntry>& results);

private:
    QVector<MemoryEntry> _memories;
    int _maxMemories = 1000;        // 最大记忆条数
    bool _autoExtract = true;       // 是否自动从对话提取记忆
    QString _storagePath;           // 默认存储路径

    qint64 generateId() const;

    // 简单词频向量（用于离线相似度计算）- 静态方法供外部使用
    static QVector<float> computeEmbedding(const QString& text);
    static float cosineSimilarity(const QVector<float>& a, const QVector<float>& b);

    // 从文本提取潜在记忆（简单启发式规则）
    QStringList extractPotentialMemories(const QString& text) const;

    // 检查是否已存在相似记忆（避免重复存储）
    bool hasSimilarMemory(const QString& content, float threshold = 0.85f) const;
};

#endif // AGENTMEMORY_H
