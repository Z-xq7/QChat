#include "cute_pet_widget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDebug>

CutePetWidget::CutePetWidget(QWidget *parent)
    : QWidget(parent)
    , _mousePos(0, 0)
    , _isMouseInside(false)
    , _animationFrame(0)
    , _eyeOffsetX(0)
    , _eyeOffsetY(0)
    , _bodyStretchX(1.0)
    , _bodyStretchY(1.0)
    , _bodyTilt(0)
    , _isPeeking(false)
    , _peekState(0)
    , _isBlinking(false)
    , _blinkState(0)
    , _blinkDuration(0)
    , _breathOffset(0)
    , _breathDirection(1.0)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true); // 设置为鼠标事件透明，让鼠标事件穿透
    setMouseTracking(true); // 启用鼠标追踪
    
    // 确保控件能够接收到鼠标移动事件
    setFocusPolicy(Qt::NoFocus); // 不获取焦点，避免影响UI控件

    // 创建动画定时器，降低频率以减少CPU使用
    _animationTimer = new QTimer(this);
    connect(_animationTimer, &QTimer::timeout, this, &CutePetWidget::updateAnimation);
    _animationTimer->start(100); // 每100ms更新一次动画，减少CPU使用

    // 创建偷看动画定时器
    _peekTimer = new QTimer(this);
    connect(_peekTimer, &QTimer::timeout, this, &CutePetWidget::updatePeekAnimation);

    // 创建眨眼动画定时器
    _blinkTimer = new QTimer(this);
    connect(_blinkTimer, &QTimer::timeout, this, &CutePetWidget::updateBlinkAnimation);
    // 随机眨眼间隔：通常在4-8秒之间随机眨眼
    QTimer *randomBlinkTimer = new QTimer(this);
    connect(randomBlinkTimer, &QTimer::timeout, [this]() {
        _isBlinking = true;
        _blinkState = 0;
        _blinkDuration = 0;
        _blinkTimer->start(30); // 每30ms更新一次眨眼动画
    });
    randomBlinkTimer->start(4000 + rand() % 4000); // 4-8秒随机间隔

    // 创建呼吸动画定时器
    _breathTimer = new QTimer(this);
    connect(_breathTimer, &QTimer::timeout, this, &CutePetWidget::updateBreathAnimation);
    _breathTimer->start(150); // 每150ms更新一次呼吸动画
}

CutePetWidget::~CutePetWidget()
{
    if (_blinkTimer) {
        _blinkTimer->stop();
    }
    if (_breathTimer) {
        _breathTimer->stop();
    }
}

void CutePetWidget::resetToOriginalState()
{
    // 重置所有动画参数到初始状态
    _eyeOffsetX = 0;
    _eyeOffsetY = 0;
    _bodyTilt = 0;
    _bodyStretchX = 1.0;
    _bodyStretchY = 1.0;
    _isPeeking = false;
    _peekState = 0;
    if (_peekTimer->isActive()) {
        _peekTimer->stop();
    }
    update(); // 触发重绘
}

void CutePetWidget::setMousePosition(const QPoint &pos)
{
    _mousePos = pos;
    
    // 更新鼠标是否在控件内的状态
    QPoint localPos = mapFromGlobal(_mousePos);
    bool wasMouseInside = _isMouseInside;
    if (rect().contains(localPos)) {
        _isMouseInside = true;
    } else {
        _isMouseInside = false;
    }
    
    // 只有当鼠标状态改变或鼠标在内部时才更新
    if (wasMouseInside != _isMouseInside || _isMouseInside) {
        updateEyePosition();
    }
}

void CutePetWidget::updateEyePosition()
{
    bool changed = false;
    
    if (!_isMouseInside) {
        // 鼠标不在控件内，逐渐恢复到原始状态（平滑过渡）
        if (_eyeOffsetX != 0) { 
            _eyeOffsetX *= 0.7; // 平滑过渡到0
            if (qAbs(_eyeOffsetX) < 0.5) _eyeOffsetX = 0;
            changed = true; 
        }
        if (_eyeOffsetY != 0) { 
            _eyeOffsetY *= 0.7; // 平滑过渡到0
            if (qAbs(_eyeOffsetY) < 0.5) _eyeOffsetY = 0;
            changed = true; 
        }
        if (_bodyTilt != 0) { 
            _bodyTilt *= 0.7; // 平滑过渡到0
            if (qAbs(_bodyTilt) < 1.0) _bodyTilt = 0;
            changed = true; 
        }
        if (_bodyStretchX != 1.0) { 
            _bodyStretchX = 1.0 + (_bodyStretchX - 1.0) * 0.7; // 平滑过渡到1.0
            if (qAbs(_bodyStretchX - 1.0) < 0.01) _bodyStretchX = 1.0;
            changed = true; 
        }
        if (_bodyStretchY != 1.0) { 
            _bodyStretchY = 1.0 + (_bodyStretchY - 1.0) * 0.7; // 平滑过渡到1.0
            if (qAbs(_bodyStretchY - 1.0) < 0.01) _bodyStretchY = 1.0;
            changed = true; 
        }
    } else {
        // 计算眼睛偏移量，限制在一定范围内
        QPoint localPos = mapFromGlobal(_mousePos);
        // 确保不除以0
        qreal widthFactor = width() > 0 ? (localPos.x() - width() / 2.0) / (width() / 2.0) : 0;
        qreal heightFactor = height() > 0 ? (localPos.y() - height() / 2.0) / (height() / 2.0) : 0;

        // 计算新值
        qreal newEyeOffsetX = qBound(-8.0, widthFactor * 12, 8.0); // 减小眼睛偏移范围，更自然
        qreal newEyeOffsetY = qBound(-8.0, heightFactor * 12, 8.0);
        // 修复倾斜方向，增加倾斜程度
        qreal newBodyTilt = -widthFactor * 45; // 适中倾斜程度
        // 减小拉伸幅度，并保持底边宽度不变
        qreal newBodyStretchX = 1.0;  // 保持X轴拉伸为1.0，不改变底边宽度
        qreal newBodyStretchY = 1.0 - heightFactor * 0.08;  // 减小Y轴压缩幅度

        // 限制参数在合理范围内
        newBodyStretchX = qBound(0.97, newBodyStretchX, 1.03); // 基本保持不变
        newBodyStretchY = qBound(0.95, newBodyStretchY, 1.05); // 限制较小的压缩范围

        // 使用平滑插值来更新值，使动作更自然
        _eyeOffsetX = _eyeOffsetX * 0.6 + newEyeOffsetX * 0.4;
        _eyeOffsetY = _eyeOffsetY * 0.6 + newEyeOffsetY * 0.4;
        _bodyTilt = _bodyTilt * 0.6 + newBodyTilt * 0.4;
        _bodyStretchX = _bodyStretchX * 0.6 + newBodyStretchX * 0.4;
        _bodyStretchY = _bodyStretchY * 0.6 + newBodyStretchY * 0.4;
        
        // 检查是否真正改变
        if (qAbs(_eyeOffsetX - newEyeOffsetX) > 0.1) changed = true;
        if (qAbs(_eyeOffsetY - newEyeOffsetY) > 0.1) changed = true;
        if (qAbs(_bodyTilt - newBodyTilt) > 0.5) changed = true;
        if (qAbs(_bodyStretchX - newBodyStretchX) > 0.01) changed = true;
        if (qAbs(_bodyStretchY - newBodyStretchY) > 0.01) changed = true;
    }

    if (changed) {
        update();
    }
}

void CutePetWidget::enterEvent(QEnterEvent *event)
{
    _isMouseInside = true;
    QWidget::enterEvent(event);
}

void CutePetWidget::leaveEvent(QEvent *event)
{
    _isMouseInside = false;
    _eyeOffsetX = 0;
    _eyeOffsetY = 0;
    _bodyTilt = 0;
    _bodyStretchX = 1.0;
    _bodyStretchY = 1.0;
    update();
    QWidget::leaveEvent(event);
}

void CutePetWidget::resizeEvent(QResizeEvent *event)
{
    // 窗口大小改变时更新宠物位置
    QWidget::resizeEvent(event);
    update();
}

void CutePetWidget::mouseMoveEvent(QMouseEvent *event)
{
    // 当鼠标在宠物控件内部移动时，更新鼠标位置
    _mousePos = event->globalPos();
    updateEyePosition();
    // 调用父类的鼠标移动事件
    QWidget::mouseMoveEvent(event);
}

void CutePetWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 按照要求设置固定高度的宠物
    int basePetWidth = qMax(60, width() / 5); // 宠物基础宽度为控件宽度的1/5，最小60像素
    
    // 按照要求设置固定高度（调整后）
    int petHeight1 = 180; // 第一个宠物高度（半圆形，直径180px）
    int petHeight2 = 340; // 第二个宠物高度（长方体，340px）
    int petHeight3 = 245; // 第三个宠物高度（长方体，245px）
    int petHeight4 = 200; // 第四个宠物高度（头部半圆形+身体长方体，200px）
    
    // 为宠物设置调整后的宽度
    int petWidth1 = basePetWidth; // 左边宠物宽度（基础宽度，直径变化不影响宽度参数，但实际绘制会调整）
    int petWidth2 = basePetWidth + 65; // 中间宠物宽度（原基础宽度+25+40px）
    int petWidth3 = basePetWidth + 30; // 右边宠物宽度（原基础宽度+40-10px）
    int petWidth4 = 100; // 新增宠物宽度为100px
    
    // 计算紧凑布局的间距
    int totalPetWidth = petWidth1 + petWidth2 + petWidth3 + petWidth4;
    int spacing = qMax(5, (width() - totalPetWidth) / 5); // 紧凑的间距，最小5像素
    
    // 所有宠物底部固定在同一水平线（即控件底部）
    // 先绘制中间的宠物 (index 1)，然后是原右边的宠物 (index 2)，再绘制新增的宠物 (index 3)，最后绘制左边的宠物 (index 0)
    // 这样左边的宠物会在最前面，新增的宠物在原右边宠物前面
    // 第二个宠物 (中间，长方体，340px高)
    int midPetX = spacing * 1.5 + petWidth1 + spacing + 25; // 中间宠物位置向右移动25px
    drawPet(painter, midPetX, height() - petHeight2, petWidth2, petHeight2, 1);
    
    // 第三个宠物 (原右侧，长方体，245px高)
    int rightPetX = midPetX + petWidth2 + spacing - 62; // 右边宠物向左移动20px
    drawPet(painter, rightPetX, height() - petHeight3, petWidth3, petHeight3, 2);
    
    // 第四个宠物 (新增，头部半圆形+身体长方体，200px高) - 在原右边宠物前面
    int newRightPetX = rightPetX + petWidth3/2 ; // 放在最右侧，部分遮挡原右边宠物
    drawPet(painter, newRightPetX, height() - petHeight4, petWidth4, petHeight4, 3);

    // 第一个宠物 (左侧，半圆形，180px高) - 最后绘制，显示在最前面
    int leftPetX = spacing * 1.5 + 53; // 左边宠物右移40px
    drawPet(painter, leftPetX, height() - petHeight1, petWidth1, petHeight1, 0);
}

void CutePetWidget::drawPet(QPainter &painter, int x, int y, int width, int height, int petIndex)
{
    // 计算基础位置和尺寸
    qreal centerX = x + width / 2.0;
    qreal bottomY = y + height; // 底部Y坐标（固定点）
    
    // 根据宠物索引设置不同颜色
    QColor bodyColor, eyeColor, blushColor;
    switch(petIndex) {
        case 0: // 第一个宠物 - 左边，半圆形，直径180px
            bodyColor = QColor(255, 105, 180); // 鲜明的热粉色 #FF69B4 (与登录按钮颜色一致)
            eyeColor = QColor(139, 0, 0);  // 深红色眼睛，醒目
            blushColor = QColor(255, 182, 193, 120); // 浅粉色腮红
            break;
        case 1: // 第二个宠物 - 中间，长方体，340px高
            bodyColor = QColor(138, 43, 226);  // 鲜明的蓝紫色 #8A2BE2 (与界面渐变协调)
            eyeColor = Qt::white;  // 白色眼白，明亮
            blushColor = QColor(221, 160, 221, 120); // 萝兰紫腮红
            break;
        case 2: // 第三个宠物 - 右边，长方体，245px高
            bodyColor = QColor(100, 149, 237); // 鲜明的矢车菊蓝 #6495ED
            eyeColor = Qt::white;  // 白色眼白，明亮
            blushColor = QColor(173, 216, 230, 120); // 浅蓝色腮红
            break;
        case 3: // 第四个宠物 - 最右边，头部半圆形+身体长方体，200px高
            bodyColor = QColor(255, 165, 0); // 鲜明的橙黄色 #FFA500
            eyeColor = Qt::white;  // 白色眼白，明亮
            blushColor = QColor(255, 182, 193, 120); // 浅粉色腮红
            break;
        default:
            bodyColor = QColor(255, 105, 180); // 默认热粉色
            eyeColor = QColor(139, 0, 0);  // 默认深红色
            blushColor = QColor(255, 182, 193, 120); // 默认腮红
    }

    // 限制变形参数范围，防止过度变形
    qreal clampedTilt = _bodyTilt * 0.003; // 使用_bodyTilt计算倾斜参数
    qreal clampedStretchY = _bodyStretchY;

    // 保存原始画家状态
    painter.save();
    
    // 为不同类型的宠物应用不同的变换策略
    QTransform transform;
    
    if (petIndex == 0) {
        // 对于半圆形宠物，应用轻微的shear变换以实现倾斜，但限制幅度以保持圆形
        transform.translate(centerX, bottomY);     // 移动到变换中心点（底部中心）
        // 使用较小的倾斜系数和shear变换，限制变形以保持圆形
        qreal limitedTilt = clampedTilt * 1; // 限制倾斜幅度，减少变形
        transform.shear(limitedTilt, 0);          // 水平倾斜，使用较小的倾斜值
        transform.scale(1.0, clampedStretchY);    // 只对Y轴进行拉伸/压缩
        transform.translate(-centerX, -bottomY);  // 移回
    }
    else if(petIndex == 1){
        // 对于最高的宠物，应用完整的倾斜和拉伸效果
        // 应用仿射变换实现底部固定的身体变形效果
        // 使用QTransform来控制变换，使底部边缘保持固定宽度
        transform.translate(centerX, bottomY);     // 移动到变换中心点（底部中心）
        qreal limitedTilt = clampedTilt * 1.7; // 限制倾斜幅度，减少变形
        transform.shear(limitedTilt, 0);          // 水平倾斜，保持底部宽度不变
        transform.scale(1.0, clampedStretchY);    // 只对Y轴进行拉伸/压缩
        transform.translate(-centerX, -bottomY);  // 移回
    }
    else if(petIndex == 2){
        // 对于其他类型的宠物，应用完整的倾斜和拉伸效果
        // 应用仿射变换实现底部固定的身体变形效果
        // 使用QTransform来控制变换，使底部边缘保持固定宽度
        transform.translate(centerX, bottomY);     // 移动到变换中心点（底部中心）
        qreal limitedTilt = clampedTilt * 1.3; // 限制倾斜幅度，减少变形
        transform.shear(limitedTilt, 0);          // 水平倾斜，保持底部宽度不变
        transform.scale(1.0, clampedStretchY);    // 只对Y轴进行拉伸/压缩
        transform.translate(-centerX, -bottomY);  // 移回
    }
    else if(petIndex == 3){
        // 对于其他类型的宠物，应用完整的倾斜和拉伸效果
        // 应用仿射变换实现底部固定的身体变形效果
        // 使用QTransform来控制变换，使底部边缘保持固定宽度
        transform.translate(centerX, bottomY);     // 移动到变换中心点（底部中心）
        qreal limitedTilt = clampedTilt * 1; // 限制倾斜幅度，减少变形
        transform.shear(limitedTilt, 0);          // 水平倾斜，保持底部宽度不变
        transform.scale(1.0, clampedStretchY);    // 只对Y轴进行拉伸/压缩
        transform.translate(-centerX, -bottomY);  // 移回
    }
    
    painter.setTransform(transform);

    // 根据宠物索引绘制不同形状的身体
    if (petIndex == 0) {
        // 第一个宠物：标准半圆形身体（直径180px，平边在底部，弧形在顶部）
        qreal radius = 90.0;  // 半圆半径为90px（直径180px）
        
        // 绘制标准半圆形身体（平边在底部，弧形在顶部）
        QPainterPath bodyPath;
        qreal topY = bottomY - radius; // 顶部Y坐标（弧形的最高点）
        
        // 创建一个半圆路径（上半部分），平边在底部，弧形在顶部
        bodyPath.moveTo(centerX + radius, bottomY); // 从右边底部开始
        // 画弧到左边底部，形成上半圆（弧形朝上），完整圆形的宽度和高度都是直径
        bodyPath.arcTo(centerX - radius, topY, radius * 2, radius * 2, 0, 180);
        bodyPath.closeSubpath(); // 闭合路径（自动连接到起始点）
        
        painter.setBrush(bodyColor);
        painter.setPen(QPen(bodyColor.darker(), 2));
        painter.drawPath(bodyPath);
        
        // 添加身体高光，让宠物更立体生动
        // 高光也应用相同的变换，以保持一致性
        painter.save();
        painter.translate(centerX, bottomY);     // 移动到变换中心点（底部中心）
        painter.scale(1.0, clampedStretchY);    // 只对Y轴进行拉伸/压缩
        painter.translate(-centerX, -bottomY);  // 移回
        
        // 绘制高光效果
        QRadialGradient highlightGradient(centerX - radius*0.4, bottomY - radius*0.7, radius*0.6);
        highlightGradient.setColorAt(0, QColor(255, 255, 255, 60));
        highlightGradient.setColorAt(1, QColor(255, 255, 255, 0));
        painter.setBrush(highlightGradient);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(centerX - radius*0.6, bottomY - radius*0.8, radius*0.8, radius*0.8));
        
        painter.restore();
    } else if (petIndex == 3) {
        // 第四个宠物：头部半圆形+身体长方体
        qreal bodyWidth = width * 0.8;  // 身体宽度为整体宽度的80%
        qreal bodyHeight = height * 0.6; // 整体高度为60%
        qreal headHeight = height * 0.3; // 头部高度为30%
        qreal bodyRectHeight = bodyHeight - headHeight; // 身体部分高度
        
        // 计算身体矩形的位置
        qreal bodyTopY = bottomY - bodyHeight;
        QRectF bodyRect(centerX - bodyWidth/2, bodyTopY + headHeight, bodyWidth, bodyRectHeight);
        
        // 绘制身体（圆角矩形）
        painter.setBrush(bodyColor);
        painter.setPen(QPen(bodyColor.darker(), 2));
        painter.drawRoundedRect(bodyRect, 8, 8);
        
        // 绘制头部（半圆形，弧形朝上）
        QPainterPath headPath;
        qreal headTopY = bodyTopY; // 头部顶部Y坐标
        headPath.moveTo(centerX + bodyWidth/2, bodyTopY + headHeight); // 从右侧连接点开始
        headPath.arcTo(centerX - bodyWidth/2, headTopY, bodyWidth, headHeight * 2, 0, 180); // 画半圆
        headPath.closeSubpath(); // 闭合路径
        
        painter.drawPath(headPath);
        
        // 添加身体高光，让宠物更立体生动
        painter.save();
        painter.translate(centerX, bottomY);     // 移动到变换中心点（底部中心）
        painter.shear(clampedTilt, 0);          // 水平倾斜，保持底部固定
        painter.scale(1.0, clampedStretchY);    // 只对Y轴进行拉伸/压缩
        painter.translate(-centerX, -bottomY);  // 移回
        
        // 绘制身体高光
        QRadialGradient bodyHighlightGradient(centerX - bodyWidth*0.2, bodyTopY + headHeight + bodyRectHeight*0.3, bodyWidth*0.3);
        bodyHighlightGradient.setColorAt(0, QColor(255, 255, 255, 60));
        bodyHighlightGradient.setColorAt(1, QColor(255, 255, 255, 0));
        painter.setBrush(bodyHighlightGradient);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(centerX - bodyWidth*0.3, bodyTopY + headHeight + bodyRectHeight*0.1, bodyWidth*0.4, bodyRectHeight*0.4));
        
        // 绘制头部高光
        QRadialGradient headHighlightGradient(centerX - bodyWidth*0.15, bodyTopY + headHeight*0.4, bodyWidth*0.25);
        headHighlightGradient.setColorAt(0, QColor(255, 255, 255, 70));
        headHighlightGradient.setColorAt(1, QColor(255, 255, 255, 0));
        painter.setBrush(headHighlightGradient);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(centerX - bodyWidth*0.25, bodyTopY + headHeight*0.1, bodyWidth*0.35, headHeight*0.5));
        
        painter.restore();
    } else {
        // 第二、三个宠物：长方体身体
        qreal bodyWidth = width * 0.7;  // 身体宽度为整体宽度的70%
        qreal bodyHeight = height * 0.6; // 身体高度为整体高度的60%
        qreal topY = bottomY - bodyHeight; // 顶部Y坐标
        QRectF bodyRect(centerX - bodyWidth/2, topY, bodyWidth, bodyHeight);
        
        // 绘制圆角矩形长方体
        painter.setBrush(bodyColor);
        painter.setPen(QPen(bodyColor.darker(), 2));
        painter.drawRoundedRect(bodyRect, 10, 10);
        
        // 添加身体高光，让宠物更立体生动
        painter.save();
        painter.translate(centerX, bottomY);     // 移动到变换中心点（底部中心）
        painter.shear(clampedTilt, 0);          // 水平倾斜，保持底部固定
        painter.scale(1.0, clampedStretchY);    // 只对Y轴进行拉伸/压缩
        painter.translate(-centerX, -bottomY);  // 移回
        
        // 绘制高光效果
        QRadialGradient highlightGradient(centerX - bodyWidth*0.2, topY + bodyHeight*0.3, bodyWidth*0.3);
        highlightGradient.setColorAt(0, QColor(255, 255, 255, 60));
        highlightGradient.setColorAt(1, QColor(255, 255, 255, 0));
        painter.setBrush(highlightGradient);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(centerX - bodyWidth*0.3, topY + bodyHeight*0.1, bodyWidth*0.4, bodyHeight*0.4));
        
        painter.restore();
    }

    // 绘制眼睛（针对每个宠物类型精确设置）
    qreal eyeRadius, pupilRadius;
    if (petIndex == 0) {
        // 左边宠物：小黑球眼睛
        eyeRadius = 6.0; // 较小的眼睛
        pupilRadius = eyeRadius * 0.5; // 适当大小的瞳孔
    } else if (petIndex == 1) {
        // 中间宠物：小眼睛（白球+小黑球）
        eyeRadius = 6.0; // 小眼睛
        pupilRadius = eyeRadius * 0.5; // 小黑球
    } else {
        // 右边宠物：中等眼睛（白球+黑球）
        eyeRadius = 7.0; // 中等眼睛
        pupilRadius = eyeRadius * 0.5; // 适当大小的黑球
    }
    
    qreal eyeOffset = 15.0; // 较小的眼睛间距

    // 针对每个宠物精确设置眼睛位置
    qreal leftEyeX, leftEyeY, rightEyeX, rightEyeY;
    
    if (petIndex == 0) { // 左边半圆形宠物
        // 半圆形宠物眼睛位置在半圆的上半部分
        leftEyeX = centerX - eyeOffset + _eyeOffsetX * 0.5;
        leftEyeY = y + height * 0.65 + _eyeOffsetY * 0.3; // 合适的垂直位置
        rightEyeX = centerX + eyeOffset + _eyeOffsetX * 0.5;
        rightEyeY = y + height * 0.65 + _eyeOffsetY * 0.3;
    } else if (petIndex == 1) { // 中间长方体宠物
        // 中间宠物眼睛位置在身体上部
        leftEyeX = centerX - eyeOffset + _eyeOffsetX * 0.5;
        leftEyeY = y + height * 0.45 + _eyeOffsetY * 0.3; // 稍低一点的位置
        rightEyeX = centerX + eyeOffset + _eyeOffsetX * 0.5;
        rightEyeY = y + height * 0.45 + _eyeOffsetY * 0.3;
    } else { // 右边长方体宠物
        // 右边宠物眼睛位置在身体上部
        leftEyeX = centerX - eyeOffset + _eyeOffsetX * 0.5;
        leftEyeY = y + height * 0.5 + _eyeOffsetY * 0.3; // 稍低一点的位置
        rightEyeX = centerX + eyeOffset + _eyeOffsetX * 0.5;
        rightEyeY = y + height * 0.5 + _eyeOffsetY * 0.3;
    }

    if (petIndex == 0) {
        // 左边宠物：中等黑球眼睛
        // 绘制眨眼效果
        qreal eyeOpacity = (_isBlinking && _blinkState > 0) ? (5.0 - _blinkState) / 5.0 : 1.0;
        if (eyeOpacity <= 0.1) {
            // 完全闭合状态 - 绘制水平短线表示闭合的眼睛
            painter.setPen(QPen(bodyColor.darker(), 2));
            painter.drawLine(QLineF(leftEyeX - eyeRadius, leftEyeY, leftEyeX + eyeRadius, leftEyeY));
            painter.drawLine(QLineF(rightEyeX - eyeRadius, rightEyeY, rightEyeX + eyeRadius, rightEyeY));
        } else {
            // 绘制正常眼睛
            painter.setBrush(Qt::black);
            painter.setPen(QPen(Qt::black, 1.5));
            painter.drawEllipse(QRectF(leftEyeX - eyeRadius, leftEyeY - eyeRadius, 
                                       eyeRadius * 2, eyeRadius * 2));
            painter.drawEllipse(QRectF(rightEyeX - eyeRadius, rightEyeY - eyeRadius, 
                                       eyeRadius * 2, eyeRadius * 2));
        }
    } else {
        // 中间、右边和第四个宠物：白球+黑球眼睛
        // 绘制眨眼效果
        qreal eyeOpacity = (_isBlinking && _blinkState > 0) ? (5.0 - _blinkState) / 5.0 : 1.0;
        if (eyeOpacity <= 0.1) {
            // 完全闭合状态 - 绘制水平短线表示闭合的眼睛
            painter.setPen(QPen(bodyColor.darker(), 2));
            painter.drawLine(QLineF(leftEyeX - eyeRadius, leftEyeY, leftEyeX + eyeRadius, leftEyeY));
            painter.drawLine(QLineF(rightEyeX - eyeRadius, rightEyeY, rightEyeX + eyeRadius, rightEyeY));
        } else {
            // 眼白
            painter.setBrush(Qt::white);
            painter.setPen(QPen(Qt::black, 1.5));
            painter.drawEllipse(QRectF(leftEyeX - eyeRadius, leftEyeY - eyeRadius, 
                                       eyeRadius * 2, eyeRadius * 2));
            painter.drawEllipse(QRectF(rightEyeX - eyeRadius, rightEyeY - eyeRadius, 
                                       eyeRadius * 2, eyeRadius * 2));
            
            // 瞳孔 - 使用深色绘制，确保在亮色眼白中显眼
            painter.setBrush(Qt::black); // 统一使用黑色瞳孔，确保显眼
            painter.setPen(Qt::NoPen);
            
            // 瞳孔跟随鼠标效果
            qreal pupilOffsetX = _eyeOffsetX * 0.3;
            qreal pupilOffsetY = _eyeOffsetY * 0.3;
            
            painter.drawEllipse(QRectF(leftEyeX - pupilRadius + pupilOffsetX, 
                                       leftEyeY - pupilRadius + pupilOffsetY, 
                                       pupilRadius * 2, pupilRadius * 2));
            painter.drawEllipse(QRectF(rightEyeX - pupilRadius + pupilOffsetX, 
                                       rightEyeY - pupilRadius + pupilOffsetY, 
                                       pupilRadius * 2, pupilRadius * 2));
            
            // 添加瞳孔高光，让眼睛更灵动
            if (eyeOpacity > 0.3) { // 只有在眼睛相对睁开时显示高光
                painter.setBrush(Qt::white);
                painter.setPen(Qt::NoPen);
                // 高光位置稍微偏向光源方向
                qreal highlightSize = pupilRadius * 0.3;
                painter.drawEllipse(QRectF(leftEyeX - pupilRadius * 0.3 + pupilOffsetX * 0.5, 
                                           leftEyeY - pupilRadius * 0.3 + pupilOffsetY * 0.5, 
                                           highlightSize, highlightSize));
                painter.drawEllipse(QRectF(rightEyeX - pupilRadius * 0.3 + pupilOffsetX * 0.5, 
                                           rightEyeY - pupilRadius * 0.3 + pupilOffsetY * 0.5, 
                                           highlightSize, highlightSize));
            }
        }
    }

    // 绘制可爱的腮红（在变换坐标系中）
    painter.setBrush(blushColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QRectF(leftEyeX - eyeRadius * 2, leftEyeY + eyeRadius * 1.5, 
                               eyeRadius * 1.5, eyeRadius * 0.8));
    painter.drawEllipse(QRectF(rightEyeX + eyeRadius * 0.5, rightEyeY + eyeRadius * 1.5, 
                               eyeRadius * 1.5, eyeRadius * 0.8));

    // 恢复画家状态
    painter.restore();
    
    // 如果正在偷看，添加特殊的视觉效果
    if (_isPeeking) {
        // 添加一个"偷看"的表情，增加眼睛大小和瞳孔的跟随效果
        painter.save();
        painter.setBrush(eyeColor);
        painter.setPen(QPen(eyeColor.darker(), 1.2));
        
        // 在当前眼睛上方画一个较小的眼睛，表示偷看状态
        qreal peekEyeRadius = eyeRadius * 0.6;
        qreal peekOffset = 20.0; // 固定偏移
        
        qreal peekEyeX = centerX - peekOffset;
        qreal peekEyeY = (petIndex == 0) ? 
            y + height * 0.3 : // 半圆宠物偷看位置
            y + height * 0.3;  // 长方体宠物偷看位置
        
        // 偷看的眼睛 - 窄长型，表示偷看
        painter.translate(peekEyeX, peekEyeY);
        painter.drawEllipse(QRectF(-peekEyeRadius, -peekEyeRadius * 0.3, 
                                   peekEyeRadius * 1.5, peekEyeRadius * 0.6));
        painter.restore();
    }
}

void CutePetWidget::updateAnimation()
{
    _animationFrame++;
    // 只有在偷看动画进行时才触发重绘，减少不必要的重绘
    if (_isPeeking) {
        update();
    }
}

void CutePetWidget::updatePeekAnimation()
{
    int oldPeekState = _peekState; // 保存旧状态
    _peekState++;
    if (_peekState > 10) { // 持续一段时间后停止偷看
        _peekState = 0;
        _isPeeking = false;
        _peekTimer->stop();
    }
    
    // 只有当状态真正改变时才更新
    if (oldPeekState != _peekState) {
        update();
    }
}

void CutePetWidget::updateBlinkAnimation()
{
    _blinkDuration++;
    
    if (_blinkDuration < 5) {
        // 逐渐闭合眼睛 (0-4: 从睁开到闭合)
        _blinkState = _blinkDuration;
    } else if (_blinkDuration < 7) {
        // 完全闭合状态 (5-6: 完全闭合)
        _blinkState = 5;
    } else if (_blinkDuration < 12) {
        // 逐渐睁开眼睛 (7-11: 从闭合到睁开)
        _blinkState = 11 - _blinkDuration;
    } else {
        // 完成眨眼周期
        _blinkState = 0;
        _isBlinking = false;
        _blinkTimer->stop();
        _blinkDuration = 0;
    }
    
    update();
}

void CutePetWidget::updateBreathAnimation()
{
    // 更新呼吸偏移量，创建轻微的上下浮动效果
    _breathOffset = 0.5 + 0.5 * qSin(_animationFrame * 0.1) * 2.0; // 轻微的呼吸效果
    _animationFrame++;
    update();
}

// 公共槽函数：开始偷看动画
void CutePetWidget::startPeek()
{
    _isPeeking = true;
    _peekState = 0;
    if (!_peekTimer->isActive()) {
        _peekTimer->start(100); // 每100ms更新一次偷看动画
    }
    update();
}
