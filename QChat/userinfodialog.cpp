#include "userinfodialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDebug>
#include "usermgr.h"

#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QMouseEvent>
#include <QStandardPaths>
#include "filetcpmgr.h"

UserInfoDialog::UserInfoDialog(QWidget *parent) : QFrame(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    initUI();
}

UserInfoDialog::~UserInfoDialog()
{
    qDebug() << "[UserInfoDialog]: destroyed";
}

void UserInfoDialog::LoadHeadIcon(QString avatarPath, QLabel *icon_label, QString file_name, QString req_type)
{
    UserMgr::GetInstance()->AddLabelToReset(avatarPath, _head_lb);
    //先加载默认的
    QPixmap pixmap(":/images/head_default.png");
    QPixmap scaledPixmap = pixmap.scaled(_head_lb->size(),
                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
    _head_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
    _head_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

    //判断是否正在下载
    bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
    if (is_loading) {
        qWarning() << "[UserInfoPage]: 正在下载: " << file_name;
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

void UserInfoDialog::initUI()
{
    // 主容器改为使用布局绘制背景
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(25, 25, 25, 25);
    main_layout->setSpacing(15);

    // 第一行：头像、昵称、UID、点赞
    QHBoxLayout* top_layout = new QHBoxLayout();
    _head_lb = new QLabel(this);
    _head_lb->setFixedSize(60, 60);
    _head_lb->setScaledContents(true);
    
    // 加载头像逻辑
    auto user_info = UserMgr::GetInstance();
    QString icon = user_info->GetIcon();
    qDebug() << "[UserInfoDialog]: icon is " << icon ;
    //使用正则表达式检查是否使用默认头像
    QRegularExpression regex("^:/images/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(icon);
    if (match.hasMatch()) {
        QPixmap pixmap(icon);
        QPixmap scaledPixmap = pixmap.scaled(_head_lb->size(),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
        _head_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
        _head_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
    }
    else {
        // 如果是用户上传的头像，获取存储目录
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        auto uid = UserMgr::GetInstance()->GetUid();
        QDir avatarsDir(storageDir + "/user/" + QString::number(uid) + "/avatars");

        // 确保目录存在
        if (avatarsDir.exists()) {
            auto file_name = QFileInfo(icon).fileName();
            QString avatarPath = avatarsDir.filePath(file_name); // 获取上传头像的完整路径
            QPixmap pixmap(avatarPath); // 加载上传的头像图片
            if (!pixmap.isNull()) {
                //判断是否正在下载
                bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
                if (is_loading) {
                    qWarning() << "[UserInfoDialog]: 正在下载: " << file_name;
                    //先加载默认的
                    QPixmap pixmap(":/images/head_default.png");
                    QPixmap scaledPixmap = pixmap.scaled(_head_lb->size(),
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                    _head_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                    _head_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

                }
                else {
                    QPixmap scaledPixmap = pixmap.scaled(_head_lb->size(),
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                    _head_lb->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                    _head_lb->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
                }
            }
            else {
                qWarning() << "[UserInfoDialog]: 无法加载上传的头像：" << avatarPath;
                LoadHeadIcon(avatarPath, _head_lb, file_name, "self_icon");
            }
        }
        else {
            qWarning() << "[UserInfoDialog]: 头像存储目录不存在：" << avatarsDir.path();
            QString avatarPath = avatarsDir.filePath(QFileInfo(icon).fileName());
            avatarsDir.mkpath(".");
            LoadHeadIcon(avatarPath, _head_lb, icon, "self_icon");
        }
    }

    QVBoxLayout* name_uid_layout = new QVBoxLayout();
    _nick_lb = new QLabel(user_info->GetNick(), this);
    _nick_lb->setStyleSheet("font-size: 18px; font-weight: bold; color: #333; background: transparent;");
    _uid_lb = new QLabel("QChat " + QString::number(user_info->GetUid()), this);
    _uid_lb->setStyleSheet("font-size: 14px; color: #888; background: transparent;");
    _desc_lb = new QLabel(user_info->GetDesc().isEmpty() ? "这个人很懒，什么都没写" : user_info->GetDesc(), this);
    _desc_lb->setStyleSheet("font-size: 14px; color: #555; background: transparent;");
    
    name_uid_layout->addWidget(_nick_lb);
    name_uid_layout->addWidget(_uid_lb);
    name_uid_layout->addWidget(_desc_lb);

    top_layout->addWidget(_head_lb);
    top_layout->addLayout(name_uid_layout);
    top_layout->addStretch();
    
    QLabel* like_lb = new QLabel("👍 9999+", this);
    like_lb->setStyleSheet("color: #888; background: transparent;");
    top_layout->addWidget(like_lb);

    main_layout->addLayout(top_layout);

    // 中间信息部分 (等级、特权、所在地等)
    QGridLayout* info_grid = new QGridLayout();
    info_grid->setSpacing(10);
    
    auto add_info_row = [&](int row, const QString& label, const QString& value) {
        QLabel* key_lb = new QLabel(label, this);
        key_lb->setStyleSheet("color: #888; width: 60px; background: transparent;");
        QLabel* val_lb = new QLabel(value, this);
        val_lb->setStyleSheet("color: #333; background: transparent;");
        info_grid->addWidget(key_lb, row, 0);
        info_grid->addWidget(val_lb, row, 1);
    };

    add_info_row(0, "等级", "☀️☀️☀️🌙🌙⭐⭐");
    add_info_row(1, "特权", "💎👑🚀🔥");
    add_info_row(2, "所在地", "山东·济宁");

    main_layout->addLayout(info_grid);

    // 底部按钮
    QHBoxLayout* btn_layout = new QHBoxLayout();
    _edit_btn = new QPushButton("编辑资料", this);
    _edit_btn->setFixedSize(115, 35);
    _edit_btn->setStyleSheet("QPushButton { background-color: #f0f0f0; border: none; border-radius: 15px; color: #333; font-weight: bold; }"
                             "QPushButton:hover { background-color: #e5e5e5; }");
    
    _msg_btn = new QPushButton("发消息", this);
    _msg_btn->setFixedSize(115, 35);
    _msg_btn->setStyleSheet("QPushButton { background-color: #00bfff; border: none; border-radius: 15px; color: white; font-weight: bold; }"
                            "QPushButton:hover { background-color: #009acd; }");

    btn_layout->addWidget(_edit_btn);
    btn_layout->addWidget(_msg_btn);
    main_layout->addLayout(btn_layout);

    this->adjustSize();
}

void UserInfoDialog::hideEvent(QHideEvent *event)
{
    emit sig_closed();
    QFrame::hideEvent(event);
}

void UserInfoDialog::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    qApp->installEventFilter(this);
}

bool UserInfoDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        const QPoint globalPos = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
        const QPoint localPos = mapFromGlobal(globalPos);
        if (!rect().contains(localPos)) {
            close();
            return false;
        }
    }
    return QFrame::eventFilter(obj, event);
}

void UserInfoDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRect r = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 12, 12);

    // 纯白背景
    p.fillPath(path, Qt::white);

    // 绘制灰色边框
    QPen pen(QColor(225, 225, 225, 255));
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.drawPath(path);
}
