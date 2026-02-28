#include "findsuccessdialog.h"
#include "ui_findsuccessdialog.h"
#include <QDir>
#include <QMouseEvent>

FindSuccessDialog::FindSuccessDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FindSuccessDialog),_parent(parent)
{
    ui->setupUi(this);

    // 设置对话框标题
    setWindowTitle("添加");
    // 隐藏对话框标题栏
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    //设置模态对话框，使其可以捕获所有时间，包括外部点击
    setModal(false);
    qApp->installEventFilter(this);

    ui->add_friend_btn->SetState("normal","hover","press");
}

FindSuccessDialog::~FindSuccessDialog()
{
    delete ui;
}

void FindSuccessDialog::SetSearchInfo(std::shared_ptr<SearchInfo> si)
{
    //设置名字
    ui->name_lb->setText(si->_name);

    // 设置头像
    if (!si->_icon.isEmpty()) {
        // 如果有头像路径，则使用它
        QPixmap head_pix(si->_icon);
        if (!head_pix.isNull()) {
            // 如果图片加载成功，设置图片
            head_pix = head_pix.scaled(ui->head_lb->size(),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
            ui->head_lb->setPixmap(head_pix);
        } else {
            // 如果图片加载失败，使用默认头像
            // 获取当前应用程序的路径
            QString app_path = QCoreApplication::applicationDirPath();
            QString pix_path = QDir::toNativeSeparators(app_path +
                QDir::separator() + "static"+QDir::separator()+"head_1.jpg");
            QPixmap head_pix(pix_path);
            head_pix = head_pix.scaled(ui->head_lb->size(),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
            ui->head_lb->setPixmap(head_pix);
        }
    } else {
        // 如果没有头像路径，使用默认头像
        // 获取当前应用程序的路径
        QString app_path = QCoreApplication::applicationDirPath();
        QString pix_path = QDir::toNativeSeparators(app_path +
            QDir::separator() + "static"+QDir::separator()+"head_1.jpg");
        QPixmap head_pix(pix_path);
        head_pix = head_pix.scaled(ui->head_lb->size(),
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->head_lb->setPixmap(head_pix);
    }

    _si = si;
}

void FindSuccessDialog::on_add_friend_btn_clicked()
{
    //todo... 添加好友界面弹出
    this->hide();
    auto applyFriend = new ApplyFriend(_parent);
    applyFriend->SetSearchInfo(_si);
    applyFriend->setModal(true);
    applyFriend->show();
}

bool FindSuccessDialog::eventFilter(QObject *watched, QEvent *event)
{
    if(event->type() == QEvent::MouseButtonPress){
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        handleGlobalMousePress(mouseEvent);
    }

    return QDialog::eventFilter(watched,event);
}

void FindSuccessDialog::handleGlobalMousePress(QMouseEvent *event)
{
    // 将鼠标点击位置转换为搜索列表坐标系中的位置
    QPoint posInSearchList = mapFromGlobal(event->globalPos());
    // 如果点击的是对话框外部区域，则关闭对话框
    if (!rect().contains(posInSearchList)) {
        // 检查是否点击在当前对话框区域内
        this->close(); // 使用 close() 而不是 hide() 可以确保析构函数被调用
    }
}
