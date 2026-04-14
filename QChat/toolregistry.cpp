#include "toolregistry.h"
#include <QJsonDocument>
#include <QDebug>

// ============================================================
// 单例实现
// ============================================================
ToolRegistry& ToolRegistry::instance()
{
    static ToolRegistry s_instance;
    return s_instance;
}

ToolRegistry::ToolRegistry(QObject* parent) : QObject(parent)
{
    qDebug() << "[ToolRegistry] initialized";
}

// ============================================================
// 工具注册 / 注销
// ============================================================
void ToolRegistry::registerTool(std::shared_ptr<IAgentTool> tool)
{
    if (!tool) return;
    const QString name = tool->name();
    if (_tools.contains(name)) {
        qWarning() << "[ToolRegistry] overwriting existing tool:" << name;
    }
    _tools[name] = tool;
    qDebug() << "[ToolRegistry] registered tool:" << name
             << "  total:" << _tools.size();
}

void ToolRegistry::unregisterTool(const QString& toolName)
{
    if (_tools.remove(toolName) > 0) {
        qDebug() << "[ToolRegistry] unregistered tool:" << toolName;
    }
}

// ============================================================
// 元数据查询
// ============================================================
bool ToolRegistry::hasTool(const QString& toolName) const
{
    return _tools.contains(toolName);
}

int ToolRegistry::toolCount() const
{
    return _tools.size();
}

QStringList ToolRegistry::toolNames() const
{
    return _tools.keys();
}

IAgentTool::RiskLevel ToolRegistry::riskLevel(const QString& toolName) const
{
    if (_tools.contains(toolName)) {
        return _tools[toolName]->riskLevel();
    }
    return IAgentTool::RiskLevel::Safe;
}

// ============================================================
// 工具执行分发
// ============================================================
QString ToolRegistry::execute(const QString& toolName, const QJsonObject& args)
{
    if (!_tools.contains(toolName)) {
        QString errMsg = QString("未知工具: %1").arg(toolName);
        qWarning() << "[ToolRegistry]" << errMsg;
        emit sig_tool_error(toolName, errMsg);

        QJsonObject err;
        err["error"] = errMsg;
        return QJsonDocument(err).toJson(QJsonDocument::Compact);
    }

    emit sig_tool_before_execute(toolName, args);
    qDebug() << "[ToolRegistry] executing:" << toolName;

    QString result;
    try {
        result = _tools[toolName]->execute(args);
    } catch (const std::exception& e) {
        QString errMsg = QString("工具执行异常: %1").arg(QString::fromStdString(e.what()));
        qWarning() << "[ToolRegistry]" << errMsg;
        emit sig_tool_error(toolName, errMsg);

        QJsonObject err;
        err["error"] = errMsg;
        return QJsonDocument(err).toJson(QJsonDocument::Compact);
    }

    emit sig_tool_after_execute(toolName, result);
    return result;
}

// ============================================================
// 构建 API tools 定义数组
// ============================================================
QJsonArray ToolRegistry::buildApiToolsDef() const
{
    QJsonArray tools;
    for (auto it = _tools.constBegin(); it != _tools.constEnd(); ++it) {
        tools.append(it.value()->toApiToolDef());
    }
    return tools;
}

// ============================================================
// 构建工具描述文本（注入 system prompt）
// ============================================================
QString ToolRegistry::buildToolsDescription() const
{
    if (_tools.isEmpty()) return QString();

    QStringList lines;
    lines << "## 可用工具";
    for (auto it = _tools.constBegin(); it != _tools.constEnd(); ++it) {
        lines << QString("- **%1**：%2").arg(it.value()->name()).arg(it.value()->description());
    }
    return lines.join("\n");
}
