#include "videocallwindow.h"
#include "ui_videocallwindow.h"
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QLabel>
#include <QTimer>
#include <QIcon>
#include <QSizePolicy>
#include <QEvent>
#include <QMouseEvent>
#include "YangRtcWrapper.h"

VideoCallWindow::VideoCallWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::VideoCallWindow)
    , m_localVideoWidget(nullptr)
    , m_remoteVideoWidget(nullptr)
    , m_statusLabel(nullptr)
    , m_peerUid(0)
    , m_useRtcLocal(false)
    , m_dragging(false)
{
    ui->setupUi(this);

    // 隐藏mainwindow底部边框
    this->statusBar()->hide();
    // 设置无边框窗口，这样系统菜单栏就不会显示
    setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);

    setStyleSheet(
        "QWidget#centralwidget{background-color:#141414;}"
        "QWidget#title_wid{background-color:#1f1f1f;}"
        "QWidget#video_wid{background-color:#141414;}"
        "QWidget#control_wid{background-color:#1f1f1f;}"
        "QLabel#title_label{color:#eaeaea;}"
        "QPushButton#minsize_btn,QPushButton#maxsize_btn,QPushButton#close_btn{border:none;background:transparent;}"
        "QPushButton#minsize_btn:hover,QPushButton#maxsize_btn:hover{background-color:#2a2a2a;border-radius:4px;}"
        "QPushButton#close_btn:hover{background-color:#b00020;border-radius:4px;}"
    );

    ui->minsize_btn->setIcon(QIcon(":/images/minsize.png"));
    ui->maxsize_btn->setIcon(QIcon(":/images/maxsize.png"));
    ui->close_btn->setIcon(QIcon(":/images/close2.png"));
    ui->minsize_btn->setIconSize(QSize(14, 14));
    ui->maxsize_btn->setIconSize(QSize(14, 14));
    ui->close_btn->setIconSize(QSize(14, 14));

    ui->title_wid->installEventFilter(this);
    ui->title_label->installEventFilter(this);

    // 初始化视频控件
    initVideoWidgets();
    
    // 连接控件信号
    connect(ui->leave_label, &ClickedLabel::clicked, this, &VideoCallWindow::slot_leave_clicked);
    connect(ui->voice_label, &ClickedLabel::clicked, this, &VideoCallWindow::slot_voice_clicked);
    connect(ui->video_label, &ClickedLabel::clicked, this, &VideoCallWindow::slot_video_clicked);
    connect(ui->minsize_btn, &QPushButton::clicked, this, &VideoCallWindow::onMinSizeClicked);
    connect(ui->maxsize_btn, &QPushButton::clicked, this, &VideoCallWindow::onMaxSizeClicked);
    connect(ui->close_btn, &QPushButton::clicked, this, &VideoCallWindow::onCloseClicked);
    
    // 连接视频通话管理器信号
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigIncomingCall,
            this, &VideoCallWindow::onIncomingCall);
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigCallAccepted,
            this, &VideoCallWindow::onCallAccepted);
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigCallRejected,
            this, &VideoCallWindow::onCallRejected);
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigCallHangup,
            this, &VideoCallWindow::onCallHangup);
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigCallStateChanged,
            this, &VideoCallWindow::onCallStateChanged);
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigError,
            this, &VideoCallWindow::onError);
    connect(VideoCallManager::GetInstance().get(), &VideoCallManager::sigLocalPreviewFrame,
            this, &VideoCallWindow::onLocalPreviewFrame);
}

VideoCallWindow::~VideoCallWindow()
{
    // 停止视频模拟
    if (m_localVideoWidget) {
        m_localVideoWidget->stopVideoSimulation();
    }
    if (m_remoteVideoWidget) {
        m_remoteVideoWidget->stopVideoSimulation();
    }
    
    delete ui;
}

void VideoCallWindow::initVideoWidgets()
{
    // 获取UI中的video_self和video_other控件
    QWidget* localContainer = ui->video_self;
    QWidget* remoteContainer = ui->video_other;
    
    // 创建本地视频控件
    m_localVideoWidget = new QVideoOutputWidget(localContainer);
    m_localVideoWidget->setIsLocalVideo(true);
    m_localVideoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_localVideoWidget->startVideoSimulation(); // 开始模拟视频流，直到收到真实视频数据
    
    // 创建远端视频控件
    m_remoteVideoWidget = new QVideoOutputWidget(remoteContainer);
    m_remoteVideoWidget->setIsLocalVideo(false);
    m_remoteVideoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_remoteVideoWidget->startVideoSimulation(); // 开始模拟视频流，直到收到真实视频数据
    
    // 设置布局
    QVBoxLayout* localLayout = new QVBoxLayout(localContainer);
    localLayout->addWidget(m_localVideoWidget);
    localLayout->setContentsMargins(10, 10, 10, 10); // 设置边距
    localContainer->setLayout(localLayout);
    
    QVBoxLayout* remoteLayout = new QVBoxLayout(remoteContainer);
    remoteLayout->addWidget(m_remoteVideoWidget);
    remoteLayout->setContentsMargins(10, 10, 10, 10); // 设置边距
    remoteContainer->setLayout(remoteLayout);
    
    // 找到状态标签
    m_statusLabel = ui->title_label;
}

void VideoCallWindow::setLocalVideoWidget(QVideoOutputWidget* widget)
{
    if (m_localVideoWidget) {
        delete m_localVideoWidget;
    }
    m_localVideoWidget = widget;
    m_localVideoWidget->setIsLocalVideo(true);
}

void VideoCallWindow::setRemoteVideoWidget(QVideoOutputWidget* widget)
{
    if (m_remoteVideoWidget) {
        delete m_remoteVideoWidget;
    }
    m_remoteVideoWidget = widget;
    m_remoteVideoWidget->setIsLocalVideo(false);
}

void VideoCallWindow::setUserInfo(const QString& nick, int uid)
{
    m_peerUid = uid;
    // 可以在状态栏或其他位置显示用户信息
    if (m_statusLabel) {
        m_statusLabel->setText(nick + " - 等待对方接听...");
    }
}

void VideoCallWindow::updateCallStatus(const QString& status)
{
    if (m_statusLabel) {
        m_statusLabel->setText(status);
    }
}

bool VideoCallWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->title_wid || watched == ui->title_label) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && !isMaximized()) {
                m_dragging = true;
                m_dragOffset = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (m_dragging && (mouseEvent->buttons() & Qt::LeftButton) && !isMaximized()) {
                move(mouseEvent->globalPosition().toPoint() - m_dragOffset);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_dragging = false;
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void VideoCallWindow::slot_close_window()
{
    // 发送挂断请求
    VideoCallManager::GetInstance()->hangupCall();
    this->close();
}

void VideoCallWindow::slot_leave_clicked()
{
    // 挂断通话
    VideoCallManager::GetInstance()->hangupCall();
    this->close();
}

void VideoCallWindow::slot_voice_clicked()
{
    // 静音/取消静音功能
    qDebug() << "[VideoCallWindow]: Voice toggle clicked";
}

void VideoCallWindow::slot_video_clicked()
{
    // 开启/关闭摄像头功能
    qDebug() << "[VideoCallWindow]: Video toggle clicked";
}

void VideoCallWindow::onMinSizeClicked()
{
    showMinimized();
}

void VideoCallWindow::onMaxSizeClicked()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void VideoCallWindow::onCloseClicked()
{
    VideoCallManager::GetInstance()->hangupCall();
    this->close();
}

void VideoCallWindow::onIncomingCall(int caller_uid, const QString& call_id, const QString& caller_name)
{
    if (m_statusLabel) {
        m_statusLabel->setText(caller_name + " 正在呼叫...");
    }
    qDebug() << "[VideoCallWindow]: Incoming call from" << caller_uid << caller_name;
}

void VideoCallWindow::onCallAccepted(const QString& call_id, const QString& room_id, const QString& turn_ws_url, const QJsonArray& ice_servers)
{
    if (m_statusLabel) {
        m_statusLabel->setText("通话中...");
    }
    qDebug() << "[VideoCallWindow]: Call accepted, room:" << room_id;

    YangRtcWrapper* rtcWrapper = VideoCallManager::GetInstance()->getRtcWrapper();
    if (rtcWrapper) {
        // 远程视频帧回调
        rtcWrapper->setRemoteVideoCallback(
            [this](const QByteArray& frame, int width, int height, int format) {
                qDebug() << "[VideoCallWindow] remote frame, w=" << width << " h=" << height << " fmt=" << format << " size=" << frame.size();
                QMetaObject::invokeMethod(this, [this, frame, width, height, format]() {
                    if (m_remoteVideoWidget && !frame.isEmpty()) {
                        m_remoteVideoWidget->stopVideoSimulation();
                        m_remoteVideoWidget->setVideoFrame(
                            reinterpret_cast<uint8_t*>(const_cast<char*>(frame.constData())),
                            width, height, format);
                    }
                });
            }
        );

        // 本地视频帧回调（用于本地预览）
        rtcWrapper->setLocalVideoCallback(
            [this](const QByteArray& frame, int width, int height, int format) {
                qDebug() << "[VideoCallWindow] local frame, w=" << width << " h=" << height << " fmt=" << format << " size=" << frame.size();
                QMetaObject::invokeMethod(this, [this, frame, width, height, format]() {
                    if (m_localVideoWidget && !frame.isEmpty()) {
                        m_useRtcLocal = true;
                        m_localVideoWidget->stopVideoSimulation();
                        m_localVideoWidget->setVideoFrame(
                            reinterpret_cast<uint8_t*>(const_cast<char*>(frame.constData())),
                            width, height, format);
                    }
                });
            }
        );
    }
}

void VideoCallWindow::onCallRejected(const QString& call_id, const QString& reason)
{
    if (m_statusLabel) {
        m_statusLabel->setText("通话被拒绝: " + reason);
    }
    qDebug() << "[VideoCallWindow]: Call rejected, reason:" << reason;
}

void VideoCallWindow::onCallHangup(const QString& call_id)
{
    if (m_statusLabel) {
        m_statusLabel->setText("通话已结束");
    }
    qDebug() << "[VideoCallWindow]: Call hangup";
    // 延迟关闭窗口，让用户看到结束状态
    QTimer::singleShot(1500, this, &VideoCallWindow::close);
}

void VideoCallWindow::onCallStateChanged(VideoCallState new_state)
{
    switch (new_state) {
        case VideoCallState::Idle:
            if (m_statusLabel) {
                m_statusLabel->setText("空闲");
            }
            break;
        case VideoCallState::Ringing:
            if (m_statusLabel) {
                m_statusLabel->setText("响铃中...");
            }
            break;
        case VideoCallState::Connecting:
            if (m_statusLabel) {
                m_statusLabel->setText("连接中...");
            }
            break;
        case VideoCallState::InCall:
            if (m_statusLabel) {
                m_statusLabel->setText("通话中...");
            }
            // 确保视频组件已准备好接收数据
            if (m_localVideoWidget) {
                m_localVideoWidget->startVideoSimulation(); // 启动本地视频模拟，直到收到实际数据
            }
            if (m_remoteVideoWidget) {
                m_remoteVideoWidget->startVideoSimulation(); // 启动远程视频模拟，直到收到实际数据
            }
            break;
    }
}

void VideoCallWindow::onError(const QString& error_msg)
{
    if (m_statusLabel) {
        m_statusLabel->setText(error_msg);
    }
    qDebug() << "[VideoCallWindow]: Error:" << error_msg;
}

void VideoCallWindow::onLocalPreviewFrame(const QByteArray& frame, int width, int height, int format)
{
    if (m_useRtcLocal) return;
    if (!m_localVideoWidget) return;
    if (frame.isEmpty()) return;

    QMetaObject::invokeMethod(this, [this, frame, width, height, format]() {
        if (!m_localVideoWidget) return;
        if (frame.isEmpty()) return;
        m_localVideoWidget->stopVideoSimulation();
        m_localVideoWidget->setVideoFrame(
            reinterpret_cast<uint8_t*>(const_cast<char*>(frame.constData())),
            width, height, format);
    });
}
