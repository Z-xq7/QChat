#include "aipage.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>
#include <QTextDocument>
#include "global.h"

AIPage::AIPage(QWidget *parent) : QWidget(parent),
    _apiKey("a1cc512f-23ca-4b2e-babe-95e3065d74d9"),
    _endpoint("https://ark.cn-beijing.volces.com/api/v3/chat/completions"),
    _modelId("doubao-seed-2-0-mini-260215") 
{
    setObjectName("AIPage");
    setAttribute(Qt::WA_StyledBackground);
    _networkMgr = new QNetworkAccessManager(this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    QLabel* title = new QLabel("豆包 AI 助手", this);
    title->setStyleSheet("font-size: 20px; color: #333; font-weight: bold;");
    layout->addWidget(title);

    _chatDisplay = new QTextBrowser(this);
    _chatDisplay->setStyleSheet("QTextBrowser { background-color: rgba(255,255,255,100); border: none; border-radius: 15px; padding: 15px; font-size: 14px; color: #333; }");
    _chatDisplay->setOpenExternalLinks(true);
    layout->addWidget(_chatDisplay, 1);

    // 底部输入区域布局优化
    QWidget* inputContainer = new QWidget(this);
    inputContainer->setObjectName("ai_input_container");
    inputContainer->setStyleSheet("#ai_input_container { background-color: white; border: 1px solid #ddd; border-radius: 20px; padding: 5px; }");
    
    QHBoxLayout* inputLayout = new QHBoxLayout(inputContainer);
    inputLayout->setContentsMargins(10, 5, 10, 5);
    inputLayout->setSpacing(10);

    _inputEdit = new QPlainTextEdit(inputContainer);
    _inputEdit->setPlaceholderText("发消息或输入“/”选择技能...");
    _inputEdit->setFixedHeight(70); // 调整高度
    _inputEdit->setStyleSheet("QPlainTextEdit { border: none; background: transparent; font-size: 14px; padding: 5px; color: #333; }");
    
    _sendBtn = new QPushButton(inputContainer);
    _sendBtn->setFixedSize(32, 32); // 改为圆形小图标
    _sendBtn->setCursor(Qt::PointingHandCursor);
    _sendBtn->setStyleSheet("QPushButton { border-image: url(:/images/send.png); background-color: #00bfff; border-radius: 16px; }"
                            "QPushButton:hover { background-color: #009acd; }"
                            "QPushButton:disabled { background-color: #f5f5f5; opacity: 0.5; }");

    inputLayout->addWidget(_inputEdit, 1);
    inputLayout->addWidget(_sendBtn);

    // 将输入容器居中显示，设置最大宽度
    QHBoxLayout* bottomCenterLayout = new QHBoxLayout();
    bottomCenterLayout->addStretch();
    bottomCenterLayout->addWidget(inputContainer);
    inputContainer->setFixedWidth(600); // 稍微加宽一点
    bottomCenterLayout->addStretch();
    
    layout->addLayout(bottomCenterLayout);
    layout->addSpacing(10); // 底部留点空白，更美观

    connect(_sendBtn, &QPushButton::clicked, this, &AIPage::onSendMessage);
}

void AIPage::SetChatData(std::shared_ptr<ChatThreadData> chat_data) {
    _chat_data = chat_data;
    updateDisplayFromData();
}

void AIPage::updateDisplayFromData() {
    _chatDisplay->clear();
    _messages = QJsonArray();

    // 初始化系统提示
    QJsonObject systemMsg;
    systemMsg["role"] = "system";
    systemMsg["content"] = "你是一个智能助手，名叫豆包。你会尽力回答用户的问题。";
    _messages.append(systemMsg);

    if (!_chat_data) return;

    // 从 ChatThreadData 中恢复历史消息
    auto msgMap = _chat_data->GetMsgMap();
    for (auto it = msgMap.begin(); it != msgMap.end(); ++it) {
        auto msg = it.value();
        QString role = (msg->GetSendUid() == 0) ? "assistant" : "user";
        appendMessage(role, msg->GetContent());

        // 构造 API 格式的历史记录
        QJsonObject apiMsg;
        apiMsg["role"] = role;
        apiMsg["content"] = msg->GetContent();
        _messages.append(apiMsg);
    }
}

void AIPage::showThinking(bool show) {
    if (show) {
        // 使用 Table 布局确保对齐和气泡感
        QString thinkingHtml = QString(
            "<table width='100%' border='0' cellspacing='0' cellpadding='0' style='margin-bottom: 15px;'>"
            "<tr><td align='left'>"
            "<div style='color: #888; font-size: 12px; margin-bottom: 4px;'>豆包</div>"
            "<table bgcolor='#f5f5f5' style='border-radius: 5px;'> "
            "<tr><td style='padding: 10px 14px; color: #666;'><i>正在思考...</i></td></tr>"
            "</table>"
            "</td></tr>"
            "</table>"
        );
        
        _chatDisplay->append(thinkingHtml);
        
        // 记录当前的光标位置，以便后续删除
        _thinkingCursor = _chatDisplay->textCursor();
        _thinkingCursor.movePosition(QTextCursor::End);
        _thinkingCursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
        // 向前选择直到包含整个表格块
        while (!_thinkingCursor.atStart() && !_thinkingCursor.selectedText().contains("豆包")) {
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

void AIPage::onSendMessage() {
    QString text = _inputEdit->toPlainText().trimmed();
    if (text.isEmpty() || !_chat_data) return;

    _inputEdit->clear();
    _sendBtn->setEnabled(false);

    // 显示并存储用户消息
    appendMessage("user", text);
    
    // 存储到 ChatThreadData
    int msgId = _chat_data->GetMsgMap().size() + 1;
    auto userMsgData = std::make_shared<TextChatData>(msgId, _chat_data->GetThreadId(), 
                                                      ChatFormType::PRIVATE, ChatMsgType::TEXT, 
                                                      text, 1, 1); 
    _chat_data->AddMsg(userMsgData);
    emit sig_update_last_msg(_chat_data->GetThreadId(), text);

    // 显示思考状态
    showThinking(true);

    // 更新 API 对话历史
    QJsonObject userApiMsg;
    userApiMsg["role"] = "user";
    userApiMsg["content"] = text;
    _messages.append(userApiMsg);

    // 准备请求
    QUrl url(_endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + _apiKey.toUtf8());

    QJsonObject body;
    body["model"] = _modelId;
    body["messages"] = _messages;

    QJsonDocument doc(body);
    QNetworkReply* reply = _networkMgr->post(request, doc.toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
}

void AIPage::onReplyFinished(QNetworkReply* reply) {
    reply->deleteLater();
    _sendBtn->setEnabled(true);
    
    // 隐藏思考状态
    showThinking(false);

    if (reply->error() != QNetworkReply::NoError) {
        appendMessage("system", "错误: " + reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    if (obj.contains("choices")) {
        QJsonArray choices = obj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject firstChoice = choices[0].toObject();
            QJsonObject messageObj = firstChoice["message"].toObject();
            QString content = messageObj["content"].toString();

            appendMessage("assistant", content);

            // 存储到 ChatThreadData
            int msgId = _chat_data->GetMsgMap().size() + 1;
            auto aiMsgData = std::make_shared<TextChatData>(msgId, _chat_data->GetThreadId(), 
                                                             ChatFormType::PRIVATE, ChatMsgType::TEXT, 
                                                             content, 0, 1); // uid 0 为 AI
            _chat_data->AddMsg(aiMsgData);
            emit sig_update_last_msg(_chat_data->GetThreadId(), content);

            // 加入历史记录
            _messages.append(messageObj);
        }
    }
}

QString AIPage::markdownToHtml(const QString& markdown) {
    QTextDocument doc;
    doc.setMarkdown(markdown);
    // 提取 body 内部的内容
    QString html = doc.toHtml();
    int start = html.indexOf("<body");
    if (start != -1) {
        start = html.indexOf(">", start) + 1;
        int end = html.lastIndexOf("</body>");
        if (end != -1) {
            return html.mid(start, end - start).trimmed();
        }
    }
    return markdown.toHtmlEscaped().replace("\n", "<br/>");
}

void AIPage::appendMessage(const QString& role, const QString& content) {
    QString sender = "豆包";
    QString bgColor = "#f5f5f5"; // 使用十六进制颜色，确保兼容性，比纯白略暗
    QString align = "left";

    if (role == "user") {
        sender = "您";
        bgColor = "#e1f5fe"; // 浅蓝色气泡
        align = "right";
    } else if (role == "system") {
        sender = "系统";
        bgColor = "#ffebee";
    }

    // 处理 Markdown 内容
    QString formattedContent = (role == "assistant") ? markdownToHtml(content) : content.toHtmlEscaped().replace("\n", "<br/>");

    // 使用 Table 布局是 Qt RichText 处理对齐和背景色最可靠的方式
    QString html = QString(
        "<table width='100%' border='0' cellspacing='0' cellpadding='0' style='margin-bottom: 15px;'>"
        "<tr><td align='%1'>"
        "<div style='color: #888; font-size: 12px; margin-bottom: 4px;'>%2</div>"
        "<table bgcolor='%3' style='border-radius: 5px;'> "
        "<tr><td style='padding: 10px 14px; color: #333; line-height: 1.4;'>"
        "%4"
        "</td></tr>"
        "</table>"
        "</td></tr>"
        "</table>"
    ).arg(align)
     .arg(sender)
     .arg(bgColor)
     .arg(formattedContent);

    _chatDisplay->append(html);
    _chatDisplay->verticalScrollBar()->setValue(_chatDisplay->verticalScrollBar()->maximum());
}
