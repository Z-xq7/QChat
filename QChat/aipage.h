#ifndef AIPAGE_H
#define AIPAGE_H

#include <QWidget>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>
#include <QGroupBox>
#include <QListWidget>
#include <QCheckBox>
#include <QToolButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressBar>
#include "userdata.h"
#include "voiceinputmanager.h"
#include "toolregistry.h"
#include "contextmanager.h"
#include "executiontracer.h"
#include "safetyguard.h"
#include "agentmemory.h"
#include "paralleltoolexecutor.h"
#include "knowledgebase.h"
#include "ragretriever.h"

// Agent 执行状态
enum class AgentState {
    Idle,       // 空闲，可接受新输入
    Thinking,   // 等待 LLM API 响应
    Executing,  // 正在执行工具调用
    Responding  // 收到最终文本回复，正在渲染
};

// ============================================================
// AIPage — QChat 智能 Agent 主界面
//
// 架构升级（P0）：
//   - 工具执行：通过 ToolRegistry 单例分发，不再持有 AgentTools 指针
//   - 上下文管理：通过 ContextManager 管理 messages，自动 Token 预算裁剪
//   - 信号观测：连接 ToolRegistry 的 sig_tool_before/after_execute 更新 UI
// ============================================================
class AIPage : public QWidget {
    Q_OBJECT
public:
    explicit AIPage(QWidget *parent = nullptr);

    // 设置当前 AI 会话数据
    void SetChatData(std::shared_ptr<ChatThreadData> chat_data);
    
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;  // 事件过滤器，用于处理回车键发送

signals:
    void sig_update_last_msg(int thread_id, const QString& msg);

private slots:
    void onSendMessage();
    void onAgentReplyFinished(QNetworkReply* reply);

    // ToolRegistry 信号槽（P0 新增）
    void onToolBeforeExecute(const QString& toolName, const QJsonObject& args);
    void onToolAfterExecute(const QString& toolName, const QString& result);

    // ContextManager 信号槽（P0 新增）
    void onContextCompacted(int droppedCount, int savedTokens);
    void onContextNearLimit(int usedTokens, int budget);

    // ExecutionTracer 信号槽（P1 新增）
    void onTraceStepStarted(int stepIndex, const QString& toolName, const QJsonObject& args);
    void onTraceStepFinished(int stepIndex, bool success, const QString& resultOrError);
    void onTraceSessionEnded(int totalSteps, int totalElapsedMs);

    // SafetyGuard 信号槽（P1 新增）
    void onSafetyEvent(const QString& category, const QString& reason,
                       SafetyResult::Level level);

    // ParallelToolExecutor 信号槽（P2 新增）
    void onParallelToolStarted(const QString& toolCallId,
                                const QString& toolName,
                                int index);
    void onParallelToolFinished(const QString& toolCallId,
                                 const QString& toolName,
                                 bool success,
                                 int elapsedMs,
                                 int index);
    void onParallelAllFinished(const QVector<ToolCallResult>& results,
                                int totalElapsedMs);

private:
    // ---- UI 控件 ----
    QTextBrowser*    _chatDisplay;
    QPlainTextEdit*  _inputEdit;
    QPushButton*     _sendBtn;
    QLabel*          _tokenLabel;   // Token 用量提示（P0 新增）
    
    // ---- P3: 知识库管理面板 ----
    QPushButton*     _kbToggleBtn;      // 展开/折叠知识库按钮
    QWidget*         _kbPanel;          // 知识库面板（可折叠）
    QCheckBox*       _kbEnableCheck;    // 启用知识库开关
    QListWidget*     _kbDocList;        // 文档列表
    QPushButton*     _kbAddFileBtn;     // 添加文件按钮
    QPushButton*     _kbAddDirBtn;      // 添加文件夹按钮
    QPushButton*     _kbRemoveBtn;      // 删除选中按钮
    QPushButton*     _kbRefreshBtn;     // 刷新索引按钮
    QLabel*          _kbStatusLabel;    // 状态标签
    bool             _kbPanelVisible = false;  // 面板是否可见

    // ---- P4: 语音输入 ----
    VoiceInputManager* _voiceManager;   // 语音输入管理器
    QPushButton*     _voiceBtn;         // 语音输入按钮
    QLabel*          _voiceStatusLabel; // 语音状态显示
    bool             _voiceBtnPressed;  // 按钮是否被按下（等待连接时用）
    QWidget*         _voicePanel;       // 语音录制面板
    QProgressBar*    _voiceVolumeBar;   // 音量指示器
    QString          _voicePreviousText; // 语音识别前的输入框内容
    QString          _voiceAccumulatedText; // 累积的识别文本（中间结果）

    // ---- 网络 ----
    QNetworkAccessManager* _networkMgr;

    // ---- API 配置 ----
    QString _apiKey;
    QString _endpoint;
    QString _modelId;

    // ---- 会话数据 ----
    std::shared_ptr<ChatThreadData> _chat_data;

    // ---- P0: 上下文管理器（替代裸 _messages 数组）----
    ContextManager* _ctxMgr;

    // ---- P1: 执行链追踪器 ----
    ExecutionTracer* _tracer;

    // ---- P1: 安全护栏 ----
    SafetyGuard* _guard;

    // ---- P2: 长期记忆和并行执行 ----
    AgentMemory* _memory;
    ParallelToolExecutor* _parallelExecutor;

    // ---- P3: 知识库和 RAG ----
    // 项目内嵌知识库（静态，随软件发布）
    KnowledgeBase* _projectKnowledgeBase;
    RagRetriever* _projectRagRetriever;
    bool _projectKnowledgeBaseReady = false;
    
    // 用户知识库（动态，用户自行添加）
    KnowledgeBase* _knowledgeBase;
    RagRetriever* _ragRetriever;
    bool _enableRag = true;  // 是否启用用户知识库检索

    // ---- Agent 状态 ----
    AgentState _agentState;
    int _toolCallRounds;
    static const int MAX_TOOL_CALL_ROUNDS = 5;

    // ---- thinking 气泡游标 ----
    QTextCursor _thinkingCursor;

    // ---- 私有方法 ----
    void registerTools();                               // P0: 注册所有工具到 ToolRegistry

    void callAgentAPI();
    void handleAgentResponse(QNetworkReply* reply);
    void executeToolCalls(const QJsonArray& toolCalls);

    void appendMessage(const QString& role, const QString& content);
    void appendToolStatus(const QString& toolName, const QString& statusText, bool isDone);
    void appendSafetyWarning(const QString& reason);    // P1: 显示安全警告气泡
    void updateTracerPanel();                           // P1: 刷新执行链面板
    void updateDisplayFromData();
    void showThinking(bool show);
    void setInputEnabled(bool enabled);
    void updateTokenLabel();                            // P0: 刷新 Token 用量标签
    QString markdownToHtml(const QString& markdown);
    QString buildSystemPrompt();
    
    // P3: 项目知识库初始化
    void initProjectKnowledgeBase();
    QString retrieveProjectContext(const QString& query);
    
    // P3: 用户知识库管理
    void initKnowledgeBasePanel();              // 初始化知识库面板
    void updateKnowledgeBaseList();             // 更新文档列表显示
    void onKbTogglePanel();                     // 展开/折叠知识库面板
    void onKbAddFile();                         // 添加文件
    void onKbAddDirectory();                    // 添加文件夹
    void onKbRemoveSelected();                  // 删除选中文档
    void onKbRefreshIndex();                    // 刷新索引
    void onKbEnableToggled(bool enabled);       // 启用/禁用开关

    // P4: 语音输入
    void initVoiceInput();                      // 初始化语音输入
    void onVoiceButtonPressed();                // 按下语音按钮
    void onVoiceButtonReleased();               // 松开语音按钮
    void onVoiceRecordingStarted();             // 开始录音
    void onVoiceRecordingStopped();             // 停止录音
    void onVoiceReadyChanged(bool ready);       // WebSocket 就绪状态变化
    void onVoiceVolumeChanged(int level);       // 音量变化
    void onVoiceResult(const QString& text, bool isFinal);  // 识别结果
    void onVoiceError(const QString& error);    // 识别错误
};

#endif // AIPAGE_H
