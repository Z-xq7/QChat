#include "fileconfirmdlg.h"
#include "ui_fileconfirmdlg.h"
#include <QFileInfo>
#include <QIcon>
#include <QFileIconProvider>

FileConfirmDlg::FileConfirmDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FileConfirmDlg)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    ui->close_lb->SetState("normal", "hover", "press", "normal", "hover", "press");
    connect(ui->close_lb, &ClickedLabel::clicked, this, &FileConfirmDlg::on_close_btn_clicked);
    
    // 初始化发送按钮状态
    ui->send_btn->SetState("normal", "hover", "press");
}

FileConfirmDlg::~FileConfirmDlg()
{
    delete ui;
}

void FileConfirmDlg::setFileInfo(const QString &fileName, qint64 fileSize)
{
    m_fileName = fileName;
    m_fileSize = fileSize;

    QFileInfo fileInfo(fileName);
    ui->file_name_lb->setText(fileInfo.fileName());

    QString sizeStr;
    if (fileSize < 1024) {
        sizeStr = QString::number(fileSize) + " B";
    } else if (fileSize < 1024 * 1024) {
        sizeStr = QString::number(fileSize / 1024.0, 'f', 1) + " KB";
    } else {
        sizeStr = QString::number(fileSize / (1024.0 * 1024.0), 'f', 1) + " MB";
    }
    ui->file_size_lb->setText(sizeStr);

    // 设置图标
    QFileIconProvider provider;
    QIcon icon = provider.icon(fileInfo);
    ui->file_icon_lb->setPixmap(icon.pixmap(60, 60));
}

void FileConfirmDlg::setTargetName(const QString &name)
{
    ui->title_lb->setText("发送给 " + name);
}

void FileConfirmDlg::on_send_btn_clicked()
{
    accept();
}

void FileConfirmDlg::on_close_btn_clicked()
{
    reject();
}
