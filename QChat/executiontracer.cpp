#include "executiontracer.h"
#include <QJsonDocument>
#include <QDebug>

// ============================================================
// 构造
// ============================================================
ExecutionTracer::ExecutionTracer(QObject* parent)
    : QObject(parent),
      _sessionActive(false),
      _nextStepIndex(0)
{
}

// ============================================================
// 会话管理
// ============================================================
void ExecutionTracer::beginSession(const QString& userQuery)
{
    _currentSteps.clear();
    _userQuery     = userQuery;
    _sessionActive = true;
    _sessionStart  = QDateTime::currentDateTime();
    _nextStepIndex = 1;

    qDebug() << "[Tracer] session started, query:" << userQuery;
    emit sig_session_started(userQuery);
}

void ExecutionTracer::endSession()
{
    if (!_sessionActive) return;
    _sessionActive = false;

    int totalElapsed = 0;
    for (const auto& step : _currentSteps) totalElapsed += step.elapsedMs;

    qDebug() << "[Tracer] session ended, steps:" << _currentSteps.size()
             << "  totalMs:" << totalElapsed;
    emit sig_session_ended(_currentSteps.size(), totalElapsed);
}

// ============================================================
// 步骤管理
// ============================================================
int ExecutionTracer::beginStep(const QString& toolName, const QJsonObject& args)
{
    TraceStep step;
    step.stepIndex = _nextStepIndex++;
    step.toolName  = toolName;
    step.args      = args;
    step.status    = TraceStep::Status::Running;
    step.startTime = QDateTime::currentDateTime();

    _currentSteps.append(step);

    qDebug() << "[Tracer] step" << step.stepIndex << "started:" << toolName;
    emit sig_step_started(step.stepIndex, toolName, args);

    return step.stepIndex;
}

void ExecutionTracer::endStepSuccess(int stepIndex, const QString& result)
{
    for (auto& step : _currentSteps) {
        if (step.stepIndex == stepIndex) {
            step.status    = TraceStep::Status::Success;
            step.result    = result;
            step.endTime   = QDateTime::currentDateTime();
            step.elapsedMs = static_cast<int>(step.startTime.msecsTo(step.endTime));

            qDebug() << "[Tracer] step" << stepIndex << "success,"
                     << step.elapsedMs << "ms";
            emit sig_step_finished(stepIndex, true, result);
            return;
        }
    }
    qWarning() << "[Tracer] endStepSuccess: stepIndex" << stepIndex << "not found";
}

void ExecutionTracer::endStepFailed(int stepIndex, const QString& errorMsg)
{
    for (auto& step : _currentSteps) {
        if (step.stepIndex == stepIndex) {
            step.status    = TraceStep::Status::Failed;
            step.errorMsg  = errorMsg;
            step.endTime   = QDateTime::currentDateTime();
            step.elapsedMs = static_cast<int>(step.startTime.msecsTo(step.endTime));

            qDebug() << "[Tracer] step" << stepIndex << "failed:" << errorMsg;
            emit sig_step_finished(stepIndex, false, errorMsg);
            return;
        }
    }
    qWarning() << "[Tracer] endStepFailed: stepIndex" << stepIndex << "not found";
}

// ============================================================
// 数据查询
// ============================================================
const TraceStep* ExecutionTracer::getStep(int stepIndex) const
{
    for (const auto& step : _currentSteps) {
        if (step.stepIndex == stepIndex) return &step;
    }
    return nullptr;
}

// ============================================================
// 格式化：纯文本摘要（debug 用）
// ============================================================
QString ExecutionTracer::formatSummary() const
{
    if (_currentSteps.isEmpty()) return "（无工具调用）";

    QStringList lines;
    lines << QString("执行链摘要（共 %1 步）：").arg(_currentSteps.size());

    for (const auto& step : _currentSteps) {
        QString icon = step.statusIcon();
        QString name = TraceStep::displayName(step.toolName);
        QString time = (step.elapsedMs > 0)
                       ? QString(" [%1ms]").arg(step.elapsedMs)
                       : QString();
        lines << QString("  %1 步骤%2 · %3%4")
                 .arg(icon).arg(step.stepIndex).arg(name).arg(time);
        if (!step.isSuccess() && !step.errorMsg.isEmpty()) {
            lines << QString("    ↳ 错误：%1").arg(step.errorMsg);
        }
    }
    return lines.join("\n");
}

// ============================================================
// 格式化：单步 HTML（供 UI 气泡渲染）
// ============================================================
QString ExecutionTracer::formatStepHtml(const TraceStep& step) const
{
    QString bgColor, borderColor, textColor;

    switch (step.status) {
        case TraceStep::Status::Running:
            bgColor     = "#fff8e1";
            borderColor = "#f57c00";
            textColor   = "#f57c00";
            break;
        case TraceStep::Status::Success:
            bgColor     = "#e8f5e9";
            borderColor = "#388e3c";
            textColor   = "#388e3c";
            break;
        case TraceStep::Status::Failed:
            bgColor     = "#ffebee";
            borderColor = "#d32f2f";
            textColor   = "#d32f2f";
            break;
        default:
            bgColor     = "#f5f5f5";
            borderColor = "#bdbdbd";
            textColor   = "#757575";
    }

    QString icon    = step.statusIcon();
    QString name    = TraceStep::displayName(step.toolName);
    QString timeStr;
    if (step.status == TraceStep::Status::Success ||
        step.status == TraceStep::Status::Failed) {
        timeStr = QString(" <span style='color:#bdbdbd; font-size:10px;'>%1ms</span>")
                  .arg(step.elapsedMs);
    }

    QString detail;
    if (step.status == TraceStep::Status::Failed && !step.errorMsg.isEmpty()) {
        detail = QString("<div style='margin-top:2px; font-size:11px; color:#d32f2f;'>↳ %1</div>")
                 .arg(step.errorMsg.toHtmlEscaped());
    }

    return QString(
        "<table width='100%%' border='0' cellspacing='0' cellpadding='0' style='margin-bottom: 4px;'>"
        "<tr><td align='left'>"
        "<table bgcolor='%1' style='border-radius:4px; border-left:3px solid %2;'>"
        "<tr><td style='padding:5px 10px;'>"
        "<span style='color:%3; font-size:12px;'>%4 [步骤%5] %6%7</span>"
        "%8"
        "</td></tr>"
        "</table>"
        "</td></tr>"
        "</table>"
    ).arg(bgColor).arg(borderColor).arg(textColor)
     .arg(icon).arg(step.stepIndex).arg(name).arg(timeStr).arg(detail);
}

// ============================================================
// 格式化：完整执行链 HTML（供执行面板整体渲染）
// ============================================================
QString ExecutionTracer::formatFullHtml() const
{
    if (_currentSteps.isEmpty()) return QString();

    QString html;
    html += QString(
        "<table width='100%%' border='0' cellspacing='0' cellpadding='0' style='margin-bottom:8px;'>"
        "<tr><td align='left'>"
        "<div style='color:#888; font-size:11px; margin-bottom:4px;'>🔧 执行链（%1 步）</div>"
    ).arg(_currentSteps.size());

    for (const auto& step : _currentSteps) {
        html += formatStepHtml(step);
    }

    html += "</td></tr></table>";
    return html;
}
