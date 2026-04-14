#ifndef KNOWLEDGEBASE_H
#define KNOWLEDGEBASE_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QSet>

// ============================================================
// DocumentChunk — 文档分片
// ============================================================
struct DocumentChunk {
    QString id;                 // 唯一ID
    QString docId;              // 所属文档ID
    QString content;            // 文本内容
    int chunkIndex;             // 在文档中的分片序号
    int startPos;               // 在原文档中的起始位置
    int endPos;                 // 在原文档中的结束位置
    QDateTime indexedAt;        // 索引时间

    // 元数据
    QJsonObject metadata;

    DocumentChunk() : chunkIndex(0), startPos(0), endPos(0) {}

    QJsonObject toJson() const;
    static DocumentChunk fromJson(const QJsonObject& obj);
};

// ============================================================
// Document — 知识库文档
// ============================================================
struct Document {
    QString id;                 // 唯一ID（文件路径哈希或UUID）
    QString filePath;           // 原始文件路径
    QString fileName;           // 文件名
    QString fileType;           // 文件类型：pdf, docx, txt, md, etc.
    qint64 fileSize;            // 文件大小
    QDateTime modifiedTime;     // 文件修改时间
    QDateTime indexedAt;        // 索引时间
    QString summary;            // 文档摘要
    int chunkCount;             // 分片数量

    // 元数据
    QJsonObject metadata;

    Document() : fileSize(0), chunkCount(0) {}

    QJsonObject toJson() const;
    static Document fromJson(const QJsonObject& obj);
    QString generateId() const;
};

// ============================================================
// KnowledgeBase — 本地知识库管理器
//
// 职责：
//   1. 文档导入：支持 PDF、Word、TXT、Markdown 等格式
//   2. 文档分片：按语义/长度切分，保持上下文
//   3. 向量化：使用本地模型或 API 生成 embedding
//   4. 检索：基于相似度检索相关文档片段
//   5. 增量更新：检测文件变更，自动更新索引
// ============================================================
class KnowledgeBase : public QObject {
    Q_OBJECT
public:
    explicit KnowledgeBase(QObject* parent = nullptr);
    ~KnowledgeBase();

    // ---- 文档管理 ----
    // 添加文件到知识库
    bool addDocument(const QString& filePath);
    // 批量添加文件
    int addDocuments(const QStringList& filePaths);
    // 添加目录（递归）
    int addDirectory(const QString& dirPath, const QStringList& filters = {"*.txt", "*.md", "*.pdf", "*.docx"});

    // 移除文档
    bool removeDocument(const QString& docId);
    // 更新文档（文件变更后重新索引）
    bool updateDocument(const QString& docId);

    // ---- 检索 ----
    // 语义检索（基于 embedding 相似度）
    QVector<QPair<DocumentChunk, float>> search(
        const QString& query,
        int topK = 5,
        float minScore = 0.6f
    );

    // 混合检索：关键词 + 语义
    QVector<QPair<DocumentChunk, float>> hybridSearch(
        const QString& query,
        int topK = 5,
        float keywordWeight = 0.3f
    );

    // ---- 查询接口 ----
    // 获取文档列表
    QVector<Document> listDocuments() const;
    // 获取文档详情
    Document getDocument(const QString& docId) const;
    // 获取文档的所有分片
    QVector<DocumentChunk> getDocumentChunks(const QString& docId) const;

    // ---- 维护 ----
    // 检查文件变更，更新索引
    void syncFiles();
    // 清空知识库
    void clear();
    // 统计信息
    QString stats() const;

    // ---- 配置 ----
    void setChunkSize(int size) { _chunkSize = size; }
    void setChunkOverlap(int overlap) { _chunkOverlap = overlap; }
    void setStoragePath(const QString& path);

signals:
    void sig_document_added(const QString& docId, const QString& fileName);
    void sig_document_removed(const QString& docId);
    void sig_document_updated(const QString& docId);
    void sig_indexing_progress(int current, int total, const QString& fileName);
    void sig_indexing_finished(int successCount, int failCount);
    void sig_search_completed(const QString& query, int resultCount);

private:
    QVector<Document> _documents;
    QVector<DocumentChunk> _chunks;
    QString _storagePath;
    int _chunkSize = 500;       // 每片字符数
    int _chunkOverlap = 50;     // 重叠字符数（保持上下文）

    // 文本提取
    QString extractText(const QString& filePath);
    QString extractTextFromPdf(const QString& filePath);
    QString extractTextFromDocx(const QString& filePath);
    QString extractTextFromTxt(const QString& filePath);
    QString extractTextFromMarkdown(const QString& filePath);

    // 文档分片
    QVector<DocumentChunk> splitIntoChunks(const QString& docId, const QString& content);

    // 向量化（使用与 AgentMemory 相同的本地方法，或调用 API）
    QVector<float> computeEmbedding(const QString& text) const;
    float cosineSimilarity(const QVector<float>& a, const QVector<float>& b) const;

    // 持久化
    bool saveIndex();
    bool loadIndex();

    // 工具方法
    QString getFileHash(const QString& filePath) const;
    bool isFileModified(const Document& doc) const;
};

#endif // KNOWLEDGEBASE_H
