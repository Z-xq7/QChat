#ifndef FILECONFIRMDLG_H
#define FILECONFIRMDLG_H

#include <QDialog>
#include "global.h"

namespace Ui {
class FileConfirmDlg;
}

class FileConfirmDlg : public QDialog
{
    Q_OBJECT

public:
    explicit FileConfirmDlg(QWidget *parent = nullptr);
    ~FileConfirmDlg();

    void setFileInfo(const QString& fileName, qint64 fileSize);
    void setTargetName(const QString& name);

private slots:
    void on_send_btn_clicked();
    void on_close_btn_clicked();

private:
    Ui::FileConfirmDlg *ui;
    QString m_fileName;
    qint64 m_fileSize;
};

#endif // FILECONFIRMDLG_H
