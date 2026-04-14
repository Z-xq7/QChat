#include "knowledgebase.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QTextStream>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <cmath>

// ============================================================
// DocumentChunk 序列化
// ============================================================
QJsonObject DocumentChunk::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["docId"] = docId;
    obj["content"] = content;
    obj["chunkIndex"] = chunkIndex;
    obj["startPos"] = startPos;
    obj["endPos"] = endPos;
    obj["indexedAt"] = indexedAt.toString(Qt::ISODate);
    obj["metadata"] = metadata;
    return obj;
}

DocumentChunk DocumentChunk::fromJson(const QJsonObject& obj)
{
    DocumentChunk chunk;
    chunk.id = obj["id"].toString();
    chunk.docId = obj["docId"].toString();
    chunk.content = obj["content"].toString();
    chunk.chunkIndex = obj["chunkIndex"].toInt();
    chunk.startPos = obj["startPos"].toInt();
    chunk.endPos = obj["endPos"].toInt();
    chunk.indexedAt = QDateTime::fromString(obj["indexedAt"].toString(), Qt::ISODate);
    chunk.metadata = obj["metadata"].toObject();
    return chunk;
}

// ============================================================
// Document 序列化
// ============================================================
QJsonObject Document::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["filePath"] = filePath;
    obj["fileName"] = fileName;
    obj["fileType"] = fileType;
    obj["fileSize"] = fileSize;
    obj["modifiedTime"] = modifiedTime.toString(Qt::ISODate);
    obj["indexedAt"] = indexedAt.toString(Qt::ISODate);
    obj["summary"] = summary;
    obj["chunkCount"] = chunkCount;
    obj["metadata"] = metadata;
    return obj;
}

Document Document::fromJson(const QJsonObject& obj)
{
    Document doc;
    doc.id = obj["id"].toString();
    doc.filePath = obj["filePath"].toString();
    doc.fileName = obj["fileName"].toString();
    doc.fileType = obj["fileType"].toString();
    doc.fileSize = obj["fileSize"].toVariant().toLongLong();
    doc.modifiedTime = QDateTime::fromString(obj["modifiedTime"].toString(), Qt::ISODate);
    doc.indexedAt = QDateTime::fromString(obj["indexedAt"].toString(), Qt::ISODate);
    doc.summary = obj["summary"].toString();
    doc.chunkCount = obj["chunkCount"].toInt();
    doc.metadata = obj["metadata"].toObject();
    return doc;
}

QString Document::generateId() const
{
    // 基于文件路径生成稳定ID
    QByteArray hash = QCryptographicHash::hash(
        filePath.toUtf8(), QCryptographicHash::Md5);
    return hash.toHex().left(16);
}

// ============================================================
// KnowledgeBase 实现
// ============================================================
KnowledgeBase::KnowledgeBase(QObject* parent)
    : QObject(parent)
{
    // 默认存储路径
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    _storagePath = dataDir + "/knowledge_base";
    QDir().mkpath(_storagePath);

    // 加载已有索引
    loadIndex();
}

KnowledgeBase::~KnowledgeBase()
{
    saveIndex();
}

// ------------------------------------------------------------
// 文档管理
// ------------------------------------------------------------
bool KnowledgeBase::addDocument(const QString& filePath)
{
    // 支持资源文件路径 (:/path) 和普通文件路径
    bool isResourcePath = filePath.startsWith(":");
    
    if (!isResourcePath) {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            qWarning() << "[KnowledgeBase] File not found:" << filePath;
            return false;
        }
    }

    // 检查是否已存在
    QString docId = getFileHash(filePath);
    for (const auto& doc : _documents) {
        if (doc.id == docId) {
            // 检查是否需要更新
            if (isFileModified(doc)) {
                return updateDocument(docId);
            }
            qDebug() << "[KnowledgeBase] Document already indexed:" << filePath;
            return true;
        }
    }

    // 提取文本
    QString content = extractText(filePath);
    if (content.isEmpty()) {
        qWarning() << "[KnowledgeBase] Failed to extract text from:" << filePath;
        return false;
    }

    // 创建文档记录
    Document doc;
    doc.id = docId;
    doc.filePath = filePath;
    
    if (isResourcePath) {
        // 资源文件：从路径解析文件名
        int lastSlash = filePath.lastIndexOf('/');
        doc.fileName = (lastSlash >= 0) ? filePath.mid(lastSlash + 1) : filePath;
        doc.fileType = QFileInfo(doc.fileName).suffix().toLower();
        doc.fileSize = content.size();  // 用内容大小代替
        doc.modifiedTime = QDateTime::currentDateTime();  // 资源文件无修改时间
    } else {
        // 普通文件
        QFileInfo fileInfo(filePath);
        doc.fileName = fileInfo.fileName();
        doc.fileType = fileInfo.suffix().toLower();
        doc.fileSize = fileInfo.size();
        doc.modifiedTime = fileInfo.lastModified();
    }
    
    doc.indexedAt = QDateTime::currentDateTime();
    doc.summary = content.left(200) + (content.length() > 200 ? "..." : "");

    // 分片
    QVector<DocumentChunk> chunks = splitIntoChunks(docId, content);
    doc.chunkCount = chunks.size();

    // 保存
    _documents.append(doc);
    _chunks.append(chunks);

    saveIndex();

    emit sig_document_added(docId, doc.fileName);
    qDebug() << "[KnowledgeBase] Added document:" << doc.fileName
             << "with" << chunks.size() << "chunks";

    return true;
}

int KnowledgeBase::addDocuments(const QStringList& filePaths)
{
    int success = 0;
    int total = filePaths.size();

    for (int i = 0; i < filePaths.size(); ++i) {
        emit sig_indexing_progress(i + 1, total, QFileInfo(filePaths[i]).fileName());
        if (addDocument(filePaths[i])) {
            success++;
        }
    }

    emit sig_indexing_finished(success, total - success);
    return success;
}

int KnowledgeBase::addDirectory(const QString& dirPath, const QStringList& filters)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        qWarning() << "[KnowledgeBase] Directory not found:" << dirPath;
        return 0;
    }

    QStringList files;
    for (const QString& filter : filters) {
        files.append(dir.entryList(QStringList() << filter, QDir::Files, QDir::Name));
    }

    // 去重
    files.removeDuplicates();

    QStringList fullPaths;
    for (const QString& file : files) {
        fullPaths.append(dir.absoluteFilePath(file));
    }

    return addDocuments(fullPaths);
}

bool KnowledgeBase::removeDocument(const QString& docId)
{
    // 移除文档
    for (int i = 0; i < _documents.size(); ++i) {
        if (_documents[i].id == docId) {
            _documents.removeAt(i);
            break;
        }
    }

    // 移除相关分片
    for (int i = _chunks.size() - 1; i >= 0; --i) {
        if (_chunks[i].docId == docId) {
            _chunks.removeAt(i);
        }
    }

    saveIndex();
    emit sig_document_removed(docId);
    return true;
}

bool KnowledgeBase::updateDocument(const QString& docId)
{
    // ⚠️ 必须先保存文件路径，因为 removeDocument 会从 _documents 中删除它！
    QString filePath;
    for (int i = 0; i < _documents.size(); ++i) {
        if (_documents[i].id == docId) {
            filePath = _documents[i].filePath;
            break;
        }
    }

    if (filePath.isEmpty()) {
        qWarning() << "[KnowledgeBase] updateDocument: cannot find path for docId=" << docId;
        return false;
    }

    // 移除旧版本
    removeDocument(docId);

    // 用保存的路径重新添加
    bool result = addDocument(filePath);
    if (result) {
        emit sig_document_updated(docId);
    }
    return result;
}

// ------------------------------------------------------------
// 检索
// ------------------------------------------------------------
QVector<QPair<DocumentChunk, float>> KnowledgeBase::search(
    const QString& query,
    int topK,
    float /*minScore*/)
{
    if (_chunks.isEmpty() || query.trimmed().isEmpty()) {
        return {};
    }

    qDebug() << "[KB] Searching for:" << query.left(50)
             << "chunks:" << _chunks.size();

    // ========== 关键词提取（支持中文无空格分词） ==========
    QStringList keywords;
    
    // 检测是否包含中文
    bool hasChinese = false;
    for (const QChar& c : query) {
        if (c.script() == QChar::Script_Han) { hasChinese = true; break; }
    }

    if (hasChinese) {
        // ===== 中文分词策略：生成字符 n-gram + 过滤停用词 =====
        // 中文没有空格分隔，需要用不同粒度的子串作为关键词
        
        // 1. 先尝试按标点/空格拆分（处理中英混合）
        QRegularExpression splitPat(R"([\s，。、；：！？…—·\-\(\)\[\]""''《》]+)");
        QStringList segments = query.split(splitPat, Qt::SkipEmptyParts);
        
        static QSet<QString> singleStopWords = {
            "我", "你", "他", "她", "它", "们", "这", "那",
            "是", "在", "有", "和", "的", "了", "不", "也",
            "都", "就", "要", "会", "能", "可"
        };
        
        for (const QString& rawSeg : segments) {
            QString seg = rawSeg.trimmed().toLower();
            if (seg.isEmpty()) continue;
            
            int chineseCount = 0;
            for (const QChar& c : seg) {
                if (c.script() == QChar::Script_Han) chineseCount++;
            }
            
            if (chineseCount > 0 && chineseCount <= 2) {
                // 短中文片段（1-2字）：如果是停用词则跳过
                if (!singleStopWords.contains(seg)) {
                    keywords.append(seg);
                    qDebug() << "[KB] Short CN keyword:" << seg;
                }
            } else if (chineseCount >= 3) {
                // 长中文片段(>=3字)：生成 2-gram 和 3-gram 子串
                for (int len = qMin(seg.length(), 4); len >= 2; --len) {
                    for (int i = 0; i <= seg.length() - len; ++i) {
                        QString ngram = seg.mid(i, len);
                        if (ngram.length() >= 2 && !singleStopWords.contains(ngram)) {
                            keywords.append(ngram);
                        }
                    }
                }
                
                // 同时保留完整片段作为最长匹配
                if (seg.length() >= 2) {
                    keywords.append(seg);
                    qDebug() << "[KB] Full segment keyword:" << seg;
                }
            } else {
                // 纯英文/数字片段
                if (seg.length() >= 2) keywords.append(seg);
            }
        }
        
        // 去重但保持顺序（长的优先）
        QStringList deduped;
        QSet<QString> seen;
        for (int i = keywords.size() - 1; i >= 0; --i) {
            if (!seen.contains(keywords[i])) {
                seen.insert(keywords[i]);
                deduped.prepend(keywords[i]);  // 从后往前插入，保持原始顺序但长词在前
            }
        }
        keywords = deduped;
        
    } else {
        // 纯英文/其他文字：按空格拆分
        QRegularExpression wordSplit(R"([\s,.;:!?\-()\[\]\"'<>]+)");
        QStringList rawWords = query.toLower().split(wordSplit, Qt::SkipEmptyParts);
        static QSet<QString> enStopWords = {"i","you","he","she","it","we","they","is","am","are","was","were","be","been","being","have","has","had","do","does","did","will","would","could","should","may","might","can","need","a","an","the","this","that","these","those","my","your","his","her","its","our","their","what","which","who","whom","when","where","why","how","not","and","or","but","if","then","so","to","from","of","in","on","at","by","for","with","about","as"};
        for (const QString& w : rawWords) {
            if (w.length() >= 2 && !enStopWords.contains(w)) {
                keywords.append(w);
            }
        }
    }

    qDebug() << "[KB] Keywords extracted (" << keywords.size() << "):" << keywords;

    QVector<QPair<float, DocumentChunk>> scored;

    if (!keywords.isEmpty()) {
        // 有关键词 → 用关键词匹配打分
        for (const auto& chunk : _chunks) {
            QString lowerContent = chunk.content.toLower();
            float score = 0.0f;
            int matchedCount = 0;

            for (const QString& kw : keywords) {
                if (lowerContent.contains(kw)) {
                    matchedCount++;
                    int count = lowerContent.count(kw);
                    score += kw.length() * count;
                }
            }

            // ========== 用户KB宽松策略 ==========
            // 对于小规模知识库（<=10 chunks），即使没有关键词完全命中，
            // 也尝试单字匹配，避免词汇不匹配导致漏检
            if (matchedCount == 0 && _chunks.size() <= 10) {
                QSet<QString> kwChars;
                for (const QString& kw : keywords) {
                    for (int i = 0; i < kw.length(); ++i)
                        kwChars.insert(kw[i]);
                }
                // 过滤掉停用单字
                static QSet<QString> singleStopChars = {"的", "了", "是", "在", "有", "和",
                    "不", "也", "都", "就", "要", "会", "能", "我", "你", "他", "她", "它",
                    "这", "那", "个", "很", "太", "更", "最"};
                
                int charMatchCount = 0;
                for (const QString& ch : kwChars) {
                    if (!singleStopChars.contains(ch) && lowerContent.contains(ch)) {
                        charMatchCount++;
                        score += 1.0f * lowerContent.count(ch);
                    }
                }
                if (charMatchCount > 0) {
                    matchedCount = charMatchCount;
                    qDebug() << "[KB] Fallback char match:" << charMatchCount << "chars";
                }
            }

            if (matchedCount > 0) {
                float coverage = float(matchedCount) / keywords.size();
                score = coverage * (score / qMax(1, keywords.size()));
                scored.append({score, chunk});
                qDebug() << "[KB] Chunk keyword score:" << score
                         << "matched:" << matchedCount << "/" << keywords.size()
                         << "preview:" << chunk.content.left(40);
            }
        }
    } else {
        // 无有效关键词（比如纯停用词）→ 降级为 TF-IDF
        qDebug() << "[KB] No valid keywords, fallback to TF-IDF";
        QVector<float> queryVec = computeEmbedding(query);
        for (const auto& chunk : _chunks) {
            QVector<float> chunkVec = computeEmbedding(chunk.content);
            float sim = cosineSimilarity(queryVec, chunkVec);
            if (sim > 0) {
                scored.append({sim, chunk});
            }
        }
    }

    // 按分数排序
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    // ========== 小KB兜底策略 ==========
    // 如果知识库很小（<=10个chunks）且关键词没命中足够多的内容，
    // 直接返回所有 chunk 让 AI 判断（避免因词汇不匹配而漏检）
    if (scored.size() == 0 && _chunks.size() <= 10 && !query.trimmed().isEmpty()) {
        qDebug() << "[KB] Small KB fallback: returning all" << _chunks.size() << "chunks";
        QVector<QPair<DocumentChunk, float>> results;
        for (const auto& chunk : _chunks) {
            results.append(qMakePair(chunk, 0.01f));  // 低分标记为兜底结果
        }
        emit sig_search_completed(query, results.size());
        return results;
    }

    // 返回前K个（只要分数 > 0 就返回）
    QVector<QPair<DocumentChunk, float>> results;
    for (int i = 0; i < qMin(topK, scored.size()); ++i) {
        results.append({scored[i].second, scored[i].first});
    }

    qDebug() << "[KB] Search returned" << results.size() << "results";
    emit sig_search_completed(query, results.size());
    return results;
}

QVector<QPair<DocumentChunk, float>> KnowledgeBase::hybridSearch(
    const QString& query,
    int topK,
    float keywordWeight)
{
    // 语义检索
    auto semanticResults = search(query, topK * 2, 0.0f);

    // 关键词匹配分数
    QMap<QString, float> keywordScores;
    QStringList keywords = query.toLower().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    for (const auto& chunk : _chunks) {
        QString lowerContent = chunk.content.toLower();
        float score = 0.0f;
        for (const QString& kw : keywords) {
            if (lowerContent.contains(kw)) {
                score += 1.0f;
            }
        }
        if (score > 0) {
            keywordScores[chunk.id] = score / keywords.size();
        }
    }

    // 合并分数
    QMap<QString, QPair<DocumentChunk, float>> combined;

    // 语义分数 (1 - keywordWeight)
    for (const auto& [chunk, score] : semanticResults) {
        combined[chunk.id] = {chunk, score * (1.0f - keywordWeight)};
    }

    // 关键词分数
    for (auto it = keywordScores.begin(); it != keywordScores.end(); ++it) {
        // 找到对应的chunk
        for (const auto& chunk : _chunks) {
            if (chunk.id == it.key()) {
                if (combined.contains(chunk.id)) {
                    combined[chunk.id].second += it.value() * keywordWeight;
                } else {
                    combined[chunk.id] = {chunk, it.value() * keywordWeight};
                }
                break;
            }
        }
    }

    // 排序
    QVector<QPair<float, DocumentChunk>> scored;
    for (auto it = combined.begin(); it != combined.end(); ++it) {
        scored.append({it.value().second, it.value().first});
    }
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    // 返回前K个
    QVector<QPair<DocumentChunk, float>> results;
    for (int i = 0; i < qMin(topK, scored.size()); ++i) {
        results.append({scored[i].second, scored[i].first});
    }

    return results;
}

// ------------------------------------------------------------
// 查询接口
// ------------------------------------------------------------
QVector<Document> KnowledgeBase::listDocuments() const
{
    return _documents;
}

Document KnowledgeBase::getDocument(const QString& docId) const
{
    for (const auto& doc : _documents) {
        if (doc.id == docId) {
            return doc;
        }
    }
    return Document();
}

QVector<DocumentChunk> KnowledgeBase::getDocumentChunks(const QString& docId) const
{
    QVector<DocumentChunk> result;
    for (const auto& chunk : _chunks) {
        if (chunk.docId == docId) {
            result.append(chunk);
        }
    }
    return result;
}

// ------------------------------------------------------------
// 维护
// ------------------------------------------------------------
void KnowledgeBase::syncFiles()
{
    QVector<QString> toUpdate;
    QVector<QString> toRemove;

    for (const auto& doc : _documents) {
        // 资源文件不需要同步检查（路径以 ":" 开头）
        if (doc.filePath.startsWith(":")) {
            continue;
        }

        QFileInfo fileInfo(doc.filePath);
        if (!fileInfo.exists()) {
            toRemove.append(doc.id);
        } else if (fileInfo.lastModified() != doc.modifiedTime) {
            toUpdate.append(doc.id);
        }
    }

    for (const QString& id : toRemove) {
        removeDocument(id);
    }

    for (const QString& id : toUpdate) {
        updateDocument(id);
    }
}

void KnowledgeBase::clear()
{
    _documents.clear();
    _chunks.clear();
    saveIndex();
}

QString KnowledgeBase::stats() const
{
    qint64 totalSize = 0;
    for (const auto& doc : _documents) {
        totalSize += doc.fileSize;
    }

    return QString("文档数: %1 | 分片数: %2 | 总大小: %3 MB")
        .arg(_documents.size())
        .arg(_chunks.size())
        .arg(totalSize / 1024.0 / 1024.0, 0, 'f', 2);
}

void KnowledgeBase::setStoragePath(const QString& path)
{
    _storagePath = path;
    QDir().mkpath(_storagePath);
    loadIndex();
}

// ------------------------------------------------------------
// 文本提取
// ------------------------------------------------------------
QString KnowledgeBase::extractText(const QString& filePath)
{
    // 从路径解析后缀（支持资源文件路径 :/path/to/file.md）
    int lastDot = filePath.lastIndexOf('.');
    int lastSlash = filePath.lastIndexOf('/');
    QString suffix;
    if (lastDot > lastSlash && lastDot >= 0) {
        suffix = filePath.mid(lastDot + 1).toLower();
    }

    if (suffix == "pdf") {
        return extractTextFromPdf(filePath);
    } else if (suffix == "docx" || suffix == "doc") {
        return extractTextFromDocx(filePath);
    } else if (suffix == "md" || suffix == "markdown") {
        return extractTextFromMarkdown(filePath);
    } else {
        // 默认作为纯文本处理
        return extractTextFromTxt(filePath);
    }
}

QString KnowledgeBase::extractTextFromPdf(const QString& filePath)
{
    // TODO: 集成 PDF 解析库（如 poppler 或 pdfium）
    // 目前返回空，需要后续实现
    qWarning() << "[KnowledgeBase] PDF extraction not implemented yet";
    return "";
}

QString KnowledgeBase::extractTextFromDocx(const QString& filePath)
{
    // TODO: 集成 DOCX 解析（如 quazip + 解析 XML）
    qWarning() << "[KnowledgeBase] DOCX extraction not implemented yet";
    return "";
}

QString KnowledgeBase::extractTextFromTxt(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return "";
    }

    QTextStream stream(&file);
    stream.setAutoDetectUnicode(true);
    QString content = stream.readAll();
    file.close();

    return content;
}

QString KnowledgeBase::extractTextFromMarkdown(const QString& filePath)
{
    // Markdown 作为纯文本处理，简化清理避免正则回溯问题
    QString content = extractTextFromTxt(filePath);
    if (content.isEmpty()) {
        return content;
    }

    // 简单清理：直接移除常见 Markdown 标记字符，不做复杂正则匹配
    // 这样更安全，避免大文本下的正则性能问题
    QString result;
    result.reserve(content.size());
    
    for (int i = 0; i < content.length(); ++i) {
        QChar c = content[i];
        // 跳过 Markdown 格式字符
        if (c == '#' || c == '*' || c == '_' || c == '`' || c == '~' || c == '>') {
            continue;
        }
        // 跳过图片标记 ![...](...)
        if (c == '!' && i + 1 < content.length() && content[i + 1] == '[') {
            // 找到匹配的 )
            int end = content.indexOf(')', i);
            if (end > i) {
                i = end;
                continue;
            }
        }
        // 跳过链接标记 [...](...)
        if (c == '[') {
            int end = content.indexOf(')', i);
            if (end > i) {
                i = end;
                continue;
            }
        }
        result.append(c);
    }

    return result;
}

// ------------------------------------------------------------
// 文档分片
// ------------------------------------------------------------
QVector<DocumentChunk> KnowledgeBase::splitIntoChunks(const QString& docId, const QString& content)
{
    QVector<DocumentChunk> chunks;

    int pos = 0;
    int chunkIndex = 0;

    while (pos < content.length()) {
        int endPos = qMin(pos + _chunkSize, content.length());

        // 尝试在句子边界切分
        if (endPos < content.length()) {
            // 向前查找最近的句号、问号、换行
            int searchStart = endPos;
            int searchEnd = qMin(endPos + 100, content.length());
            int bestSplit = -1;

            for (int i = searchStart; i < searchEnd; ++i) {
                QChar c = content[i];
                if (c == QChar(0x3002) || c == QChar(0xFF1F) || c == QChar(0xFF01) ||
                    c == QChar('\n') || c == QChar('.') || c == QChar('?') || c == QChar('!')) {
                    bestSplit = i + 1;
                    break;
                }
            }

            if (bestSplit > 0) {
                endPos = bestSplit;
            }
        }

        DocumentChunk chunk;
        chunk.id = docId + "_" + QString::number(chunkIndex);
        chunk.docId = docId;
        chunk.content = content.mid(pos, endPos - pos).trimmed();
        chunk.chunkIndex = chunkIndex;
        chunk.startPos = pos;
        chunk.endPos = endPos;
        chunk.indexedAt = QDateTime::currentDateTime();

        if (!chunk.content.isEmpty()) {
            chunks.append(chunk);
            chunkIndex++;
        }

        // 下一步起始位置（考虑重叠）
        int nextPos = endPos - _chunkOverlap;
        if (nextPos <= pos) {
            nextPos = endPos;  // 确保前进，防止死循环
        }
        pos = nextPos;
    }

    return chunks;
}

// ------------------------------------------------------------
// 向量化（复用 AgentMemory 的方法）
// ------------------------------------------------------------
QVector<float> KnowledgeBase::computeEmbedding(const QString& text) const
{
    // 使用与 AgentMemory 相同的字符 bigram 方法
    QVector<float> vec(256, 0.0f);
    QString lower = text.toLower();

    // 使用字符双字（bigram）作为特征
    for (int i = 0; i < lower.length() - 1; ++i) {
        ushort c1 = lower[i].unicode() % 16;
        ushort c2 = lower[i + 1].unicode() % 16;
        int idx = (c1 * 16 + c2) % 256;
        vec[idx] += 1.0f;
    }

    // L2归一化
    float norm = 0.0f;
    for (float v : vec) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 0) {
        for (float& v : vec) v /= norm;
    }

    return vec;
}

float KnowledgeBase::cosineSimilarity(const QVector<float>& a, const QVector<float>& b) const
{
    if (a.size() != b.size()) return 0.0f;

    float dot = 0.0f;
    for (int i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
    }
    return dot;  // 已归一化，dot即cosine
}

// ------------------------------------------------------------
// 持久化
// ------------------------------------------------------------
bool KnowledgeBase::saveIndex()
{
    QJsonObject root;

    QJsonArray docArray;
    for (const auto& doc : _documents) {
        docArray.append(doc.toJson());
    }
    root["documents"] = docArray;

    QJsonArray chunkArray;
    for (const auto& chunk : _chunks) {
        chunkArray.append(chunk.toJson());
    }
    root["chunks"] = chunkArray;

    root["version"] = "1.0";
    root["savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QFile file(_storagePath + "/index.json");
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[KnowledgeBase] Failed to save index:" << file.errorString();
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

bool KnowledgeBase::loadIndex()
{
    QFile file(_storagePath + "/index.json");
    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[KnowledgeBase] Failed to open index:" << file.errorString();
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();

    _documents.clear();
    QJsonArray docArray = root["documents"].toArray();
    for (const QJsonValue& v : docArray) {
        _documents.append(Document::fromJson(v.toObject()));
    }

    _chunks.clear();
    QJsonArray chunkArray = root["chunks"].toArray();
    for (const QJsonValue& v : chunkArray) {
        _chunks.append(DocumentChunk::fromJson(v.toObject()));
    }

    qDebug() << "[KnowledgeBase] Loaded" << _documents.size()
             << "documents," << _chunks.size() << "chunks";

    return true;
}

// ------------------------------------------------------------
// 工具方法
// ------------------------------------------------------------
QString KnowledgeBase::getFileHash(const QString& filePath) const
{
    QByteArray hash = QCryptographicHash::hash(
        filePath.toUtf8(), QCryptographicHash::Md5);
    return hash.toHex().left(16);
}

bool KnowledgeBase::isFileModified(const Document& doc) const
{
    // 资源文件不会修改，直接返回 false
    if (doc.filePath.startsWith(":")) {
        return false;
    }
    
    QFileInfo fileInfo(doc.filePath);
    if (!fileInfo.exists()) {
        return false;
    }
    return fileInfo.lastModified() != doc.modifiedTime ||
           fileInfo.size() != doc.fileSize;
}
