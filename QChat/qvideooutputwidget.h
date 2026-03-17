#ifndef QVIDEOOUTPUTWIDGET_H
#define QVIDEOOUTPUTWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QPainter>
#include <QPaintEvent>

// 模拟视频输出控件
class QVideoOutputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QVideoOutputWidget(QWidget *parent = nullptr);
    ~QVideoOutputWidget();

    // 设置显示的图片
    void setImage(const QPixmap &image);
    // 设置是否为本地视频（用于UI样式变化）
    void setIsLocalVideo(bool isLocal);
    
    // 从视频帧数据创建图片并显示
    void setVideoFrame(uint8_t* data, int width, int height, int format = 0); // format: 0=RGB, 1=YUV420P, etc.
    
    // 模拟视频流
    void startVideoSimulation();
    void stopVideoSimulation();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_currentImage;
    bool m_isLocalVideo;
    QTimer *m_simulationTimer;
    int m_simulationFrame;
    
    // YUV420P到RGB转换函数
    QImage yuv420pToRgb(const uint8_t* yuv, int width, int height);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void updateSimulationFrame();
};

#endif // QVIDEOOUTPUTWIDGET_H