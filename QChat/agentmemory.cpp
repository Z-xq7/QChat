#include "agentmemory.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QRegularExpression>
#include <QDir>
#include <QStandardPaths>
#include <QRandomGenerator>
#include <cmath>
#include "usermgr.h"

// ============================================================
// MemoryEntry 序列化
// ============================================================
QJsonObject MemoryEntry::toJson() const
{
    QJsonObject obj;
    obj["id"] = QString::number(id);
    obj["type"] = type;
    obj["content"] = content;
    obj["source"] = source;
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    obj["accessCount"] = accessCount;
    // Note: embedding 不持久化存储，用时动态计算以减少存储开销

    return obj;
}

MemoryEntry MemoryEntry::fromJson(const QJsonObject& obj)
{
    MemoryEntry entry;
    entry.id = obj["id"].toString().toLongLong();
    entry.type = obj["type"].toString();
    entry.content = obj["content"].toString();
    entry.source = obj["source"].toString();
    entry.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
    entry.accessCount = obj["accessCount"].toInt();
    // embedding不存储，用时动态计算
    return entry;
}



// ============================================================
// AgentMemory 实现
// ============================================================
AgentMemory::AgentMemory(QObject* parent)
    : QObject(parent)
{
    // 默认存储路径：应用数据目录
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    qDebug() << "[AgentMemory] AppDataLocation:" << dataDir;

    auto uid = UserMgr::GetInstance()->GetUid();
    QString agentDir = dataDir + "/user/" + QString::number(uid) + "/agent";
    if (!QDir().mkpath(agentDir)) {
        qWarning() << "[AgentMemory] Failed to create directory:" << dataDir;
    }

    _storagePath = agentDir + "/agent_memory.json";
    qDebug() << "[AgentMemory] Storage path:" << _storagePath;

    // 尝试加载已有记忆
    if (loadFromFile(_storagePath)) {
        qDebug() << "[AgentMemory] Successfully loaded existing memories";
    } else {
        qDebug() << "[AgentMemory] No existing memory file found or failed to load";
    }
}

AgentMemory::~AgentMemory()
{
    // 自动保存
    saveToFile(_storagePath);
}

// ------------------------------------------------------------
// 记忆存储
// ------------------------------------------------------------
qint64 AgentMemory::addMemory(const QString& type,
                               const QString& content,
                               const QString& source)
{
    if (content.trimmed().isEmpty()) return -1;

    // 检查是否已存在相似记忆
    if (hasSimilarMemory(content)) {
        qDebug() << "[AgentMemory] Skip duplicate memory:" << content.left(50);
        return -1;
    }

    // 如果超过上限，删除最旧的
    if (_memories.size() >= _maxMemories) {
        // 按访问次数+时间排序，删除最少访问的
        std::sort(_memories.begin(), _memories.end(),
            [](const MemoryEntry& a, const MemoryEntry& b) {
                if (a.accessCount != b.accessCount)
                    return a.accessCount < b.accessCount;
                return a.timestamp < b.timestamp;
            });
        qint64 removedId = _memories.first().id;
        _memories.removeFirst();
        emit sig_memory_removed(removedId);
    }

    MemoryEntry entry;
    entry.id = generateId();
    entry.type = type;
    entry.content = content.trimmed();
    entry.source = source;
    entry.timestamp = QDateTime::currentDateTime();
    entry.accessCount = 1;
    // embedding不存储，检索时动态计算

    _memories.append(entry);

    emit sig_memory_added(entry.id, entry.type, entry.content);
    qDebug() << "[AgentMemory] Added:" << entry.type << "-" << entry.content.left(50);

    // 立即保存到文件（避免程序崩溃丢失数据）
    if (!saveToFile(_storagePath)) {
        qWarning() << "[AgentMemory] Failed to save to:" << _storagePath;
    } else {
        qDebug() << "[AgentMemory] Saved to:" << _storagePath;
    }

    return entry.id;
}

void AgentMemory::extractAndStoreFromDialog(const QString& userMsg,
                                             const QString& assistantReply)
{
    if (!_autoExtract) return;

    // 从用户消息提取
    QStringList userMemories = extractPotentialMemories(userMsg);
    for (const QString& mem : userMemories) {
        addMemory("fact", mem, "user_dialog");
    }

    // 从助手回复提取（通常是确认信息）
    // 注意：助手回复中通常包含的是对用户的确认，不是用户的个人信息
    // 只提取包含明确事实的键值对格式内容
    QStringList assistantMemories = extractPotentialMemories(assistantReply);
    for (const QString& mem : assistantMemories) {
        // 只保留键值对格式的确认（如"居住地：西安"）
        if ((mem.contains("：") || mem.contains(":")) &&
            !mem.contains("已发送") && !mem.contains("已完成") &&
            !mem.contains("无法") && !mem.contains("没有") &&
            !mem.contains("不能") && !mem.contains("暂时")) {
            addMemory("fact", mem, "assistant_confirmed");
        }
    }
}

// ------------------------------------------------------------
// 记忆检索
// ------------------------------------------------------------
QVector<MemoryEntry> AgentMemory::searchByKeyword(const QString& keyword,
                                                   int maxResults) const
{
    QVector<MemoryEntry> results;
    QString lowerKeyword = keyword.toLower();

    // 扩展关键词映射表：查询词 -> 可能匹配的记忆关键词
    QMap<QString, QStringList> keywordExpansions = {
        {"住", {"居住地", "住址", "地址", "住", "在"}},
        {"居住", {"居住地", "住址", "地址"}},
        {"哪", {"居住地", "住址", "地址", "位置", "地方"}},
        {"名字", {"姓名", "名字", "叫"}},
        {"姓名", {"姓名", "名字", "叫"}},
        {"年龄", {"年龄", "岁", "年纪", "多大"}},
        {"岁", {"年龄", "岁", "年纪"}},
        {"性别", {"性别", "男", "女"}},
        {"身高", {"身高", "高", "cm", "米"}},
        {"体重", {"体重", "重", "kg", "公斤", "斤"}},
        {"学校", {"学校", "大学", "学院", "身份"}},
        {"专业", {"专业", "学院", "身份"}},
        {"身份", {"身份", "职业", "工作", "学生"}}
    };

    // 收集所有要匹配的关键词
    QStringList keywordsToMatch = {lowerKeyword};
    for (auto it = keywordExpansions.begin(); it != keywordExpansions.end(); ++it) {
        if (lowerKeyword.contains(it.key())) {
            for (const QString& expanded : it.value()) {
                if (!keywordsToMatch.contains(expanded)) {
                    keywordsToMatch.append(expanded);
                }
            }
        }
    }

    for (const auto& mem : _memories) {
        QString lowerContent = mem.content.toLower();
        for (const QString& kw : keywordsToMatch) {
            if (lowerContent.contains(kw)) {
                results.append(mem);
                break;  // 匹配到一个关键词即可
            }
        }
        if (results.size() >= maxResults) break;
    }

    return results;
}

QVector<MemoryEntry> AgentMemory::searchBySimilarity(const QString& query,
                                                      int maxResults) const
{
    if (_memories.isEmpty()) return {};

    QVector<float> queryVec = AgentMemory::computeEmbedding(query);
    QVector<QPair<float, MemoryEntry>> scored;

    for (const auto& mem : _memories) {
        // 动态计算embedding（不存储，减少内存和磁盘占用）
        QVector<float> memVec = AgentMemory::computeEmbedding(mem.content);
        float sim = AgentMemory::cosineSimilarity(queryVec, memVec);
        scored.append({sim, mem});
    }

    // 按相似度排序
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    QVector<MemoryEntry> results;
    for (int i = 0; i < qMin(maxResults, scored.size()); ++i) {
        results.append(scored[i].second);
    }

    return results;
}

QVector<MemoryEntry> AgentMemory::retrieveRelevantMemories(const QString& query,
                                                            int maxResults,
                                                            int maxTotalChars) const
{
    // 先关键词检索
    auto keywordResults = searchByKeyword(query, maxResults * 3);  // 多取一些用于排序

    // 再相似度检索
    auto similarityResults = searchBySimilarity(query, maxResults * 3);

    // 合并去重，按综合得分排序
    QMap<qint64, QPair<MemoryEntry, float>> combined;

    for (const auto& mem : keywordResults) {
        // 优先给 "fact" 类型更高基础分
        float baseScore = (mem.type == "fact") ? 1.5f : 1.0f;
        combined[mem.id] = {mem, baseScore};
    }

    for (const auto& mem : similarityResults) {
        // 动态计算embedding
        QVector<float> memVec = AgentMemory::computeEmbedding(mem.content);
        float simScore = AgentMemory::cosineSimilarity(AgentMemory::computeEmbedding(query), memVec);
        // 给 "fact" 类型的相似度加权
        if (mem.type == "fact") {
            simScore *= 1.2f;
        }
        if (combined.contains(mem.id)) {
            combined[mem.id].second += simScore;  // 累加分数
        } else {
            combined[mem.id] = {mem, simScore};
        }
    }

    // 转换为列表并排序
    QVector<QPair<float, MemoryEntry>> scored;
    for (auto it = combined.begin(); it != combined.end(); ++it) {
        scored.append({it.value().second, it.value().first});
    }
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    // 按maxResults和maxTotalChars限制返回结果
    // 优先返回 "fact" 类型的记忆（用户的实际信息）
    QVector<MemoryEntry> results;
    int totalChars = 0;

    // 第一轮：只添加 "fact" 类型的记忆
    for (int i = 0; i < scored.size() && results.size() < maxResults; ++i) {
        const auto& mem = scored[i].second;
        if (mem.type != "fact") continue;
        if (totalChars + mem.content.length() > maxTotalChars && !results.isEmpty()) {
            continue;
        }
        results.append(mem);
        totalChars += mem.content.length();
    }

    // 第二轮：如果还有空间，添加其他类型的记忆
    for (int i = 0; i < scored.size() && results.size() < maxResults; ++i) {
        const auto& mem = scored[i].second;
        if (mem.type == "fact") continue;  // 已添加过
        // 检查是否已存在（避免重复）
        bool exists = false;
        for (const auto& r : results) {
            if (r.id == mem.id) {
                exists = true;
                break;
            }
        }
        if (exists) continue;

        if (totalChars + mem.content.length() > maxTotalChars && !results.isEmpty()) {
            continue;
        }
        results.append(mem);
        totalChars += mem.content.length();
    }

    return results;
}

// ------------------------------------------------------------
// 记忆管理
// ------------------------------------------------------------
void AgentMemory::removeMemory(qint64 id)
{
    for (int i = 0; i < _memories.size(); ++i) {
        if (_memories[i].id == id) {
            _memories.removeAt(i);
            emit sig_memory_removed(id);
            break;
        }
    }
}

void AgentMemory::clearMemories()
{
    _memories.clear();
}

void AgentMemory::clearByType(const QString& type)
{
    for (int i = _memories.size() - 1; i >= 0; --i) {
        if (_memories[i].type == type) {
            qint64 id = _memories[i].id;
            _memories.removeAt(i);
            emit sig_memory_removed(id);
        }
    }
}

void AgentMemory::cleanupOldMemories(int daysThreshold)
{
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-daysThreshold);

    for (int i = _memories.size() - 1; i >= 0; --i) {
        if (_memories[i].timestamp < cutoff && _memories[i].accessCount < 3) {
            qint64 id = _memories[i].id;
            _memories.removeAt(i);
            emit sig_memory_removed(id);
        }
    }
}

void AgentMemory::cleanupLowQualityMemories()
{
    QStringList lowQualityPatterns = {
        "我是谁", "我住", "我的", "我在", "我是",  // 疑问句开头
        "无法", "没有", "不能", "暂时", "抱歉",  // 否定/道歉
        "目前", "现在", "今天", "明天"  // 时间词（通常是临时状态）
    };

    QVector<qint64> toRemove;

    // 第一轮：收集要删除的ID（不直接删除，避免信号槽问题）
    for (int i = 0; i < _memories.size(); ++i) {
        const auto& mem = _memories[i];
        bool isLowQuality = false;

        // 检查是否是疑问句
        if (mem.content.endsWith("吗") || mem.content.endsWith("呢") ||
            mem.content.endsWith("？") || mem.content.endsWith("?") ||
            mem.content.contains("谁") || mem.content.contains("哪") ||
            mem.content.contains("什么") || mem.content.contains("怎么")) {
            isLowQuality = true;
        }

        // 检查是否太短
        if (mem.content.length() < 5) {
            isLowQuality = true;
        }

        // 检查是否匹配低质量模式
        for (const QString& pattern : lowQualityPatterns) {
            if (mem.content.contains(pattern)) {
                // 但如果包含键值对格式（如"居住地：西安"），保留
                if (!mem.content.contains("：") && !mem.content.contains(":")) {
                    isLowQuality = true;
                    break;
                }
            }
        }

        // 检查是否是助手回复中的噪音（非键值对格式）
        if (mem.source == "assistant_reply" &&
            !mem.content.contains("：") && !mem.content.contains(":")) {
            isLowQuality = true;
        }

        if (isLowQuality) {
            toRemove.append(mem.id);
        }
    }

    // 第二轮：执行删除
    for (qint64 id : toRemove) {
        removeMemory(id);
        qDebug() << "[AgentMemory] Removed low-quality memory id:" << id;
    }

    // 保存清理后的结果
    if (!toRemove.isEmpty()) {
        saveToFile(_storagePath);
        qDebug() << "[AgentMemory] Cleanup finished, removed" << toRemove.size() << "memories";
    }
}

// ------------------------------------------------------------
// 持久化
// ------------------------------------------------------------
bool AgentMemory::saveToFile(const QString& filePath) const
{
    QJsonArray array;
    for (const auto& mem : _memories) {
        array.append(mem.toJson());
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[AgentMemory] Cannot open file for writing:" << filePath
                   << "Error:" << file.errorString();
        return false;
    }

    QByteArray data = QJsonDocument(array).toJson(QJsonDocument::Indented);
    qint64 written = file.write(data);
    file.close();

    if (written != data.size()) {
        qWarning() << "[AgentMemory] Failed to write complete data. Written:" << written
                   << "Expected:" << data.size();
        return false;
    }

    qDebug() << "[AgentMemory] Successfully saved" << _memories.size()
             << "memories to" << filePath;
    return true;
}

bool AgentMemory::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return false;

    _memories.clear();
    QJsonArray array = doc.array();
    for (const QJsonValue& v : array) {
        _memories.append(MemoryEntry::fromJson(v.toObject()));
    }

    qDebug() << "[AgentMemory] Loaded" << _memories.size() << "memories from" << filePath;
    return true;
}

// ------------------------------------------------------------
// 统计
// ------------------------------------------------------------
int AgentMemory::memoryCount() const
{
    return _memories.size();
}

int AgentMemory::memoryCountByType(const QString& type) const
{
    int count = 0;
    for (const auto& mem : _memories) {
        if (mem.type == type) count++;
    }
    return count;
}

QString AgentMemory::stats() const
{
    return QString("记忆总数: %1 | fact: %2 | preference: %3 | context: %4")
        .arg(memoryCount())
        .arg(memoryCountByType("fact"))
        .arg(memoryCountByType("preference"))
        .arg(memoryCountByType("context"));
}

// ------------------------------------------------------------
// 私有方法
// ------------------------------------------------------------
qint64 AgentMemory::generateId() const
{
    // 时间戳 + 随机数 (Qt6: QRandomGenerator 替代 qrand)
    qint64 ts = QDateTime::currentDateTime().toMSecsSinceEpoch();
    int rand = QRandomGenerator::global()->bounded(10000);
    return ts * 10000 + rand;
}

// 简单词频向量（基于字符n-gram，无需外部API）
QVector<float> AgentMemory::computeEmbedding(const QString& text)
{
    QVector<float> vec(256, 0.0f);  // 256维简单向量
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

float AgentMemory::cosineSimilarity(const QVector<float>& a, const QVector<float>& b)
{
    if (a.size() != b.size()) return 0.0f;

    float dot = 0.0f;
    for (int i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
    }
    return dot;  // 已归一化，dot即cosine
}

// 启发式提取潜在记忆
QStringList AgentMemory::extractPotentialMemories(const QString& text) const
{
    QStringList memories;

    // 模式1："我喜欢..." / "我讨厌..." / "我的...是..." / "我叫..."
    // 过滤掉疑问句（以吗、呢、？结尾的）
    QRegularExpression prefRegex("(我喜欢|我讨厌|我的|我叫|我是|我在|我住|我工作)[^,.。，;；]+");
    auto it = prefRegex.globalMatch(text);
    while (it.hasNext()) {
        QString match = it.next().captured(0).trimmed();
        // 过滤疑问句：以疑问词结尾或包含疑问词
        if (match.endsWith("吗") || match.endsWith("呢") || match.endsWith("？") || match.endsWith("?") ||
            match.contains("谁") || match.contains("哪") || match.contains("什么") || match.contains("怎么") ||
            match.contains("多少") || match.contains("几") || match.contains("是否")) {
            continue;
        }
        // 过滤太短的（可能只是"我的"这种不完整片段）
        if (match.length() < 5) {
            continue;
        }
        memories.append(match);
    }

    // 模式2：键值对格式（性别：男，年龄：21，等）
    // 匹配 "关键词：值" 或 "关键词:值" 格式，支持中文冒号和英文冒号
    QRegularExpression kvRegex("([\u4e00-\u9fa5_a-zA-Z]+[：:][^,.。，;；]+)");
    auto itKv = kvRegex.globalMatch(text);
    while (itKv.hasNext()) {
        QString kv = itKv.next().captured(1).trimmed();
        // 过滤掉太短的
        if (kv.length() >= 3) {
            memories.append(kv);
        }
    }

    // 模式3：包含具体信息的完整句子（以逗号、句号、分号分隔的独立信息单元）
    // 将输入按分隔符拆分，检查每个片段是否包含个人信息关键词
    QStringList separators = {"，", "。", ",", ".", "；", ";"};
    QString tempText = text;
    for (const QString& sep : separators) {
        tempText = tempText.replace(sep, "\n");
    }
    QStringList segments = tempText.split("\n", Qt::SkipEmptyParts);

    QStringList infoKeywords = {"性别", "年龄", "居住地", "身份", "身高", "体重",
                                 "职业", "专业", "学校", "大学", "学院", "电话", "邮箱"};

    for (QString segment : segments) {
        segment = segment.trimmed();
        if (segment.length() < 3 || segment.length() > 100) continue;

        // 过滤疑问句
        if (segment.endsWith("吗") || segment.endsWith("呢") || segment.endsWith("？") || segment.endsWith("?") ||
            segment.contains("谁") || segment.contains("哪") || segment.contains("什么") || segment.contains("怎么") ||
            segment.contains("多少") || segment.contains("几") || segment.contains("是否") ||
            segment.contains("哪里") || segment.contains("何处")) {
            continue;
        }

        // 检查是否包含个人信息关键词
        for (const QString& keyword : infoKeywords) {
            if (segment.contains(keyword)) {
                // 清理常见前缀（"我的"、"本人"等）
                segment.remove(QRegularExpression("^(我的|本人)"));
                segment = segment.trimmed();
                if (!segment.isEmpty() && !memories.contains(segment)) {
                    memories.append(segment);
                }
                break;
            }
        }
    }

    // 去重并过滤
    memories.removeDuplicates();

    // 后处理：如果原始文本很长且包含多个信息，尝试保留完整版本
    if (text.length() > 20 && memories.size() >= 2) {
        // 检查是否有被拆散的相关信息，尝试合并
        QStringList mergedMemories;
        for (const QString& mem : memories) {
            // 保留所有包含冒号的键值对（这是结构化的个人信息）
            if (mem.contains("：") || mem.contains(":")) {
                mergedMemories.append(mem);
            }
            // 保留以"我叫/我是/我的"开头的完整陈述
            else if (mem.startsWith("我叫") || mem.startsWith("我是") ||
                     mem.startsWith("我的") || mem.startsWith("我喜欢") ||
                     mem.startsWith("我讨厌")) {
                mergedMemories.append(mem);
            }
        }
        if (!mergedMemories.isEmpty()) {
            memories = mergedMemories;
        }
    }

    return memories;
}

bool AgentMemory::hasSimilarMemory(const QString& content, float threshold) const
{
    QVector<float> vec = AgentMemory::computeEmbedding(content);

    for (const auto& mem : _memories) {
        // 动态计算embedding
        QVector<float> memVec = AgentMemory::computeEmbedding(mem.content);
        float sim = AgentMemory::cosineSimilarity(vec, memVec);
        if (sim >= threshold) return true;
    }
    return false;
}
