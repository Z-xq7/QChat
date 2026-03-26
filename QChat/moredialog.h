#ifndef MOREDIALOG_H
#define MOREDIALOG_H

#include <QFrame>
#include <QVBoxLayout>
#include "clickedlabel.h"

class MoreDialog : public QFrame
{
    Q_OBJECT
public:
    explicit MoreDialog(QWidget *parent = nullptr);
    ~MoreDialog();

signals:
    void switch_login(); // 用于触发退出账号逻辑
    void sig_closed(); // 当窗口关闭或隐藏时触发

protected:
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void initUI();
    
    ClickedLabel* _help_lb;
    ClickedLabel* _lock_lb;
    ClickedLabel* _settings_lb;
    ClickedLabel* _logout_lb;
};

#endif // MOREDIALOG_H
