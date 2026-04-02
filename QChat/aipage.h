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
#include "userdata.h"

class AIPage : public QWidget {
    Q_OBJECT
public:
    explicit AIPage(QWidget *parent = nullptr);

    // 设置当前 AI 会话数据
    void SetChatData(std::shared_ptr<ChatThreadData> chat_data);

signals:
    void sig_update_last_msg(int thread_id, const QString& msg);

private slots:
    void onSendMessage();
    void onReplyFinished(QNetworkReply* reply);

private:
    QTextBrowser* _chatDisplay;
    QPlainTextEdit* _inputEdit;
    QPushButton* _sendBtn;
    QNetworkAccessManager* _networkMgr;
    QString _apiKey;
    QString _endpoint;
    QString _modelId;
    
    std::shared_ptr<ChatThreadData> _chat_data;
    QJsonArray _messages; // 对话历史 (用于 API 请求)

    void appendMessage(const QString& role, const QString& content);
    void updateDisplayFromData();
    void showThinking(bool show);
    QString markdownToHtml(const QString& markdown);

    QTextCursor _thinkingCursor;
};

#endif // AIPAGE_H
