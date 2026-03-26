#ifndef USERINFODIALOG_H
#define USERINFODIALOG_H

#include <QFrame>
#include "clickedlabel.h"
#include <QLabel>
#include <QPushButton>

class UserInfoDialog : public QFrame
{
    Q_OBJECT
public:
    explicit UserInfoDialog(QWidget *parent = nullptr);
    ~UserInfoDialog();

    void LoadHeadIcon(QString avatarPath, QLabel* icon_label, QString file_name, QString req_type);

signals:
    void sig_closed();

protected:
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void initUI();
    
    QLabel* _head_lb;
    QLabel* _name_lb;
    QLabel* _nick_lb;
    QLabel* _uid_lb;
    QLabel* _sex_lb;
    QLabel* _desc_lb;
    
    QPushButton* _edit_btn;
    QPushButton* _msg_btn;
};

#endif // USERINFODIALOG_H
