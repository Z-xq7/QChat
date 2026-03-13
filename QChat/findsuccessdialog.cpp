#include "findsuccessdialog.h"
#include "ui_findsuccessdialog.h"
#include <QDir>
#include <QMouseEvent>
#include <QStandardPaths>
#include "usermgr.h"
#include "filetcpmgr.h"

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

    // 使用正则表达式检查是否是默认头像
    QRegularExpression regex("^:/images/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(si->_icon);

    if (match.hasMatch()) {
        // 如果是默认头像（:/images/head_X.jpg 格式）
        QPixmap pixmap(si->_icon); // 加载默认头像图片
        QPixmap scaledPixmap = pixmap.scaled(ui->head_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->head_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
        ui->head_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
    }
    else {
        // 如果是用户上传的头像，获取存储目录
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        auto other_uid = si->_uid;
        QDir avatarsDir(storageDir + "/user/" + QString::number(other_uid) + "/avatars");

        auto file_name = QFileInfo(si->_icon).fileName();

        // 确保目录存在
        if (avatarsDir.exists()) {
            QString avatarPath = avatarsDir.filePath(file_name); // 获取上传头像的完整路径
            QPixmap pixmap(avatarPath); // 加载上传的头像图片
            if (!pixmap.isNull()) {
                QPixmap scaledPixmap = pixmap.scaled(ui->head_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                ui->head_lb->setPixmap(scaledPixmap);
                ui->head_lb->setScaledContents(true);
            }
            else {
                qWarning() << "[ApplyFriendItem]: 无法加载上传的头像：" << avatarPath;
                LoadHeadIcon(avatarPath, ui->head_lb, file_name,"other_icon");
            }
        }
        else {
            qWarning() << "[ApplyFriendItem]: 头像存储目录不存在：" << avatarsDir.path();
            QString avatarPath = avatarsDir.filePath(QFileInfo(si->_icon).fileName());
            avatarsDir.mkpath(".");
            LoadHeadIcon(avatarPath, ui->head_lb, file_name, "other_icon");
        }
    }

    _si = si;
}

void FindSuccessDialog::LoadHeadIcon(QString avatarPath, QLabel *icon_label, QString file_name, QString req_type)
{
    UserMgr::GetInstance()->AddLabelToReset(avatarPath, icon_label);
    //先加载默认的
    QPixmap pixmap(":/images/head_default.png");
    QPixmap scaledPixmap = pixmap.scaled(icon_label->size(),
                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
    icon_label->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
    icon_label->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

    //判断是否正在下载
    bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
    if (is_loading) {
        qWarning() << "[ApplyFriendItem]: 正在下载: " << file_name;
    }
    else {
        //发送请求获取资源
        auto download_info = std::make_shared<DownloadInfo>();
        download_info->_name = file_name;
        download_info->_current_size = 0;
        download_info->_seq = 1;
        download_info->_total_size = 0;
        download_info->_client_path = avatarPath;
        //添加文件到管理者
        UserMgr::GetInstance()->AddDownloadFile(file_name, download_info);
        //发送消息
        FileTcpMgr::GetInstance()->SendDownloadInfo(download_info, req_type);
    }
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
