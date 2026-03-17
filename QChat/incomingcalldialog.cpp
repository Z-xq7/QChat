#include "incomingcalldialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QTimer>
#include <QDebug>
#include "videocallmanager.h"
#include <QStandardPaths>
#include "usermgr.h"
#include "filetcpmgr.h"

IncomingCallDialog::IncomingCallDialog(QWidget *parent)
    : QDialog(parent)
    , m_avatarLabel(nullptr)
    , m_nameLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_acceptBtn(nullptr)
    , m_rejectBtn(nullptr)
    , m_callerUid(0)
    , m_timeoutSeconds(30)  // 30秒超时
    , m_timeoutTimer(nullptr)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    
    setupUI();
    setupConnections();
    
    // 创建超时定时器
    m_timeoutTimer = new QTimer(this);
    connect(m_timeoutTimer, &QTimer::timeout, this, &IncomingCallDialog::onTimeout);
}

IncomingCallDialog::~IncomingCallDialog()
{
}

void IncomingCallDialog::LoadHeadIcon(QString avatarPath, QLabel *icon_label, QString file_name, QString req_type)
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
        qWarning() << "[IncomingCallDialog]: 正在下载: " << file_name;
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

void IncomingCallDialog::setupUI()
{
    setStyleSheet("background-color: #2C2C2C; border: 2px solid #4CAF50; border-radius: 10px;");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);
    
    // 头像和姓名
    QHBoxLayout *topLayout = new QHBoxLayout();
    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(80, 80);
    m_avatarLabel->setStyleSheet("QLabel { border-radius: 40px; background-color: #444; }");
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setText("头像");
    
    QVBoxLayout *nameLayout = new QVBoxLayout();
    m_nameLabel = new QLabel();
    m_nameLabel->setStyleSheet("QLabel { color: white; font-size: 18px; font-weight: bold; }");
    m_nameLabel->setText("未知联系人");
    m_nameLabel->setAlignment(Qt::AlignLeft);
    
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("QLabel { color: #AAA; font-size: 14px; }");
    m_statusLabel->setText("正在呼叫您...");
    m_statusLabel->setAlignment(Qt::AlignLeft);
    
    nameLayout->addWidget(m_nameLabel);
    nameLayout->addWidget(m_statusLabel);
    nameLayout->addStretch();
    
    topLayout->addWidget(m_avatarLabel);
    topLayout->addSpacing(15);
    topLayout->addLayout(nameLayout);
    topLayout->addStretch();
    
    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    m_rejectBtn = new QPushButton();
    m_rejectBtn->setFixedSize(60, 60);
    m_rejectBtn->setStyleSheet(
        "QPushButton { "
        "   border: none; "
        "   border-radius: 30px; "
        "   background-color: #F44336; "
        "   color: white; "
        "   font-size: 24px; "
        "   font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "   background-color: #D32F2F; "
        "} "
        "QPushButton:pressed { "
        "   background-color: #B71C1C; "
        "}"
    );
    m_rejectBtn->setText("拒绝");
    
    m_acceptBtn = new QPushButton();
    m_acceptBtn->setFixedSize(60, 60);
    m_acceptBtn->setStyleSheet(
        "QPushButton { "
        "   border: none; "
        "   border-radius: 30px; "
        "   background-color: #4CAF50; "
        "   color: white; "
        "   font-size: 24px; "
        "   font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "   background-color: #388E3C; "
        "} "
        "QPushButton:pressed { "
        "   background-color: #1B5E20; "
        "}"
    );
    m_acceptBtn->setText("接听");
    
    btnLayout->addWidget(m_rejectBtn);
    btnLayout->addSpacing(20);
    btnLayout->addWidget(m_acceptBtn);
    btnLayout->addStretch();
    
    mainLayout->addLayout(topLayout);
    mainLayout->addStretch();
    mainLayout->addLayout(btnLayout);
}

void IncomingCallDialog::setupConnections()
{
    connect(m_acceptBtn, &QPushButton::clicked, this, &IncomingCallDialog::onAcceptCall);
    connect(m_rejectBtn, &QPushButton::clicked, this, &IncomingCallDialog::onRejectCall);
}

void IncomingCallDialog::setCallerInfo(const QString &callerName, const QString &callerAvatar, int callerUid)
{
    m_callerName = callerName;
    m_callerAvatar = callerAvatar;
    m_callerUid = callerUid;
    
    m_nameLabel->setText(callerName);

    //使用正则表达式检查是否使用默认头像
    QRegularExpression regex("^:/images/head_(\\d+)\\.jpg$");
    QRegularExpressionMatch match = regex.match(m_callerAvatar);
    if (match.hasMatch()) {
        QPixmap pixmap(m_callerAvatar);
        QPixmap scaledPixmap = pixmap.scaled(m_avatarLabel->size(),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
        m_avatarLabel->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
        m_avatarLabel->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
    }
    else if(m_callerAvatar == ""){
        //先加载默认的
        QPixmap pixmap(":/images/head_default.png");
        QPixmap scaledPixmap = pixmap.scaled(m_avatarLabel->size(),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
        m_avatarLabel->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
        m_avatarLabel->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
    }
    else {
        // 如果是用户上传的头像，获取存储目录
        QString storageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

        QDir avatarsDir(storageDir + "/user/" + QString::number(m_callerUid) + "/avatars");
        auto file_name = QFileInfo(m_callerAvatar).fileName();

        // 确保目录存在
        if (avatarsDir.exists()) {
            QString avatarPath = avatarsDir.filePath(file_name); // 获取上传头像的完整路径
            QPixmap pixmap(avatarPath); // 加载上传的头像图片
            if (!pixmap.isNull()) {
                //判断是否正在下载
                bool is_loading = UserMgr::GetInstance()->IsDownLoading(file_name);
                if (is_loading) {
                    qWarning() << "[IncomingCallDialog]: 正在下载: " << file_name;
                    //先加载默认的
                    QPixmap pixmap(":/images/head_default.png");
                    QPixmap scaledPixmap = pixmap.scaled(m_avatarLabel->size(),
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                    m_avatarLabel->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                    m_avatarLabel->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小

                }
                else {
                    QPixmap scaledPixmap = pixmap.scaled(m_avatarLabel->size(),
                                                         Qt::KeepAspectRatio, Qt::SmoothTransformation); // 将图片缩放到label的大小
                    m_avatarLabel->setPixmap(scaledPixmap); // 将缩放后的图片设置到QLabel上
                    m_avatarLabel->setScaledContents(true); // 设置QLabel自动缩放图片内容以适应大小
                }
            }
            else {
                qWarning() << "[IncomingCallDialog]: 无法加载上传的头像：" << avatarPath;
                LoadHeadIcon(avatarPath, m_avatarLabel, file_name, "other_icon");
            }
        }
        else {
            qWarning() << "[IncomingCallDialog]: 头像存储目录不存在：" << avatarsDir.path();
            QString avatarPath = avatarsDir.filePath(QFileInfo(m_callerAvatar).fileName());
            avatarsDir.mkpath(".");
            LoadHeadIcon(avatarPath, m_avatarLabel, file_name, "other_icon");
        }
    }
}

void IncomingCallDialog::onAcceptCall()
{
    qDebug() << "[IncomingCallDialog]: Call accepted from" << m_callerName;
    
    // 停止超时定时器
    if (m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
    }
    
    accept();
}

void IncomingCallDialog::onRejectCall()
{
    qDebug() << "[IncomingCallDialog]: Call rejected from" << m_callerName;
    
    // 停止超时定时器
    if (m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
    }
    
    reject();
}

void IncomingCallDialog::onTimeout()
{
    qDebug() << "[IncomingCallDialog]: Call timeout from" << m_callerName;
    
    reject();
}

// 公共方法来启动超时定时器
void IncomingCallDialog::startTimeout(int seconds)
{
    m_timeoutSeconds = seconds;
    m_timeoutTimer->start(seconds * 1000); // 毫秒
}
