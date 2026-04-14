#include "safetyguard.h"
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>

// ============================================================
// 构造 —— 初始化规则库
// ============================================================
SafetyGuard::SafetyGuard(QObject* parent)
    : QObject(parent),
      _maxCallsPerMinute(20),
      _callCountInWindow(0),
      _windowStartMs(QDateTime::currentMSecsSinceEpoch()),
      _rateLimitEnabled(true),
      _promptInjectionEnabled(true),
      _totalBlocked(0),
      _totalWarned(0)
{
    // ---- Prompt 注入检测规则（中英文混合正则）----
    // 目标：识别用户试图覆盖系统 prompt、伪装成 AI、绕过限制等行为
    _injectionPatterns = {
        // 忽略/覆盖指令
        R"(忽略.{0,20}(上面|之前|前面|以上|所有).{0,20}(指令|命令|提示|规则))",
        R"(ignore.{0,20}(above|previous|prior|all).{0,20}(instruction|prompt|rule|command))",
        R"(disregard.{0,10}(previous|above|all))",
        R"(forget.{0,10}(instruction|what i said|previous))",

        // 直接要求输出系统 prompt
        R"(输出.{0,10}(系统|system).{0,10}(提示|prompt))",
        R"(print.{0,10}system.{0,10}prompt)",
        R"(show.{0,10}(your|the).{0,10}(system|initial).{0,10}prompt)",
        R"(reveal.{0,10}(system|hidden).{0,10}(prompt|instruction))",

        // 角色扮演绕过
        R"(pretend.{0,20}(you are|you're).{0,20}(not|without|no).{0,20}(restriction|rule|limit))",
        R"(act as.{0,20}(jailbreak|DAN|unrestricted|unfiltered))",
        R"(you are now.{0,20}(DAN|jailbroken|unfiltered))",
        R"(扮演.{0,20}没有.{0,20}(限制|规则|约束))",

        // 直接 prompt 注入标记
        R"(\[\s*system\s*\]|\[\s*SYSTEM\s*\])",
        R"(<\s*system\s*>|<\s*SYSTEM\s*>)",
        R"(###\s*(instruction|system|prompt))",

        // 要求执行危险操作
        R"(删除.{0,10}(所有|全部).{0,5}(好友|消息|数据|记录))",
    };

    // ---- 敏感内容关键词（基础过滤）----
    // 仅拦截明显的恶意内容，不过度拦截
    _sensitiveKeywords = {
        // 极端内容（此处仅做示范，实际项目可接入专业内容审核 API）
        // 出于演示目的，关键词列表保持精简
    };
}

// ============================================================
// 主接口：检查用户输入
// ============================================================
SafetyResult SafetyGuard::checkUserInput(const QString& input)
{
    if (input.trimmed().isEmpty()) return SafetyResult();

    // 1. Prompt 注入检测
    if (_promptInjectionEnabled) {
        SafetyResult r = detectPromptInjection(input);
        if (!r.passed()) {
            if (r.blocked()) _totalBlocked++;
            else             _totalWarned++;
            emit sig_safety_event(r.category, r.reason, r.level);
            return r;
        }
    }

    // 2. 内容过滤
    SafetyResult r = checkContent(input);
    if (!r.passed()) {
        if (r.blocked()) _totalBlocked++;
        else             _totalWarned++;
        emit sig_safety_event(r.category, r.reason, r.level);
        return r;
    }

    return SafetyResult(); // Pass
}

// ============================================================
// 主接口：检查工具调用
// ============================================================
SafetyResult SafetyGuard::checkToolCall(const QString& toolName,
                                         const QJsonObject& args,
                                         IAgentTool::RiskLevel riskLevel)
{
    // 1. 频率限制
    if (_rateLimitEnabled) {
        SafetyResult r = checkRateLimit();
        if (!r.passed()) {
            _totalBlocked++;
            emit sig_safety_event(r.category, r.reason, r.level);
            return r;
        }
    }

    // 2. 高风险工具检查
    if (riskLevel == IAgentTool::RiskLevel::High) {
        QString reason = QString("工具 [%1] 为高风险操作，此操作不可逆，当前版本已禁止执行。")
                         .arg(toolName);
        SafetyResult r(SafetyResult::Level::Block, reason, "risk_tool");
        _totalBlocked++;
        emit sig_safety_event(r.category, r.reason, r.level);
        qWarning() << "[SafetyGuard] High-risk tool blocked:" << toolName;
        return r;
    }

    // 3. Medium 风险工具：检查参数中是否含有异常内容
    if (riskLevel == IAgentTool::RiskLevel::Medium) {
        // 对 send_message 工具：检查消息内容
        if (toolName == "send_message") {
            QString content = args["content"].toString();

            // 拦截：消息内容超过 1000 字（防止 spam）
            if (content.length() > 1000) {
                SafetyResult r(SafetyResult::Level::Block,
                               QString("消息内容过长（%1 字），最多支持 1000 字").arg(content.length()),
                               "content_violation");
                _totalBlocked++;
                emit sig_safety_event(r.category, r.reason, r.level);
                return r;
            }

            // 警告：消息内容为空（参数异常）
            if (content.trimmed().isEmpty()) {
                SafetyResult r(SafetyResult::Level::Block,
                               "消息内容不能为空", "content_violation");
                _totalBlocked++;
                emit sig_safety_event(r.category, r.reason, r.level);
                return r;
            }
        }
    }

    // 更新频率计数
    _callCountInWindow++;
    return SafetyResult(); // Pass
}

// ============================================================
// Prompt 注入检测
// ============================================================
SafetyResult SafetyGuard::detectPromptInjection(const QString& input)
{
    for (const QString& pattern : _injectionPatterns) {
        QRegularExpression re(pattern,
            QRegularExpression::CaseInsensitiveOption |
            QRegularExpression::UseUnicodePropertiesOption);

        if (!re.isValid()) {
            qWarning() << "[SafetyGuard] invalid regex:" << pattern;
            continue;
        }

        if (re.match(input).hasMatch()) {
            QString reason = QString(
                "检测到疑似 Prompt 注入攻击，已拦截。\n"
                "如果这是误判，请换一种方式表达你的需求。"
            );
            qWarning() << "[SafetyGuard] Prompt injection detected, pattern:" << pattern;
            return SafetyResult(SafetyResult::Level::Block, reason, "prompt_injection");
        }
    }
    return SafetyResult();
}

// ============================================================
// 内容过滤
// ============================================================
SafetyResult SafetyGuard::checkContent(const QString& input)
{
    if (_sensitiveKeywords.isEmpty()) return SafetyResult();

    for (const QString& kw : _sensitiveKeywords) {
        if (input.contains(kw, Qt::CaseInsensitive)) {
            return SafetyResult(
                SafetyResult::Level::Block,
                "输入包含不允许的内容，已拦截。",
                "content_violation"
            );
        }
    }
    return SafetyResult();
}

// ============================================================
// 频率限制
// ============================================================
SafetyResult SafetyGuard::checkRateLimit()
{
    qint64 now     = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = now - _windowStartMs;

    // 每 60 秒重置一次窗口
    if (elapsed > 60000) {
        _windowStartMs    = now;
        _callCountInWindow = 0;
    }

    if (_callCountInWindow >= _maxCallsPerMinute) {
        QString reason = QString(
            "工具调用过于频繁（%1次/分钟），请稍后再试。"
        ).arg(_maxCallsPerMinute);
        qWarning() << "[SafetyGuard] Rate limit hit:" << _callCountInWindow;
        return SafetyResult(SafetyResult::Level::Block, reason, "rate_limit");
    }

    return SafetyResult();
}

// ============================================================
// 重置频率计数器
// ============================================================
void SafetyGuard::resetRateCounter()
{
    _callCountInWindow = 0;
    _windowStartMs     = QDateTime::currentMSecsSinceEpoch();
}
