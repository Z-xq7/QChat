#include "aipage.h"
#include "usermgr.h"
#include "agenttools_impl.h"   // 具体工具实现（P0 迁移后的新文件）
#include <QUrl>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>
#include <QTextDocument>
#include <QKeyEvent>
#include <QDebug>
#include <QTimer>
#include <QStandardPaths>
#include "global.h"
// ============================================================
// 构造函数
// ============================================================
AIPage::AIPage(QWidget *parent) : QWidget(parent),
    _apiKey("a1cc512f-23ca-4b2e-babe-95e3065d74d9"),
    _endpoint("https://ark.cn-beijing.volces.com/api/v3/chat/completions"),
    _modelId("doubao-seed-2-0-mini-260215"),
    _agentState(AgentState::Idle),
    _toolCallRounds(0)
{
    setObjectName("AIPage");
    setAttribute(Qt::WA_StyledBackground);

    _networkMgr = new QNetworkAccessManager(this);

    // ---- P0: 初始化 ContextManager ----
    _ctxMgr = new ContextManager(this);
    _ctxMgr->setTokenBudget(6000);
    _ctxMgr->setKeepRecentCount(20);

    connect(_ctxMgr, &ContextManager::sig_context_compacted,
            this, &AIPage::onContextCompacted);
    connect(_ctxMgr, &ContextManager::sig_context_near_limit,
            this, &AIPage::onContextNearLimit);

    // ---- P1: 初始化 ExecutionTracer ----
    _tracer = new ExecutionTracer(this);
    connect(_tracer, &ExecutionTracer::sig_step_started,
            this, &AIPage::onTraceStepStarted);
    connect(_tracer, &ExecutionTracer::sig_step_finished,
            this, &AIPage::onTraceStepFinished);
    connect(_tracer, &ExecutionTracer::sig_session_ended,
            this, &AIPage::onTraceSessionEnded);

    // ---- P1: 初始化 SafetyGuard ----
    _guard = new SafetyGuard(this);
    _guard->setMaxCallsPerMinute(20);
    connect(_guard, &SafetyGuard::sig_safety_event,
            this, &AIPage::onSafetyEvent);

    // ---- P2: 初始化 AgentMemory ----
    _memory = new AgentMemory(this);
    _memory->setMaxMemories(500);
    _memory->setAutoExtract(true);

    // ---- P3: 初始化项目内嵌知识库（静态）----
    _projectKnowledgeBase = new KnowledgeBase(this);
    _projectKnowledgeBase->setStoragePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/kb_project");
    _projectRagRetriever = new RagRetriever(_projectKnowledgeBase, this);
    _projectRagRetriever->setMinScore(0.4f);  // 项目知识库使用稍低的阈值，确保能召回
    initProjectKnowledgeBase();  // 加载内置文档
    
    // ---- P3: 初始化用户知识库（动态）----
    _knowledgeBase = new KnowledgeBase(this);
    _knowledgeBase->setStoragePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/kb_user");
    _ragRetriever = new RagRetriever(_knowledgeBase, this);
    _ragRetriever->setMinScore(0.5f);

    // ---- P4: 初始化语音输入 ----
    _voiceManager = new VoiceInputManager(this);
    _voiceBtnPressed = false;  // 初始化按钮状态
    connect(_voiceManager, &VoiceInputManager::recordingStarted,
            this, &AIPage::onVoiceRecordingStarted);
    connect(_voiceManager, &VoiceInputManager::recordingStopped,
            this, &AIPage::onVoiceRecordingStopped);
    connect(_voiceManager, &VoiceInputManager::readyChanged,
            this, &AIPage::onVoiceReadyChanged);
    connect(_voiceManager, &VoiceInputManager::volumeChanged,
            this, &AIPage::onVoiceVolumeChanged);
    connect(_voiceManager, &VoiceInputManager::recognitionResult,
            this, &AIPage::onVoiceResult);
    connect(_voiceManager, &VoiceInputManager::recognitionError,
            this, &AIPage::onVoiceError);

    // ---- P2: 初始化 ParallelToolExecutor ----
    _parallelExecutor = new ParallelToolExecutor(this);
    _parallelExecutor->setMaxConcurrency(4);
    connect(_parallelExecutor, &ParallelToolExecutor::sig_tool_started,
            this, &AIPage::onParallelToolStarted);
    connect(_parallelExecutor, &ParallelToolExecutor::sig_tool_finished,
            this, &AIPage::onParallelToolFinished);
    connect(_parallelExecutor, &ParallelToolExecutor::sig_all_finished,
            this, &AIPage::onParallelAllFinished);

    // ---- P0: 注册工具到 ToolRegistry，并连接信号 ----
    registerTools();

    connect(&ToolRegistry::instance(), &ToolRegistry::sig_tool_before_execute,
            this, &AIPage::onToolBeforeExecute);
    connect(&ToolRegistry::instance(), &ToolRegistry::sig_tool_after_execute,
            this, &AIPage::onToolAfterExecute);

    // ---- 布局 ----
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    // 标题栏（含 Token 指示标签）
    QHBoxLayout* titleBar = new QHBoxLayout();
    QLabel* title = new QLabel("QChat 智能助手", this);
    title->setStyleSheet("font-size: 20px; color: #333; font-weight: bold;");

    _tokenLabel = new QLabel("", this);
    _tokenLabel->setStyleSheet("font-size: 11px; color: #999;");
    _tokenLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    titleBar->addWidget(title);
    titleBar->addStretch();
    titleBar->addWidget(_tokenLabel);
    layout->addLayout(titleBar);

    _chatDisplay = new QTextBrowser(this);
    _chatDisplay->setStyleSheet(
        "QTextBrowser { background-color: rgba(255,255,255,100); border: none; "
        "border-radius: 15px; padding: 15px; font-size: 14px; color: #333; }"
    );
    _chatDisplay->setOpenExternalLinks(true);
    layout->addWidget(_chatDisplay, 1);

    // ---- P3: 知识库管理面板（初始隐藏）----
    initKnowledgeBasePanel();
    _kbPanel->setVisible(false);
    layout->addWidget(_kbPanel);

    // 底部输入区（包含知识库切换按钮 + 输入框）
    QWidget* bottomContainer = new QWidget(this);
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomContainer);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(5);

    // 功能按钮栏（知识库 + 语音）
    QHBoxLayout* toolBtnLayout = new QHBoxLayout();
    toolBtnLayout->addStretch();
    
    // 知识库切换按钮
    _kbToggleBtn = new QPushButton("📚 知识库", this);
    _kbToggleBtn->setCheckable(true);
    _kbToggleBtn->setChecked(false);
    _kbToggleBtn->setFixedHeight(28);
    _kbToggleBtn->setStyleSheet(
        "QPushButton { background-color: transparent; border: 1px solid #ddd; "
        "border-radius: 4px; padding: 4px 12px; font-size: 12px; color: #666; }"
        "QPushButton:hover { background-color: #f5f5f5; border-color: #ccc; }"
        "QPushButton:checked { background-color: #e3f2fd; border-color: #2196F3; color: #1976D2; }"
    );
    connect(_kbToggleBtn, &QPushButton::toggled, this, &AIPage::onKbTogglePanel);
    toolBtnLayout->addWidget(_kbToggleBtn);
    
    // 语音输入按钮
    _voiceBtn = new QPushButton("🎤 语音", this);
    _voiceBtn->setFixedHeight(28);
    _voiceBtn->setStyleSheet(
        "QPushButton { background-color: transparent; border: 1px solid #ddd; "
        "border-radius: 4px; padding: 4px 12px; font-size: 12px; color: #666; }"
        "QPushButton:hover { background-color: #f5f5f5; border-color: #ccc; }"
        "QPushButton:pressed { background-color: #e3f2fd; border-color: #2196F3; color: #1976D2; }"
    );
    // 注意：不要启用 AutoRepeat，否则按住时会反复触发 pressed 信号
    _voiceBtn->setAutoRepeat(false);
    connect(_voiceBtn, &QPushButton::pressed, this, &AIPage::onVoiceButtonPressed);
    connect(_voiceBtn, &QPushButton::released, this, &AIPage::onVoiceButtonReleased);
    toolBtnLayout->addWidget(_voiceBtn);
    
    bottomLayout->addLayout(toolBtnLayout);

    // 输入框容器
    QWidget* inputContainer = new QWidget(this);
    inputContainer->setObjectName("ai_input_container");
    inputContainer->setStyleSheet(
        "#ai_input_container { background-color: white; border: 1px solid #ddd; "
        "border-radius: 20px; padding: 5px; }"
    );

    QHBoxLayout* inputLayout = new QHBoxLayout(inputContainer);
    inputLayout->setContentsMargins(10, 5, 10, 5);
    inputLayout->setSpacing(10);

    _inputEdit = new QPlainTextEdit(inputContainer);
    _inputEdit->setPlaceholderText("告诉我你想做什么，例如：给小明发消息：你在干什么？");
    _inputEdit->setFixedHeight(70);
    _inputEdit->setStyleSheet(
        "QPlainTextEdit { border: none; background: transparent; font-size: 14px; padding: 5px; color: #333; }"
    );

    _sendBtn = new QPushButton(inputContainer);
    _sendBtn->setFixedSize(32, 32);
    _sendBtn->setCursor(Qt::PointingHandCursor);
    _sendBtn->setStyleSheet(
        "QPushButton { border-image: url(:/images/send.png); background-color: #00bfff; border-radius: 16px; }"
        "QPushButton:hover { background-color: #009acd; }"
        "QPushButton:disabled { background-color: #ccc; }"
    );

    inputLayout->addWidget(_inputEdit, 1);
    inputLayout->addWidget(_sendBtn);
    bottomLayout->addWidget(inputContainer);

    QHBoxLayout* bottomCenterLayout = new QHBoxLayout();
    bottomCenterLayout->addStretch();
    bottomCenterLayout->addWidget(bottomContainer);
    inputContainer->setFixedWidth(600);
    bottomCenterLayout->addStretch();

    layout->addLayout(bottomCenterLayout);
    layout->addSpacing(10);

    connect(_sendBtn, &QPushButton::clicked, this, &AIPage::onSendMessage);
    
    // 安装事件过滤器以捕获回车键
    _inputEdit->installEventFilter(this);
}

// ============================================================
// P0: 注册所有工具到 ToolRegistry
// 只需在此添加/删除行即可扩展工具，不需要修改其他代码
// ============================================================
void AIPage::registerTools()
{
    auto& reg = ToolRegistry::instance();
    reg.registerTool(std::make_shared<SendMessageTool>());
    reg.registerTool(std::make_shared<CheckOnlineTool>());
    reg.registerTool(std::make_shared<GetFriendInfoTool>());
    reg.registerTool(std::make_shared<ListFriendsTool>());
    reg.registerTool(std::make_shared<GetChatHistoryTool>());

    qDebug() << "[AIPage] registered" << reg.toolCount() << "tools:"
             << reg.toolNames().join(", ");
}

// ============================================================
// 设置会话数据
// ============================================================
void AIPage::SetChatData(std::shared_ptr<ChatThreadData> chat_data)
{
    _chat_data = chat_data;
    updateDisplayFromData();
}

// ============================================================
// 从 ChatThreadData 恢复历史消息到 UI 和 ContextManager
// ============================================================
void AIPage::updateDisplayFromData()
{
    _chatDisplay->clear();

    // P0: 重置 ContextManager，设置新的 system 消息
    _ctxMgr->reset();
    _ctxMgr->setSystemMessage(buildSystemPrompt());

    if (!_chat_data) {
        updateTokenLabel();
        return;
    }

    auto msgMap = _chat_data->GetMsgMap();
    for (auto it = msgMap.begin(); it != msgMap.end(); ++it) {
        auto msg = it.value();
        QString role = (msg->GetSendUid() == 0) ? "assistant" : "user";
        appendMessage(role, msg->GetContent());

        // P0: 通过 ContextManager 添加历史消息（自动管理 Token）
        QJsonObject apiMsg;
        apiMsg["role"]    = role;
        apiMsg["content"] = msg->GetContent();
        _ctxMgr->addMessage(apiMsg);
    }

    updateTokenLabel();
}

// ============================================================
// 事件过滤器：处理回车键发送消息
// ============================================================
bool AIPage::eventFilter(QObject *obj, QEvent *event)
{
    // 只处理输入框的键盘事件
    if (obj == _inputEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        
        // 回车键或小键盘回车键
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Shift+回车 = 换行（不发送）
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                return false;  // 让默认处理换行
            }
            
            // 普通回车 = 发送消息
            onSendMessage();
            return true;  // 事件已处理，不再传递
        }
    }
    
    // 其他事件交给默认处理
    return QWidget::eventFilter(obj, event);
}

// ============================================================
// 用户点击发送
// ============================================================
void AIPage::onSendMessage()
{
    if (_agentState != AgentState::Idle) return;

    QString text = _inputEdit->toPlainText().trimmed();
    if (text.isEmpty() || !_chat_data) return;

    // ---- P1: SafetyGuard 输入检查 ----
    SafetyResult safetyResult = _guard->checkUserInput(text);
    if (safetyResult.blocked()) {
        appendSafetyWarning(safetyResult.reason);
        return;
    }
    if (safetyResult.warned()) {
        appendSafetyWarning(safetyResult.reason);
        // Warn 级别：继续执行但已通知用户
    }

    _inputEdit->clear();
    setInputEnabled(false);
    _agentState = AgentState::Thinking;
    _toolCallRounds = 0;

    // ---- P1: ExecutionTracer 开始新会话 ----
    _tracer->beginSession(text);
    _guard->resetRateCounter();

    appendMessage("user", text);

    // 持久化用户消息
    int msgId = _chat_data->GetMsgMap().size() + 1;
    auto userMsgData = std::make_shared<TextChatData>(
        msgId, _chat_data->GetThreadId(),
        ChatFormType::PRIVATE, ChatMsgType::TEXT,
        text, 1, 1
    );
    _chat_data->AddMsg(userMsgData);
    emit sig_update_last_msg(_chat_data->GetThreadId(), text);

    // P0: 通过 ContextManager 追加用户消息
    QJsonObject userApiMsg;
    userApiMsg["role"]    = "user";
    userApiMsg["content"] = text;
    _ctxMgr->addMessage(userApiMsg);

    updateTokenLabel();
    showThinking(true);
    callAgentAPI();
}

// ============================================================
// 发起一次 API 调用（P0: ContextManager | P2: AgentMemory 增强）
// ============================================================
void AIPage::callAgentAPI()
{
    QUrl url(_endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + _apiKey.toUtf8());

    // ---- P2: 从 AgentMemory 检索相关记忆 ----
    // 使用最近的用户消息作为查询
    QJsonArray contextMessages = _ctxMgr->messages();
    QString queryText;
    for (int i = contextMessages.size() - 1; i >= 0; --i) {
        QJsonObject msg = contextMessages[i].toObject();
        if (msg["role"].toString() == "user") {
            queryText = msg["content"].toString();
            break;
        }
    }

    QJsonArray finalMessages;

    // 构建系统提示词（包含记忆和知识库）
    QString enhancedSystemPrompt = buildSystemPrompt();

    // P2: 添加长期记忆
    if (!queryText.isEmpty()) {
        auto relevantMemories = _memory->retrieveRelevantMemories(queryText, 5, 1000);
        if (!relevantMemories.isEmpty()) {
            QString memoryContext = "## 相关背景记忆\n";
            for (const auto& mem : relevantMemories) {
                memoryContext += "- " + mem.content + "\n";
            }
            enhancedSystemPrompt += "\n\n" + memoryContext;
        }
    }

    // P3: 添加项目知识库检索结果（高优先级，始终启用）
    if (!queryText.isEmpty()) {
        QString projectContext = retrieveProjectContext(queryText);
        if (!projectContext.isEmpty()) {
            enhancedSystemPrompt += "\n\n## 项目知识库参考\n" + projectContext +
                                   "\n\n以上是与用户问题相关的 QChat 项目文档内容。"
                                   "请优先基于这些文档回答关于项目架构、协议、部署等技术问题。";
        }
    }
    
    // P3: 添加用户知识库检索结果（可选）
    qDebug() << "[AIPage] User KB check: _enableRag=" << _enableRag
             << "queryEmpty=" << queryText.isEmpty()
             << "kbChunks=" << (_knowledgeBase ? _knowledgeBase->stats() : "null");
    if (_enableRag && !queryText.isEmpty()) {
        qDebug() << "[AIPage] Retrieving user knowledge base for query:" << queryText;
        
        QString kbContext = _ragRetriever->retrieveContext(queryText, 3, 600);
        
        if (!kbContext.isEmpty()) {
            qDebug() << "[AIPage] User KB context retrieved, length:" << kbContext.length();
            enhancedSystemPrompt += "\n\n## 用户知识库参考\n" + kbContext +
                                   "\n\n以上是与用户问题相关的用户文档内容。"
                                   "如果用户询问的是这些文档中的内容，请基于文档回答。";
        } else {
            qDebug() << "[AIPage] No relevant content found in user knowledge base";
        }
    } else if (!_enableRag) {
        qDebug() << "[AIPage] User knowledge base retrieval disabled";
    }

    // 构建最终的 messages 数组
    QJsonObject enhancedSystem;
    enhancedSystem["role"] = "system";
    enhancedSystem["content"] = enhancedSystemPrompt;
    finalMessages.append(enhancedSystem);

    // 添加历史消息（跳过原有的 system）
    for (const QJsonValue& v : contextMessages) {
        QJsonObject msg = v.toObject();
        if (msg["role"].toString() != "system") {
            finalMessages.append(msg);
        }
    }

    QJsonObject body;
    body["model"]       = _modelId;
    body["messages"]    = finalMessages;
    body["tools"]       = ToolRegistry::instance().buildApiToolsDef();
    body["tool_choice"] = "auto";

    QNetworkReply* reply = _networkMgr->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onAgentReplyFinished(reply);
    });
}

// ============================================================
// API 响应处理入口
// ============================================================
void AIPage::onAgentReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    handleAgentResponse(reply);
}

// ============================================================
// Agent 核心：解析响应，决定调工具还是输出最终结果
// ============================================================
void AIPage::handleAgentResponse(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        showThinking(false);
        appendMessage("system", "网络错误：" + reply->errorString());
        _agentState = AgentState::Idle;
        setInputEnabled(true);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull()) {
        showThinking(false);
        appendMessage("system", "响应解析失败，请重试");
        _agentState = AgentState::Idle;
        setInputEnabled(true);
        return;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("error")) {
        showThinking(false);
        QString errMsg = obj["error"].toObject()["message"].toString();
        appendMessage("system", "API 错误：" + errMsg);
        _agentState = AgentState::Idle;
        setInputEnabled(true);
        return;
    }

    if (!obj.contains("choices") || obj["choices"].toArray().isEmpty()) {
        showThinking(false);
        appendMessage("system", "未收到有效响应");
        _agentState = AgentState::Idle;
        setInputEnabled(true);
        return;
    }

    QJsonObject firstChoice = obj["choices"].toArray()[0].toObject();
    QString     finishReason = firstChoice["finish_reason"].toString();
    QJsonObject messageObj   = firstChoice["message"].toObject();

    // P0: 通过 ContextManager 追加 assistant 消息
    _ctxMgr->addMessage(messageObj);
    updateTokenLabel();

    // ---- 情况 A：LLM 要调用工具 ----
    if (finishReason == "tool_calls" && messageObj.contains("tool_calls")) {
        QJsonArray toolCalls = messageObj["tool_calls"].toArray();

        if (_toolCallRounds >= MAX_TOOL_CALL_ROUNDS) {
            showThinking(false);
            appendMessage("system", "工具调用轮次超限，请重新输入");
            _agentState = AgentState::Idle;
            setInputEnabled(true);
            return;
        }

        _agentState = AgentState::Executing;
        _toolCallRounds++;
        showThinking(false);
        executeToolCalls(toolCalls);

        _agentState = AgentState::Thinking;
        showThinking(true);
        callAgentAPI();
        return;
    }

    // ---- 情况 B：最终文本回复 ----
    showThinking(false);
    _agentState = AgentState::Responding;

    QString content = messageObj["content"].toString();
    if (content.isEmpty()) content = "操作已完成。";

    appendMessage("assistant", content);

    // 持久化 AI 回复
    int msgId = _chat_data->GetMsgMap().size() + 1;
    auto aiMsgData = std::make_shared<TextChatData>(
        msgId, _chat_data->GetThreadId(),
        ChatFormType::PRIVATE, ChatMsgType::TEXT,
        content, 0, 1
    );
    _chat_data->AddMsg(aiMsgData);
    emit sig_update_last_msg(_chat_data->GetThreadId(), content);

    // ---- P2: 从对话中提取并存储记忆 ----
    // 获取用户输入（从 ContextManager 历史中找到最近的用户消息）
    QString lastUserMsg;
    QJsonArray allMsgs = _ctxMgr->messages();
    for (int i = allMsgs.size() - 1; i >= 0; --i) {
        QJsonObject msg = allMsgs[i].toObject();
        if (msg["role"].toString() == "user") {
            lastUserMsg = msg["content"].toString();
            break;
        }
    }
    if (!lastUserMsg.isEmpty()) {
        _memory->extractAndStoreFromDialog(lastUserMsg, content);
    }

    // ---- P1: 结束 Tracer 会话 ----
    _tracer->endSession();

    _agentState = AgentState::Idle;
    setInputEnabled(true);
}

// ============================================================
// 执行工具调用列表（P2: 并行执行 | P1: Tracer + SafetyGuard）
// ============================================================
void AIPage::executeToolCalls(const QJsonArray& toolCalls)
{
    // ---- P1: SafetyGuard 前置检查（过滤被拦截的工具）----
    QJsonArray allowedToolCalls;
    for (const QJsonValue& tcVal : toolCalls) {
        QJsonObject tc      = tcVal.toObject();
        QJsonObject funcObj = tc["function"].toObject();
        QString toolName    = funcObj["name"].toString();
        QJsonObject args    = QJsonDocument::fromJson(
            funcObj["arguments"].toString().toUtf8()
        ).object();

        IAgentTool::RiskLevel risk = ToolRegistry::instance().riskLevel(toolName);
        SafetyResult safetyResult  = _guard->checkToolCall(toolName, args, risk);

        if (safetyResult.blocked()) {
            appendSafetyWarning(safetyResult.reason);

            // 立即构造拒绝结果
            QJsonObject toolResultMsg;
            toolResultMsg["role"]         = "tool";
            toolResultMsg["tool_call_id"] = tc["id"].toString();
            toolResultMsg["content"]      = QString(
                R"({"error":"安全策略拒绝执行此工具：%1"})"
            ).arg(safetyResult.reason);
            _ctxMgr->addMessage(toolResultMsg);
        } else {
            allowedToolCalls.append(tc);
        }
    }

    if (allowedToolCalls.isEmpty()) {
        qDebug() << "[Agent] All tool calls blocked by SafetyGuard";
        return;
    }

    // ---- P2: 使用 ParallelToolExecutor 并行执行 ----
    // 同步等待执行完成（保持原有流程结构）
    QEventLoop loop;
    connect(_parallelExecutor, &ParallelToolExecutor::sig_all_finished,
            &loop, &QEventLoop::quit);
    connect(_parallelExecutor, &ParallelToolExecutor::sig_error,
            &loop, &QEventLoop::quit);

    _parallelExecutor->executeAll(allowedToolCalls);
    loop.exec();  // 等待全部完成

    // 处理结果
    QVector<ToolCallResult> results = _parallelExecutor->lastResults();
    for (const auto& result : results) {
        // 将工具结果追加到 ContextManager
        QJsonObject toolResultMsg;
        toolResultMsg["role"]         = "tool";
        toolResultMsg["tool_call_id"] = result.toolCallId;
        toolResultMsg["content"]      = result.result;
        _ctxMgr->addMessage(toolResultMsg);

        updateTokenLabel();
    }

    qDebug() << "[Agent] Parallel execution finished:"
             << results.size() << "tools in"
             << _parallelExecutor->totalElapsedMs() << "ms";
}

// ============================================================
// P0: ToolRegistry 信号槽 —— 在工具执行前后更新 UI 状态气泡
// ============================================================
void AIPage::onToolBeforeExecute(const QString& toolName, const QJsonObject& /*args*/)
{
    appendToolStatus(toolName, "执行中...", false);
}

void AIPage::onToolAfterExecute(const QString& toolName, const QString& result)
{
    appendToolStatus(toolName, result, true);
}

// ============================================================
// P0: ContextManager 信号槽
// ============================================================
void AIPage::onContextCompacted(int droppedCount, int savedTokens)
{
    qDebug() << "[AIPage] context compacted: dropped" << droppedCount
             << "msgs, saved" << savedTokens << "tokens";
    // 可选：在聊天窗口插入一条系统提示（此处仅打印，不打扰用户）
}

void AIPage::onContextNearLimit(int usedTokens, int budget)
{
    qDebug() << "[AIPage] context near limit:" << usedTokens << "/" << budget;
    // Token 标签颜色变红，提醒用户
    _tokenLabel->setStyleSheet("font-size: 11px; color: #e57373;");
    updateTokenLabel();
}

// ============================================================
// P0: 更新 Token 用量标签
// ============================================================
void AIPage::updateTokenLabel()
{
    int used   = _ctxMgr->estimatedTotalTokens();
    int budget = _ctxMgr->tokenBudget();
    int pct    = budget > 0 ? (used * 100 / budget) : 0;

    _tokenLabel->setText(QString("上下文: ~%1 / %2 tokens (%3%)")
                         .arg(used).arg(budget).arg(pct));

    // 颜色随使用率变化
    if (pct >= 85) {
        _tokenLabel->setStyleSheet("font-size: 11px; color: #e57373;"); // 红
    } else if (pct >= 60) {
        _tokenLabel->setStyleSheet("font-size: 11px; color: #f57c00;"); // 橙
    } else {
        _tokenLabel->setStyleSheet("font-size: 11px; color: #999;");    // 灰
    }
}

// ============================================================
// UI 辅助函数
// ============================================================
void AIPage::showThinking(bool show)
{
    if (show) {
        QString html = QString(
            "<table width='100%' border='0' cellspacing='0' cellpadding='0' style='margin-bottom: 12px;'>"
            "<tr><td align='left'>"
            "<div style='color: #888; font-size: 11px; margin-bottom: 3px;'>AI 助手</div>"
            "<table bgcolor='#fffff0' style='border-radius: 5px;'>"
            "<tr><td style='padding: 7px 7px; color: #999; font-style: italic; font-size: 11px;'>正在思考中...</td></tr>"
            "</table>"
            "</td></tr>"
            "</table>"
        );
        _chatDisplay->append(html);
        _thinkingCursor = _chatDisplay->textCursor();
        _thinkingCursor.movePosition(QTextCursor::End);
        _thinkingCursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
        while (!_thinkingCursor.atStart() && !_thinkingCursor.selectedText().contains("AI 助手")) {
            _thinkingCursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::KeepAnchor);
        }
        _chatDisplay->verticalScrollBar()->setValue(_chatDisplay->verticalScrollBar()->maximum());
    } else {
        if (!_thinkingCursor.isNull()) {
            _thinkingCursor.removeSelectedText();
            _thinkingCursor = QTextCursor();
        }
    }
}

void AIPage::appendToolStatus(const QString& toolName, const QString& statusText, bool isDone)
{
    static const QMap<QString, QString> toolDisplayName = {
        {"send_message",    "发送消息"},
        {"check_online",    "查询在线状态"},
        {"get_friend_info", "获取好友信息"},
        {"list_friends",    "列出所有好友"},
        {"get_chat_history","获取聊天记录"}
    };

    QString displayName = toolDisplayName.value(toolName, toolName);
    QString icon     = isDone ? "✓" : "⏳";
    QString bgColor  = isDone ? "#e8f5e9" : "#fff8e1";
    QString txtColor = isDone ? "#388e3c" : "#f57c00";

    // isDone 时显示简洁的完成标记，不显示原始 JSON
    QString label = isDone
        ? QString("%1 [%2] 完成").arg(icon).arg(displayName)
        : QString("%1 [%2] %3").arg(icon).arg(displayName).arg(statusText);

    QString html = QString(
        "<table width='100%' border='0' cellspacing='0' cellpadding='0' style='margin-bottom: 6px;'>"
        "<tr><td align='left'>"
        "<table bgcolor='%1' style='border-radius: 4px; border-left: 3px solid %2;'>"
        "<tr><td style='padding: 5px 10px; color: %2; font-size: 12px;'>%3</td></tr>"
        "</table>"
        "</td></tr>"
        "</table>"
    ).arg(bgColor).arg(txtColor).arg(label);

    _chatDisplay->append(html);
    _chatDisplay->verticalScrollBar()->setValue(_chatDisplay->verticalScrollBar()->maximum());
}

void AIPage::appendMessage(const QString& role, const QString& content)
{
    QString sender  = "AI 助手";
    QString bgColor = "#ffffff";
    QString align   = "left";

    if (role == "user") {
        sender  = "您";
        bgColor = "#e1f5fe";
        align   = "right";
    } else if (role == "system") {
        sender  = "系统";
        bgColor = "#fffff0";
        align   = "left";
    }

    QString formattedContent = (role == "assistant")
        ? markdownToHtml(content)
        : content.toHtmlEscaped().replace("\n", "<br/>");

    QString html = QString(
        "<table width='100%' border='0' cellspacing='0' cellpadding='0' style='margin-bottom: 12px;'>"
        "<tr><td align='%1'>"
        "<div style='color: #888; font-size: 12px; margin-bottom: 5px;'>%2</div>"
        "<table bgcolor='%3' style='border-radius: 5px;'>"
        "<tr><td style='padding: 10px 8px; color: #333; line-height: 1.4;'>%4</td></tr>"
        "</table>"
        "</td></tr>"
        "</table>"
    ).arg(align).arg(sender).arg(bgColor).arg(formattedContent);

    _chatDisplay->append(html);
    _chatDisplay->verticalScrollBar()->setValue(_chatDisplay->verticalScrollBar()->maximum());
}

void AIPage::setInputEnabled(bool enabled)
{
    _sendBtn->setEnabled(enabled);
    _inputEdit->setEnabled(enabled);
}

QString AIPage::markdownToHtml(const QString& markdown)
{
    QTextDocument doc;
    doc.setMarkdown(markdown);
    QString html = doc.toHtml();
    int start = html.indexOf("<body");
    if (start != -1) {
        start = html.indexOf(">", start) + 1;
        int end = html.lastIndexOf("</body>");
        if (end != -1) return html.mid(start, end - start).trimmed();
    }
    return markdown.toHtmlEscaped().replace("\n", "<br/>");
}

// ============================================================
// P1: ExecutionTracer 信号槽
// ============================================================
void AIPage::onTraceStepStarted(int stepIndex, const QString& toolName, const QJsonObject& /*args*/)
{
    // 步骤开始时，ToolRegistry 的 sig_tool_before_execute 也会触发 appendToolStatus
    // Tracer 这里额外更新执行面板（如果有的话）
    qDebug() << "[AIPage] Trace step" << stepIndex << "started:" << toolName;
}

void AIPage::onTraceStepFinished(int stepIndex, bool success, const QString& /*resultOrError*/)
{
    qDebug() << "[AIPage] Trace step" << stepIndex
             << (success ? "✓ success" : "✗ failed");
    // 执行面板实时刷新
    updateTracerPanel();
}

void AIPage::onTraceSessionEnded(int totalSteps, int totalElapsedMs)
{
    if (totalSteps == 0) return;

    // 在聊天区下方显示一个轻量的执行摘要折叠提示
    QString html = QString(
        "<table width='100%%' border='0' cellspacing='0' cellpadding='0' style='margin-bottom:10px;'>"
        "<tr><td align='left'>"
        "<div style='color:#bdbdbd; font-size:11px; padding:4px 8px;"
        " background:#fafafa; border-radius:4px; border:1px solid #eee;'>"
        "🔧 本次共调用 <b>%1</b> 个工具，总耗时 <b>%2ms</b>"
        "</div>"
        "</td></tr></table>"
    ).arg(totalSteps).arg(totalElapsedMs);

    _chatDisplay->append(html);
    _chatDisplay->verticalScrollBar()->setValue(
        _chatDisplay->verticalScrollBar()->maximum());
}

// ============================================================
// P1: SafetyGuard 信号槽
// ============================================================
void AIPage::onSafetyEvent(const QString& category,
                            const QString& reason,
                            SafetyResult::Level level)
{
    qWarning() << "[AIPage] Safety event [" << category << "]"
               << (level == SafetyResult::Level::Block ? "BLOCK" : "WARN")
               << reason;
    // Block 级别在 onSendMessage / executeToolCalls 里已经调用 appendSafetyWarning
    // Warn 级别在这里额外记录（UI 已由调用处处理）
}

// ============================================================
// P1: 安全警告气泡
// ============================================================
void AIPage::appendSafetyWarning(const QString& reason)
{
    QString html = QString(
        "<table width='100%%' border='0' cellspacing='0' cellpadding='0' style='margin-bottom:10px;'>"
        "<tr><td align='left'>"
        "<table bgcolor='#fff3e0' style='border-radius:5px; border-left:4px solid #ff6f00;'>"
        "<tr><td style='padding:8px 12px;'>"
        "<div style='color:#ff6f00; font-size:12px; font-weight:bold;'>⚠ 安全提示</div>"
        "<div style='color:#795548; font-size:12px; margin-top:3px;'>%1</div>"
        "</td></tr>"
        "</table>"
        "</td></tr></table>"
    ).arg(reason.toHtmlEscaped());

    _chatDisplay->append(html);
    _chatDisplay->verticalScrollBar()->setValue(
        _chatDisplay->verticalScrollBar()->maximum());
}

// ============================================================
// P1: 刷新执行链面板（当前版本：将最新执行链 HTML 追加到对话区）
// 在 P2 阶段可升级为独立侧边面板
// ============================================================
void AIPage::updateTracerPanel()
{
    // 当前实现：由 onTraceSessionEnded 统一渲染摘要，此处仅 debug 输出
    // 若需要实时面板，可在此 append formatFullHtml()
    Q_UNUSED(_tracer);
}

// ============================================================
// P2: ParallelToolExecutor 信号槽
// ============================================================
void AIPage::onParallelToolStarted(const QString& toolCallId,
                                    const QString& toolName,
                                    int /*index*/)
{
    Q_UNUSED(toolCallId)
    // 复用 P1 的 Tracer 开始步骤
    QJsonObject args;  // 参数在并行执行器内部管理
    _tracer->beginStep(toolName, args);

    // UI 状态已在 ToolRegistry 信号中处理
    qDebug() << "[AIPage] Parallel tool started:" << toolName;
}

void AIPage::onParallelToolFinished(const QString& toolCallId,
                                     const QString& toolName,
                                     bool success,
                                     int elapsedMs,
                                     int /*index*/)
{
    Q_UNUSED(toolCallId)
    Q_UNUSED(elapsedMs)

    // 复用 P1 的 Tracer 结束步骤
    if (success) {
        _tracer->endStepSuccess(-1, "并行执行完成");
    } else {
        _tracer->endStepFailed(-1, "执行失败");
    }

    qDebug() << "[AIPage] Parallel tool finished:" << toolName
             << (success ? "✓" : "✗");
}

void AIPage::onParallelAllFinished(const QVector<ToolCallResult>& results,
                                    int totalElapsedMs)
{
    Q_UNUSED(results)
    Q_UNUSED(totalElapsedMs)
    // 结果处理已在 executeToolCalls 中完成
    qDebug() << "[AIPage] All parallel tools finished in" << totalElapsedMs << "ms";
}

// ============================================================
// 系统提示词（P0: 由 ToolRegistry 动态生成工具描述部分）
// ============================================================
QString AIPage::buildSystemPrompt()
{
    QString myName = UserMgr::GetInstance()->GetNick();
    if (myName.isEmpty()) myName = UserMgr::GetInstance()->GetName();

    // P0: 工具描述由 ToolRegistry 动态生成，不再硬编码
    QString toolsDesc = ToolRegistry::instance().buildToolsDescription();

    return QString(
        "你是QChat智能助手，一个集成在IM聊天软件中的AI Agent。\n"
        "当前登录用户的昵称是：%1\n\n"
        "%2\n\n"
        "## 行为准则\n"
        "1. 当用户请求执行操作时，优先调用工具完成，而不是询问是否要执行。\n"
        "2. 如果用户说的好友名字模糊，先用 list_friends 确认，再操作。\n"
        "3. 发送消息前无需询问确认，直接发送并告知结果。\n"
        "4. 查询在线状态后，用自然语言告知（\"XXX 现在在线\" 或 \"XXX 目前不在线\"）。\n"
        "5. 找不到对应好友时，明确告知用户。\n"
        "6. 对于普通问答，直接回答，不需要调用工具。\n\n"
        "## 注意事项\n"
        "- 只能发文本消息，不支持图片/文件\n"
        "- 每次只能给一个好友发消息，不支持群发\n"
        "- 没有权限添加或删除好友\n\n"
        "## 项目信息\n"
        "QChat 是一个基于 C++ 和 Qt 的分布式即时通信系统，采用微服务架构，"
        "包含 GateServer、ChatServer、StatusServer、VarifyServer 等多个服务组件。"
        "客户端使用 Qt6 开发，支持文本/图片/文件消息、视频通话、群聊等功能。"
    ).arg(myName).arg(toolsDesc);
}

// ============================================================
// P3: 项目内嵌知识库初始化
// ============================================================
void AIPage::initProjectKnowledgeBase()
{
    qDebug() << "[AIPage] Initializing project knowledge base...";
    
    // 内置知识库文档列表（从资源文件加载）
    QStringList kbFiles = {
        ":/resources/knowledge_base/01_overview.md",
        ":/resources/knowledge_base/02_architecture.md",
        ":/resources/knowledge_base/03_protocol.md",
        ":/resources/knowledge_base/04_database.md",
        ":/resources/knowledge_base/05_deployment.md",
        ":/resources/knowledge_base/06_coding_standards.md"
    };
    
    int successCount = 0;
    for (const QString& filePath : kbFiles) {
        qDebug() << "[AIPage] Trying to load:" << filePath;
        
        // 先检查资源文件是否存在
        QFile testFile(filePath);
        if (!testFile.exists()) {
            qWarning() << "[AIPage] Resource file not found:" << filePath;
            continue;
        }
        qDebug() << "[AIPage] Resource file exists:" << filePath;
        
        if (_projectKnowledgeBase->addDocument(filePath)) {
            qDebug() << "[AIPage] Loaded project KB:" << filePath;
            successCount++;
        } else {
            qWarning() << "[AIPage] Failed to load project KB:" << filePath;
        }
    }
    
    _projectKnowledgeBaseReady = (successCount > 0);
    qDebug() << "[AIPage] Project knowledge base ready:" << _projectKnowledgeBaseReady 
             << "(" << successCount << "/" << kbFiles.size() << " files)";
}

QString AIPage::retrieveProjectContext(const QString& query)
{
    if (!_projectKnowledgeBaseReady) {
        return QString();
    }
    
    // 从项目知识库检索相关上下文
    QString context = _projectRagRetriever->retrieveContext(query, 3, 800);
    
    if (!context.isEmpty()) {
        qDebug() << "[AIPage] Retrieved project context for query:" << query.left(50);
    }
    
    return context;
}

// ============================================================
// P3: 用户知识库管理面板
// ============================================================

// 辅助函数：格式化文件大小
static QString formatFileSize(qint64 size)
{
    if (size < 1024) return QString("%1 B").arg(size);
    if (size < 1024 * 1024) return QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
}

void AIPage::initKnowledgeBasePanel()
{
    // 从持久化设置加载 RAG 开关状态
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "QChat", "AIPage");
    _enableRag = settings.value("kbEnable", true).toBool();
    qDebug() << "[AIPage] Loaded kbEnable from settings:" << _enableRag;

    _kbPanel = new QWidget(this);
    _kbPanel->setStyleSheet(
        "QWidget { background-color: #fafafa; border: 1px solid #e0e0e0; border-radius: 8px; }"
    );
    _kbPanel->setFixedHeight(220);

    QVBoxLayout* panelLayout = new QVBoxLayout(_kbPanel);
    panelLayout->setSpacing(8);
    panelLayout->setContentsMargins(12, 12, 12, 12);

    // 标题栏
    QHBoxLayout* titleLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel("📚 知识库管理", this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; border: none; background: transparent;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    
    _kbEnableCheck = new QCheckBox("启用检索", this);
    _kbEnableCheck->setChecked(_enableRag);
    _kbEnableCheck->setToolTip("开启后，AI 会优先从知识库中检索相关信息");
    // 复选框样式：使用自定义图标
    _kbEnableCheck->setStyleSheet(
        "QCheckBox { "
        "  font-size: 12px; color: #333; border: none; background: transparent;"
        "  padding: 2px 4px; spacing: 6px;"
        "} "
        "QCheckBox::indicator { "
        "  width: 23px; height: 23px; border: none; background: transparent; "
        "  image: url(:/images/check_mark.png);"
        "} "
        "QCheckBox::indicator:hover { "
        "  image: url(:/images/check_mark_hover.png);"
        "} "
        "QCheckBox::indicator:checked { "
        "  image: url(:/images/check_mark_press.png);"
        "} "
        "QCheckBox::indicator:checked:hover { "
        "  image: url(:/images/check_mark_press.png);"
        "}"
    );
    connect(_kbEnableCheck, &QCheckBox::toggled, this, &AIPage::onKbEnableToggled);
    titleLayout->addWidget(_kbEnableCheck);
    panelLayout->addLayout(titleLayout);

    // 操作按钮栏
    QHBoxLayout* btnLayout = new QHBoxLayout();
    _kbAddFileBtn = new QPushButton("➕ 添加文件", this);
    _kbAddFileBtn->setFixedHeight(26);
    _kbAddFileBtn->setStyleSheet("font-size: 11px; padding: 2px 8px;");
    connect(_kbAddFileBtn, &QPushButton::clicked, this, &AIPage::onKbAddFile);
    btnLayout->addWidget(_kbAddFileBtn);

    _kbAddDirBtn = new QPushButton("📁 添加文件夹", this);
    _kbAddDirBtn->setFixedHeight(26);
    _kbAddDirBtn->setStyleSheet("font-size: 11px; padding: 2px 8px;");
    connect(_kbAddDirBtn, &QPushButton::clicked, this, &AIPage::onKbAddDirectory);
    btnLayout->addWidget(_kbAddDirBtn);

    _kbRemoveBtn = new QPushButton("🗑️ 删除", this);
    _kbRemoveBtn->setFixedHeight(26);
    _kbRemoveBtn->setStyleSheet("font-size: 11px; padding: 2px 8px;");
    connect(_kbRemoveBtn, &QPushButton::clicked, this, &AIPage::onKbRemoveSelected);
    btnLayout->addWidget(_kbRemoveBtn);

    _kbRefreshBtn = new QPushButton("🔄 刷新", this);
    _kbRefreshBtn->setFixedHeight(26);
    _kbRefreshBtn->setStyleSheet("font-size: 11px; padding: 2px 8px;");
    connect(_kbRefreshBtn, &QPushButton::clicked, this, &AIPage::onKbRefreshIndex);
    btnLayout->addWidget(_kbRefreshBtn);
    btnLayout->addStretch();
    panelLayout->addLayout(btnLayout);

    // 文档列表
    _kbDocList = new QListWidget(this);
    _kbDocList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _kbDocList->setStyleSheet(
        "QListWidget { border: 1px solid #e0e0e0; border-radius: 4px; background: white; }"
        "QListWidget::item { padding: 4px; border-bottom: 1px solid #f5f5f5; font-size: 12px; }"
        "QListWidget::item:selected { background-color: #e3f2fd; color: #333; }"
    );
    panelLayout->addWidget(_kbDocList, 1);

    // 状态标签
    _kbStatusLabel = new QLabel("就绪 - 共 0 个文档", this);
    _kbStatusLabel->setStyleSheet("color: #999; font-size: 11px; border: none; background: transparent;");
    panelLayout->addWidget(_kbStatusLabel);

    // 初始更新列表
    updateKnowledgeBaseList();
}

void AIPage::onKbTogglePanel()
{
    _kbPanelVisible = !_kbPanelVisible;
    _kbPanel->setVisible(_kbPanelVisible);
    
    if (_kbPanelVisible) {
        updateKnowledgeBaseList();  // 展开时刷新列表
    }
}

void AIPage::updateKnowledgeBaseList()
{
    _kbDocList->clear();

    if (!_knowledgeBase) {
        _kbStatusLabel->setText("知识库未初始化");
        return;
    }

    QVector<Document> docs = _knowledgeBase->listDocuments();
    for (const auto& doc : docs) {
        QString itemText = QString("📄 %1 (%2 片段, %3)")
            .arg(doc.fileName)
            .arg(doc.chunkCount)
            .arg(formatFileSize(doc.fileSize));
        QListWidgetItem* item = new QListWidgetItem(itemText, _kbDocList);
        item->setData(Qt::UserRole, doc.id);  // 存储文档ID
        item->setToolTip(doc.filePath);
    }

    _kbStatusLabel->setText(QString("就绪 - %1").arg(_knowledgeBase->stats()));
}

void AIPage::onKbAddFile()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        "选择要添加到知识库的文件",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "文本文档 (*.txt *.md);;所有文件 (*.*)");

    if (filePath.isEmpty()) return;

    if (_knowledgeBase->addDocument(filePath)) {
        QMessageBox::information(this, "成功", "文档已添加到知识库");
        updateKnowledgeBaseList();
    } else {
        QMessageBox::warning(this, "失败", "无法添加文档，请检查文件格式或内容");
    }
}

void AIPage::onKbAddDirectory()
{
    QString dirPath = QFileDialog::getExistingDirectory(this,
        "选择要添加到知识库的文件夹",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));

    if (dirPath.isEmpty()) return;

    int added = _knowledgeBase->addDirectory(dirPath);
    if (added > 0) {
        QMessageBox::information(this, "成功",
            QString("已添加 %1 个文档到知识库").arg(added));
        updateKnowledgeBaseList();
    } else {
        QMessageBox::information(this, "提示", "文件夹中没有可添加的文本文档");
    }
}

void AIPage::onKbRemoveSelected()
{
    QList<QListWidgetItem*> selected = _kbDocList->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要删除的文档");
        return;
    }

    int reply = QMessageBox::question(this, "确认删除",
        QString("确定要删除选中的 %1 个文档吗？").arg(selected.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    for (auto* item : selected) {
        QString docId = item->data(Qt::UserRole).toString();
        _knowledgeBase->removeDocument(docId);
    }

    updateKnowledgeBaseList();
}

void AIPage::onKbRefreshIndex()
{
    _kbStatusLabel->setText("正在刷新索引...");
    _kbRefreshBtn->setEnabled(false);

    // 使用 QTimer 延迟执行，让 UI 有机会更新
    QTimer::singleShot(100, this, [this]() {
        _knowledgeBase->syncFiles();  // 同步文件变更
        updateKnowledgeBaseList();
        _kbRefreshBtn->setEnabled(true);
    });
}

void AIPage::onKbEnableToggled(bool enabled)
{
    _enableRag = enabled;
    qDebug() << "[AIPage] Knowledge base retrieval" << (enabled ? "enabled" : "disabled");
    
    // 持久化设置
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "QChat", "AIPage");
    settings.setValue("kbEnable", enabled);
}

// ============================================================
// P4: 语音输入
// ============================================================
void AIPage::onVoiceButtonPressed()
{
    // 防止重复触发（如果已经在录音中，忽略）
    if (_voiceManager->isRecording()) {
        return;
    }
    
    _voiceBtnPressed = true;
    // 预连接 WebSocket（如果还没连接）
    _voiceManager->prepare();
    // 如果已经连接，立即开始录音；否则等待 readyChanged 信号
    if (_voiceManager->isReady()) {
        _voiceManager->startRecording();
    } else {
        _inputEdit->setPlaceholderText("按住等待连接...");
    }
}

void AIPage::onVoiceReadyChanged(bool ready)
{
    if (ready && _voiceBtnPressed && !_voiceManager->isRecording()) {
        // WebSocket 已连接且按钮正被按住，开始录音
        qDebug() << "[AIPage] WebSocket ready, starting recording...";
        _voiceManager->startRecording();
    } else if (!ready) {
        _inputEdit->setPlaceholderText("语音识别服务已断开");
    }
}

void AIPage::onVoiceButtonReleased()
{
    _voiceBtnPressed = false;
    if (_voiceManager->isRecording()) {
        _voiceManager->stopRecording();
    }
}

void AIPage::onVoiceRecordingStarted()
{
    _voiceBtn->setText("🎙️ 聆听中...");
    _voiceBtn->setStyleSheet(
        "QPushButton { background-color: #e3f2fd; border: 1px solid #2196F3; "
        "border-radius: 4px; padding: 4px 12px; font-size: 12px; color: #1976D2; }"
    );
    _inputEdit->setPlaceholderText("正在聆听，请说话...");
    
    // 保存当前输入框内容，用于后续追加语音识别结果
    _voicePreviousText = _inputEdit->toPlainText();
    _voiceAccumulatedText.clear();
}

void AIPage::onVoiceRecordingStopped()
{
    _voiceBtn->setText("🎤 语音");
    _voiceBtn->setStyleSheet(
        "QPushButton { background-color: transparent; border: 1px solid #ddd; "
        "border-radius: 4px; padding: 4px 12px; font-size: 12px; color: #666; }"
        "QPushButton:hover { background-color: #f5f5f5; border-color: #ccc; }"
        "QPushButton:pressed { background-color: #e3f2fd; border-color: #2196F3; color: #1976D2; }"
    );
    _inputEdit->setPlaceholderText("告诉我你想做什么，例如：给小明发消息：你在干什么？");
}

void AIPage::onVoiceVolumeChanged(int level)
{
    // 可以在这里更新音量动画
    Q_UNUSED(level)
}

void AIPage::onVoiceResult(const QString& text, bool isFinal)
{
    // 讯飞 API 说明：
    // - 中间结果（isFinal=false）：返回当前已识别的完整文本
    // - 最终结果（isFinal=true）：只返回最后一片文本（可能只有标点）
    // 因此需要累积中间结果，最终结果时合并
    
    if (isFinal) {
        // 最终结果：使用累积的文本 + 最后一片文本
        QString finalText = _voiceAccumulatedText;
        
        // 检查最后一片文本是否已经在累积文本中（避免重复）
        if (!text.isEmpty() && !finalText.endsWith(text)) {
            finalText += text;
        }
        
        // 合并到原始输入框内容
        QString resultText = _voicePreviousText;
        if (!resultText.isEmpty() && !resultText.endsWith(" ") && !finalText.isEmpty()) {
            resultText += " ";
        }
        resultText += finalText;
        
        _inputEdit->setPlainText(resultText);
        _inputEdit->moveCursor(QTextCursor::End);
        
        // 清空临时状态
        _voicePreviousText.clear();
        _voiceAccumulatedText.clear();
    } else {
        // 中间结果：更新累积文本并实时显示
        // 注意：中间结果是完整的当前识别文本，不是增量
        _voiceAccumulatedText = text;
        _inputEdit->setPlainText(_voicePreviousText + text);
        _inputEdit->moveCursor(QTextCursor::End);
    }
}

void AIPage::onVoiceError(const QString& error)
{
    qWarning() << "[AIPage] Voice recognition error:" << error;
    _inputEdit->setPlaceholderText("语音识别失败: " + error);
    
    // 发生错误时停止录音
    if (_voiceManager->isRecording()) {
        _voiceManager->stopRecording();
    }
    _voiceBtnPressed = false;
    onVoiceRecordingStopped();  // 恢复按钮状态
    
    // 3秒后恢复默认提示
    QTimer::singleShot(3000, this, [this]() {
        _inputEdit->setPlaceholderText("告诉我你想做什么，例如：给小明发消息：你在干什么？");
    });
}
