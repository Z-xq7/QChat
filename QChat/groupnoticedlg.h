#ifndef GROUPNOTICEDLG_H
#define GROUPNOTICEDLG_H

#include <QDialog>

// 群公告查看弹窗（只读模式）
class GroupNoticeViewDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GroupNoticeViewDialog(const QString& notice, QWidget *parent = nullptr);
};

#endif // GROUPNOTICEDLG_H
