#include "filebubble.h"
#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QDesktopServices>
#include <QUrl>
#include <QMouseEvent>

FileBubble::FileBubble(ChatRole role, const QString& fileName, qint64 fileSize, QWidget* parent)
    : BubbleFrame(role, parent), _msg_info(nullptr)
{
    QHBoxLayout* hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(10, 10, 10, 10);
    hLayout->setSpacing(10);

    m_iconLabel = new QLabel();
    m_iconLabel->setFixedSize(40, 40);
    
    QFileInfo fileInfo(fileName);
    QFileIconProvider provider;
    QIcon icon = provider.icon(fileInfo);
    m_iconLabel->setPixmap(icon.pixmap(40, 40));
    m_iconLabel->setScaledContents(true);

    QVBoxLayout* vLayout = new QVBoxLayout();
    vLayout->setSpacing(2);
    
    m_nameLabel = new QLabel(fileInfo.fileName());
    m_nameLabel->setStyleSheet("font-weight: bold; color: #333;");
    
    QString sizeStr = formatSize(fileSize);
    m_sizeLabel = new QLabel(sizeStr);
    m_sizeLabel->setStyleSheet("color: #888; font-size: 10px;");

    m_progressBar = new QProgressBar();
    m_progressBar->setFixedHeight(4);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet("QProgressBar { background-color: #e0e0e0; border: none; border-radius: 2px; } "
                               "QProgressBar::chunk { background-color: #00a3ff; border-radius: 2px; }");
    m_progressBar->hide();

    vLayout->addWidget(m_nameLabel);
    vLayout->addWidget(m_sizeLabel);
    vLayout->addWidget(m_progressBar);

    if (role == ChatRole::Self) {
        hLayout->addLayout(vLayout);
        hLayout->addWidget(m_iconLabel);
    } else {
        hLayout->addWidget(m_iconLabel);
        hLayout->addLayout(vLayout);
    }

    QWidget* container = new QWidget();
    container->setLayout(hLayout);
    this->setMargin(0);
    this->setWidget(container);
    this->setCursor(Qt::PointingHandCursor);
}

QString FileBubble::formatSize(qint64 size)
{
    QString sizeStr;
    if (size < 1024) {
        sizeStr = QString::number(size) + " B";
    } else if (size < 1024 * 1024) {
        sizeStr = QString::number(size / 1024.0, 'f', 1) + " KB";
    } else {
        sizeStr = QString::number(size / (1024.0 * 1024.0), 'f', 1) + " MB";
    }
    return sizeStr;
}

void FileBubble::setProgress(int value, int total_value)
{
    m_progressBar->show();
    m_progressBar->setMaximum(total_value);
    m_progressBar->setValue(value);

    // 更新大小显示
    m_sizeLabel->setText(formatSize(total_value));
    
    if (value >= total_value) {
        m_progressBar->hide();
        if (_msg_info) {
            _msg_info->_transfer_state = TransferState::Completed;
        }
    }
}

void FileBubble::setFailed()
{
    m_progressBar->hide();
    m_sizeLabel->setText("传输失败");
    m_sizeLabel->setStyleSheet("color: red; font-size: 10px;");
    if (_msg_info) {
        _msg_info->_transfer_state = TransferState::Failed;
    }
}

void FileBubble::setMsgInfo(std::shared_ptr<MsgInfo> msg)
{
    _msg_info = msg;
    if (_msg_info) {
        m_nameLabel->setText(_msg_info->_origin_name);
        m_sizeLabel->setText(formatSize(_msg_info->_total_size));
        
        QFileIconProvider provider;
        QIcon icon = provider.icon(QFileInfo(_msg_info->_origin_name));
        m_iconLabel->setPixmap(icon.pixmap(40, 40));
    }
}

void FileBubble::setDownloadFinish(std::shared_ptr<MsgInfo> msg, QString file_path)
{
    _msg_info = msg;
    _msg_info->_text_or_url = file_path;
    _msg_info->_transfer_state = TransferState::Completed;
    m_progressBar->hide();
}

void FileBubble::onFileClicked()
{
    if (_msg_info) {
        // 判断本地文件是否存在
        bool fileExists = false;
        if (!_msg_info->_text_or_url.isEmpty()) {
            QFileInfo fileInfo(_msg_info->_text_or_url);
            if (fileInfo.isFile() && fileInfo.exists()) {
                fileExists = true;
            }
        }

        if (_msg_info->_transfer_state == TransferState::Completed && fileExists) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(_msg_info->_text_or_url));
        } else if (_msg_info->_transfer_state == TransferState::Downloading) {
            // 如果正在下载，可以提示用户正在下载中，或者不执行操作
            qDebug() << "[FileBubble]: File is still downloading...";
        } else if (_msg_info->_transfer_state == TransferState::Failed || !fileExists) {
            qDebug() << "[FileBubble]: Starting/Retrying transfer...";
            emit resumeRequested(_msg_info);
        }
    }
}

void FileBubble::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        onFileClicked();
    }
    BubbleFrame::mousePressEvent(event);
}
