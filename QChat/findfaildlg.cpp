#include "findfaildlg.h"
#include "ui_findfaildlg.h"
#include <QDebug>
#include <QMouseEvent>

FindFailDlg::FindFailDlg(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FindFailDlg)
{
    ui->setupUi(this);

    setWindowTitle("添加");
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    this->setObjectName("FindFailDlg");
    ui->fail_sure_btn->SetState("normal","hover","press");
    qApp->installEventFilter(this);
    this->setModal(false);
}

FindFailDlg::~FindFailDlg()
{
    qDebug() << "FindFailDlg destruct !";
    delete ui;
}

bool FindFailDlg::eventFilter(QObject *watched, QEvent *event)
{
    if(event->type() == QEvent::MouseButtonPress){
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        handleGlobalMousePress(mouseEvent);
    }

    return QDialog::eventFilter(watched,event);
}

void FindFailDlg::handleGlobalMousePress(QMouseEvent *event)
{
    // 将鼠标点击位置转换为搜索列表坐标系中的位置
    QPoint posInSearchList = mapFromGlobal(event->globalPos());
    // 如果点击的是对话框外部区域，则关闭对话框
    if (!rect().contains(posInSearchList)) {
        // 检查是否点击在当前对话框区域内
        this->close(); // 使用 close() 而不是 hide() 可以确保析构函数被调用
    }
}

void FindFailDlg::on_fail_sure_btn_clicked()
{
    this->hide();
}
