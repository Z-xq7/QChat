#ifndef INCOMINGCALLDIALOG_H
#define INCOMINGCALLDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
//#include <QMediaPlayer>
#include "userdata.h"

class IncomingCallDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IncomingCallDialog(QWidget *parent = nullptr);
    ~IncomingCallDialog();
    
    void LoadHeadIcon(QString avatarPath, QLabel* icon_label, QString file_name, QString req_type);
    void setCallerInfo(const QString &callerName, const QString &callerAvatar, int callerUid);

public slots:
    void onAcceptCall();
    void onRejectCall();
    void onTimeout();
    void startTimeout(int seconds);

private:
    void setupUI();
    void setupConnections();

private:
    QLabel *m_avatarLabel;
    QLabel *m_nameLabel;
    QLabel *m_statusLabel;
    QPushButton *m_acceptBtn;
    QPushButton *m_rejectBtn;
    
    QString m_callerName;
    QString m_callerAvatar;
    int m_callerUid;
    int m_timeoutSeconds;
    QTimer *m_timeoutTimer;
    
    // 模拟音频播放
    // QMediaPlayer *m_ringtonePlayer;
};

#endif // INCOMINGCALLDIALOG_H
