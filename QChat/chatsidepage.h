#ifndef CHATSIDEPAGE_H
#define CHATSIDEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTimer>
#include "clickedlabel.h"

class ChatSidePage : public QWidget
{
    Q_OBJECT
public:
    explicit ChatSidePage(QWidget *parent = nullptr);

signals:
    void sig_search_text_changed(const QString& text);
    void sig_clear_history();
    void sig_delete_friend();

private slots:
    void on_search_timeout();

protected:
    // 让ChatSidePage具备样式表渲染能力
    void paintEvent(QPaintEvent *event) override;

private:
    void initUi();
    QWidget* createSwitchItem(const QString& text, bool checked);
    QPushButton* createCardButton(const QString& text);

    QLineEdit* m_searchEdit;
    QTimer* m_searchTimer;
};

#endif // CHATSIDEPAGE_H
