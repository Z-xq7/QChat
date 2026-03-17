#include "qvideooutputwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QDebug>
#include <QImage>
#include <QtMath>

QVideoOutputWidget::QVideoOutputWidget(QWidget *parent)
    : QWidget(parent)
    , m_isLocalVideo(false)
    , m_simulationTimer(nullptr)
    , m_simulationFrame(0)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background-color: rgba(0, 0, 0, 100);"); // 半透明背景
    setMinimumSize(430, 450);
    
    // 创建模拟定时器
    m_simulationTimer = new QTimer(this);
    connect(m_simulationTimer, &QTimer::timeout, this, &QVideoOutputWidget::updateSimulationFrame);
}

QVideoOutputWidget::~QVideoOutputWidget()
{
    if (m_simulationTimer->isActive()) {
        m_simulationTimer->stop();
    }
}

void QVideoOutputWidget::setImage(const QPixmap &image)
{
    m_currentImage = image;
    update(); // 触发重绘
}

QImage QVideoOutputWidget::yuv420pToRgb(const uint8_t* yuv, int width, int height)
{
    // 创建RGB图像
    QImage rgbImage(width, height, QImage::Format_RGB888);
    
    int half_width = width / 2;
    int half_height = height / 2;
    int total_size = width * height;
    int half_size = half_width * half_height;
    
    const uint8_t* y_plane = yuv;                       // Y平面开始
    const uint8_t* u_plane = yuv + total_size;          // U平面开始
    const uint8_t* v_plane = yuv + total_size + half_size; // V平面开始
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // 获取Y值
            int y_index = y * width + x;
            int y_val = y_plane[y_index];
            
            // 获取UV值（YUV420P是4:2:0采样，UV分辨率是Y的一半）
            int u_index = (y / 2) * half_width + (x / 2);
            int u_val = u_plane[u_index];
            int v_val = v_plane[u_index];
            
            // YUV到RGB转换公式
            int c = y_val - 16;
            int d = u_val - 128;
            int e = v_val - 128;
            
            int r = qBound(0, (298 * c + 409 * e + 128) >> 8, 255);
            int g = qBound(0, (298 * c - 100 * d - 208 * e + 128) >> 8, 255);
            int b = qBound(0, (298 * c + 516 * d + 128) >> 8, 255);
            
            rgbImage.setPixel(x, y, qRgb(r, g, b));
        }
    }
    
    return rgbImage;
}

void QVideoOutputWidget::setVideoFrame(uint8_t* data, int width, int height, int format)
{
    qDebug() << "[QVideoOutputWidget] setVideoFrame, fmt=" << format << " w=" << width << " h=" << height;
    // 将YUV或RGB数据转换为QImage并显示
    if (data && width > 0 && height > 0) {
        QImage image;
        
        // 根据格式选择适当的QImage格式
        switch (format) {
            case 0: // RGB
            {
                image = QImage(data, width, height, width * 3, QImage::Format_RGB888).copy();
                break;
            }
            case 1: // YUV420P - 需要转换为RGB
            {
                // 使用自定义转换函数
                image = yuv420pToRgb(data, width, height);
                break;
            }
            case 2: // BGRA
            {
                image = QImage(data, width, height, width * 4, QImage::Format_ARGB32).copy();
                break;
            }
            default:
                image = QImage(width, height, QImage::Format_RGB888);
                image.fill(QColor(128, 128, 128)); // 默认灰色占位符
                break;
        }
        
        if (!image.isNull()) {
            m_currentImage = QPixmap::fromImage(image);
            update(); // 触发重绘
        }
    }
}

void QVideoOutputWidget::setIsLocalVideo(bool isLocal)
{
    m_isLocalVideo = isLocal;
    update();
}

void QVideoOutputWidget::startVideoSimulation()
{
    if (!m_simulationTimer->isActive()) {
        m_simulationTimer->start(100); // 每100ms更新一次
    }
}

void QVideoOutputWidget::stopVideoSimulation()
{
    if (m_simulationTimer->isActive()) {
        m_simulationTimer->stop();
    }
}

void QVideoOutputWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制背景
    painter.fillRect(rect(), QColor(0, 0, 0, 150)); // 深色半透明背景
    
    // 如果有图片则绘制图片
    if (!m_currentImage.isNull()) {
        // 计算图片在控件中的显示位置（居中）
        QRect imageRect = m_currentImage.rect();
        imageRect.setSize(imageRect.size().scaled(this->size(), Qt::KeepAspectRatio));
        imageRect.moveCenter(this->rect().center());
        
        painter.drawPixmap(imageRect, m_currentImage);
    } else {
        // 如果没有图片，绘制模拟视频内容
        if (m_simulationTimer->isActive()) {
            // 绘制模拟视频帧
            QRect rect = this->rect().adjusted(20, 20, -20, -20);
            
            // 绘制边框
            QPen pen(m_isLocalVideo ? QColor(0, 255, 0) : QColor(0, 150, 255));
            pen.setWidth(2);
            painter.setPen(pen);
            painter.drawRect(rect);
            
            // 绘制一些模拟内容
            painter.setBrush(QBrush(QColor(50, 50, 50, 100)));
            painter.drawRect(rect.adjusted(10, 10, -10, -10));
            
            // 绘制用户头像占位符
            int avatarSize = qMin(rect.width(), rect.height()) / 3;
            QRect avatarRect(rect.center().x() - avatarSize/2, 
                             rect.center().y() - avatarSize/2, 
                             avatarSize, avatarSize);
            painter.setBrush(QBrush(QColor(100, 100, 100)));
            painter.drawEllipse(avatarRect);
            
            // 绘制用户名
            painter.setPen(QColor(200, 200, 200));
            painter.drawText(rect, Qt::AlignCenter, 
                           m_isLocalVideo ? "Local Video" : "Remote Video");
        } else {
            // 绘制占位符
            painter.setPen(QColor(100, 100, 100));
            painter.drawText(rect(), Qt::AlignCenter, 
                           m_isLocalVideo ? "Local Video" : "Remote Video");
        }
    }
    
    // 绘制视频状态指示器（小方块）
    QRect statusRect(10, 10, 12, 12);
    painter.setBrush(m_isLocalVideo ? QColor(0, 255, 0) : QColor(0, 150, 255));
    painter.setPen(Qt::NoPen);
    painter.drawRect(statusRect);
}

void QVideoOutputWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}

void QVideoOutputWidget::updateSimulationFrame()
{
    m_simulationFrame++;
    update(); // 触发重绘以更新模拟内容
}
