#ifndef FILEBUBBLE_H
#define FILEBUBBLE_H

#include "bubbleframe.h"
#include <QLabel>
#include <QProgressBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "global.h"
#include "ClickedLabel.h"

class FileBubble : public BubbleFrame
{
    Q_OBJECT
public:
    FileBubble(ChatRole role, const QString& fileName, qint64 fileSize, QWidget* parent = nullptr);

    void setProgress(int value, int total_value);
    void setFailed();
    void setMsgInfo(std::shared_ptr<MsgInfo> msg);
    void setDownloadFinish(std::shared_ptr<MsgInfo> msg, QString file_path);
    QString formatSize(qint64 size);

protected:
    void mousePressEvent(QMouseEvent *event) override;

signals:
    void pauseRequested(std::shared_ptr<MsgInfo> msg_info);
    void resumeRequested(std::shared_ptr<MsgInfo> msg_info);

private slots:
    void onFileClicked();

private:
    QLabel* m_iconLabel;
    QLabel* m_nameLabel;
    QLabel* m_sizeLabel;
    QProgressBar* m_progressBar;
    std::shared_ptr<MsgInfo> _msg_info;
};

#endif // FILEBUBBLE_H
