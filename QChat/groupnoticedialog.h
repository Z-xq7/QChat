#ifndef GROUPNOTICEDIALOG_H
#define GROUPNOTICEDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

class GroupNoticeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GroupNoticeDialog(const QString& current_notice, QWidget *parent = nullptr);
    QString GetNotice() const;

private slots:
    void on_save_clicked();
    void on_cancel_clicked();

private:
    QTextEdit* m_noticeEdit;
    QPushButton* m_saveBtn;
    QPushButton* m_cancelBtn;
};

#endif // GROUPNOTICEDIALOG_H
