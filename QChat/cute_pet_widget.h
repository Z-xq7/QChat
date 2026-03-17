#ifndef CUTE_PET_WIDGET_H
#define CUTE_PET_WIDGET_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QPoint>
#include <QPropertyAnimation>
#include <QEasingCurve>

class CutePetWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CutePetWidget(QWidget *parent = nullptr);
    ~CutePetWidget();

    void setMousePosition(const QPoint &pos);
    void resetToOriginalState(); // 重置宠物到原始状态

public slots:
    void startPeek();           // 开始偷看动画

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QPoint _mousePos;           // 鼠标位置
    bool _isMouseInside;        // 鼠标是否在控件内
    QTimer *_animationTimer;    // 动画定时器
    int _animationFrame;        // 动画帧
    qreal _eyeOffsetX;          // 眼睛偏移量
    qreal _eyeOffsetY;
    qreal _bodyStretchX;        // 身体X轴拉伸
    qreal _bodyStretchY;        // 身体Y轴拉伸
    qreal _bodyTilt;            // 身体倾斜角度
    bool _isPeeking;            // 是否在偷看
    int _peekState;             // 偷看状态
    QTimer *_peekTimer;         // 偷看动画定时器
    // 眨眼动画相关
    QTimer *_blinkTimer;        // 眨眼定时器
    bool _isBlinking;           // 是否正在眨眼
    int _blinkState;            // 眨眼状态（0-完全睁开，1-完全闭合）
    int _blinkDuration;         // 眨眼持续时间
    QTimer *_breathTimer;       // 呼吸定时器
    qreal _breathOffset;        // 呼吸偏移量
    qreal _breathDirection;     // 呼吸方向

    void updateEyePosition();   // 更新眼睛位置
    void drawPet(QPainter &painter, int x, int y, int width, int height, int petIndex); // 绘制宠物

private slots:
    void updateAnimation();     // 更新动画
    void updatePeekAnimation(); // 更新偷看动画
    void updateBlinkAnimation(); // 更新眨眼动画
    void updateBreathAnimation(); // 更新呼吸动画
};

#endif // CUTE_PET_WIDGET_H