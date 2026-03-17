#include "titlebar.h"
#include <QIcon>
#include <QApplication>
#include <QStyle>
#include <QPainter>
#include <QMouseEvent>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
    , m_iconLabel(new QLabel(this))
    , m_titleLabel(new QLabel(this))
    , m_minimizeBtn(new QPushButton(this))
    , m_maximizeBtn(new QPushButton(this))
    , m_closeBtn(new QPushButton(this))
    , m_layout(new QHBoxLayout(this))
    , m_dragging(false)
    , m_theme("login")  // 默认主题设为登录页面
    , m_draggable(true)
    , _ani_timer(nullptr)
    , _ani_offset(0)
{
    // 设置标题栏固定高度
    setFixedHeight(30);
    
    // 启用鼠标跟踪，这样即使没有按下鼠标也能跟踪鼠标移动
    setMouseTracking(true);

    // 设置窗口标志，保持透明背景
    setAttribute(Qt::WA_TranslucentBackground, true);
    
    // 初始化背景和动画
    _ani_timer = new QTimer(this);
    connect(_ani_timer, &QTimer::timeout, this, &TitleBar::updateAnimation);
    _ani_timer->start(50); // 每50ms更新一次动画，约20fps
    
    // 初始化背景
    updateBackground();
    
    // 设置按钮样式 - 与聊天页面按钮风格保持一致
    QString btnStyle = 
        "QPushButton { "
        "    border: none; "
        "    background-color: transparent; "
        "    color: white; "
        "    font-size: 12px; "
        "    font-weight: bold; "
        "    min-width: 27px; "
        "    min-height: 27px; "
        "} "
        "QPushButton:hover { "
        "    background-color: rgba(255, 255, 255, 0.2); "
        "} "
        "QPushButton:pressed { "
        "    background-color: rgba(255, 255, 255, 0.3); "
        "}";
    
    m_minimizeBtn->setStyleSheet(btnStyle);
    m_maximizeBtn->setStyleSheet(btnStyle);
    m_closeBtn->setStyleSheet(btnStyle);
    
    // 设置按钮大小提示
    m_minimizeBtn->setMinimumSize(27, 27);
    m_maximizeBtn->setMinimumSize(27, 27);
    m_closeBtn->setMinimumSize(27, 27);
    
    // 设置按钮图标
    m_minimizeBtn->setIcon(QIcon(":/images/minsize.png"));
    m_maximizeBtn->setIcon(QIcon(":/images/maxsize.png"));
    m_closeBtn->setIcon(QIcon(":/images/close2.png"));
    
    // 设置布局
    m_layout->setContentsMargins(5, 0, 5, 0);
    m_layout->setSpacing(4);
    m_layout->addWidget(m_iconLabel);
    m_layout->addWidget(m_titleLabel);
    m_layout->addStretch();
    m_layout->addWidget(m_minimizeBtn);
    m_layout->addWidget(m_maximizeBtn);
    m_layout->addWidget(m_closeBtn);
    
    // 连接按钮信号
    connect(m_minimizeBtn, &QPushButton::clicked, this, &TitleBar::onMinimizeClicked);
    connect(m_maximizeBtn, &QPushButton::clicked, this, &TitleBar::onMaximizeClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &TitleBar::onCloseClicked);
    
    // 设置标题
    m_titleLabel->setText("QChat");
    m_titleLabel->setStyleSheet("color: white; font-weight: bold;");
    
    // 设置默认图标
    setWindowIcon(QIcon(":/images/favicon.png"));
}

void TitleBar::setWindowTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

void TitleBar::setWindowIcon(const QIcon &icon)
{
    m_iconLabel->setPixmap(icon.pixmap(16, 16));
    m_iconLabel->setFixedSize(16, 16);
}

void TitleBar::setTheme(const QString &theme)
{
    m_theme = theme;
    if (theme == "login") {
        // 登录页面主题 - 启动动态背景动画
        if (!_ani_timer) {
            _ani_timer = new QTimer(this);
            connect(_ani_timer, &QTimer::timeout, this, &TitleBar::updateAnimation);
        }
        if (!_ani_timer->isActive()) {
            _ani_timer->start(50); // 每50ms更新一次动画，约20fps
        }
        // 初始化背景
        updateBackground();
    }
    else if (theme == "chat") {
        // 聊天页面主题 - 也需要使用动态背景
        if (!_ani_timer) {
            _ani_timer = new QTimer(this);
            connect(_ani_timer, &QTimer::timeout, this, &TitleBar::updateAnimation);
        }
        if (!_ani_timer->isActive()) {
            _ani_timer->start(50); // 每50ms更新一次动画，约20fps
        }
        // 初始化聊天主题背景
        updateChatBackground();
    }
    else {
        // 默认主题 - 启动动态背景动画
        if (!_ani_timer) {
            _ani_timer = new QTimer(this);
            connect(_ani_timer, &QTimer::timeout, this, &TitleBar::updateAnimation);
        }
        if (!_ani_timer->isActive()) {
            _ani_timer->start(50); // 每50ms更新一次动画，约20fps
        }
        // 初始化背景
        updateBackground();
    }
    
    // 更新按钮样式以匹配当前主题
    updateButtonStyle();
}

void TitleBar::updateBackground()
{
    // 创建与窗口大小相同的背景图片
    _bg_pixmap = QPixmap(size());
    _bg_pixmap.fill(Qt::transparent);

    QPainter painter(&_bg_pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 获取窗口大小
    QRect rect = this->rect();
    int width = rect.width();
    int height = rect.height();

    // 绘制更美观的渐变色背景 - 粉紫色系
    QLinearGradient gradient(0, 0, width, height);
    gradient.setColorAt(0, QColor(255, 182, 193, 200));  // 浅粉色
    gradient.setColorAt(0.5, QColor(221, 160, 221, 200)); // 萝兰紫
    gradient.setColorAt(1, QColor(176, 196, 222, 200));   // 钢蓝色
    painter.fillRect(rect, gradient);

    // 绘制动态装饰效果 - 细线网格
    painter.setPen(QColor(255, 255, 255, 40));
    // 垂直线
    for (int x = 0; x < width; x += 30) {
        int offset = static_cast<int>(qSin(_ani_offset * 0.05 + x * 0.1) * 10);
        painter.drawLine(x, 0, x + offset, height);
    }
    // 水平线
    for (int y = 0; y < height; y += 30) {
        int offset = static_cast<int>(qCos(_ani_offset * 0.05 + y * 0.1) * 10);
        painter.drawLine(0, y, width, y + offset);
    }

    // 绘制漂浮粒子效果
    for (int i = 0; i < 5; ++i) {
        int size = 8 + (i % 3); // 不同大小
        int speed = 1 + (i % 3); // 不同速度
        int x = static_cast<int>(qSin(_ani_offset * 0.02 * speed + i) * (width / 4) + width / 2);
        int y = static_cast<int>(qCos(_ani_offset * 0.02 * speed + i) * (height / 4) + height / 2);

        QRadialGradient particleGradient(x, y, size);
        particleGradient.setColorAt(0, QColor(255, 255, 255, 100));
        particleGradient.setColorAt(1, QColor(255, 255, 255, 0));

        painter.setBrush(particleGradient);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(x - size/2, y - size/2, size, size);
    }

    // 刷新显示
    update();
}

void TitleBar::updateAnimation()
{
    _ani_offset++;
    if (m_theme == "chat") {
        updateChatBackground();
    } else {
        updateBackground();
    }
}

void TitleBar::updateChatBackground()
{
    // 创建与窗口大小相同的背景图片
    _bg_pixmap = QPixmap(size());
    _bg_pixmap.fill(Qt::transparent);

    QPainter painter(&_bg_pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 获取窗口大小
    QRect rect = this->rect();
    int width = rect.width();
    int height = rect.height();

    // 绘制聊天页面风格的渐变色背景 - 浅蓝到浅粉渐变
    QLinearGradient gradient(0, 0, width, height);
    gradient.setColorAt(0, QColor(220, 255, 255, 200));  // 浅蓝
    gradient.setColorAt(1, QColor(255, 225, 255, 200));   // 浅粉
    painter.fillRect(rect, gradient);

    // 绘制动态装饰效果 - 垂直线
    painter.setPen(QColor(180, 230, 230, 40)); // 浅蓝色线条
    for (int x = 0; x < width; x += 30) {
        int offset = static_cast<int>(qSin(_ani_offset * 0.05 + x * 0.1) * 8);
        painter.drawLine(x, 0, x + offset, height);
    }
    // 水平线
    for (int y = 0; y < height; y += 30) {
        int offset = static_cast<int>(qCos(_ani_offset * 0.05 + y * 0.1) * 8);
        painter.drawLine(0, y, width, y + offset);
    }

    // 绘制漂浮粒子效果 - 更符合聊天页面风格
    for (int i = 0; i < 5; ++i) {
        int size = 6 + (i % 2); // 不同大小
        int speed = 1 + (i % 2); // 不同速度
        int x = static_cast<int>(qSin(_ani_offset * 0.02 * speed + i) * (width / 5) + width / 2);
        int y = static_cast<int>(qCos(_ani_offset * 0.02 * speed + i) * (height / 5) + height / 2);

        QRadialGradient particleGradient(x, y, size);
        particleGradient.setColorAt(0, QColor(180, 225, 230, 80)); // 浅蓝色粒子
        particleGradient.setColorAt(1, QColor(240, 255, 255, 0));

        painter.setBrush(particleGradient);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(x - size/2, y - size/2, size, size);
    }

    // 刷新显示
    update();
}

void TitleBar::updateButtonStyle()
{
    // 根据主题设置按钮样式
    QString btnStyle;
    QString textColor;
    
    if (m_theme == "chat") {
        // 聊天页面主题 - 深灰色文字
        textColor = "rgb(50,50,50)";
        btnStyle = QString(
            "QPushButton { "
            "    border: none; "
            "    background-color: transparent; "
            "    color: %1; "
            "    font-size: 12px; "
            "    font-weight: bold; "
            "    min-width: 27px; "
            "    min-height: 27px; "
            "} "
            "QPushButton:hover { "
            "    background-color: rgba(173,216,230, 0.4); "  // 浅蓝色悬停效果
            "} "
            "QPushButton:pressed { "
            "    background-color: rgba(100,149,237, 0.4); "  // 酞蓝色按下效果
            "}"
        ).arg(textColor);
    } else {
        // 登录页面主题 - 白色文字
        textColor = "white";
        btnStyle = QString(
            "QPushButton { "
            "    border: none; "
            "    background-color: transparent; "
            "    color: %1; "
            "    font-size: 12px; "
            "    font-weight: bold; "
            "    min-width: 27px; "
            "    min-height: 27px; "
            "} "
            "QPushButton:hover { "
            "    background-color: rgba(255, 255, 255, 0.2); "
            "} "
            "QPushButton:pressed { "
            "    background-color: rgba(255, 255, 255, 0.3); "
            "}"
        ).arg(textColor);
    }
    
    m_minimizeBtn->setStyleSheet(btnStyle);
    m_maximizeBtn->setStyleSheet(btnStyle);
    m_closeBtn->setStyleSheet(btnStyle);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_draggable) {
        m_dragging = true;
        // 计算鼠标按下的位置相对于窗口的位置
        m_dragPosition = event->globalPos() - parentWidget()->window()->pos();
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && m_draggable && (event->buttons() & Qt::LeftButton)) {
        // 根据鼠标移动的距离移动整个窗口
        parentWidget()->window()->move(event->globalPos() - m_dragPosition);
        event->accept();
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    if (m_draggable) {
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void TitleBar::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制动态背景
    painter.drawPixmap(0, 0, _bg_pixmap);

    // 绘制圆角矩形窗体内容（保持原UI）
    QPainterPath path;
    path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 5, 5); // 使用5像素的圆角
    painter.setClipPath(path);

    // 调用父类的绘制函数以保持UI内容
    QWidget::paintEvent(event);
}

void TitleBar::onMinimizeClicked()
{
    emit minimizeClicked();
}

void TitleBar::onMaximizeClicked()
{
    emit maximizeClicked();
}

void TitleBar::onCloseClicked()
{
    emit closeClicked();
}
