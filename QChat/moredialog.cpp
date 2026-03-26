#include "moredialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QMouseEvent>

#include <QPainter>
#include <QPainterPath>
#include <QApplication>

MoreDialog::MoreDialog(QWidget *parent) : QFrame(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    initUI();
}

MoreDialog::~MoreDialog()
{
    qDebug() << "[MoreDialog]: destroyed";
}

void MoreDialog::initUI()
{
    // 主容器 (不再使用子QWidget设置背景，改由paintEvent绘制，以避免黑线)
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(5);

    auto create_item = [&](const QString& text, const QString& objName) {
        ClickedLabel* lb = new ClickedLabel(this);
        lb->setText(text);
        lb->setObjectName(objName);
        lb->setMinimumSize(100, 30);
        lb->setAlignment(Qt::AlignCenter);
        lb->setStyleSheet("QLabel { padding: 5px; font-size: 14px; color: #333; background: transparent; }"
                          "QLabel:hover { background-color: rgba(224, 240, 255, 200); border-radius: 4px; }");
        return lb;
    };

    _help_lb = create_item("帮助", "help_lb");
    _lock_lb = create_item("锁定", "lock_lb");
    _settings_lb = create_item("设置", "settings_lb");
    _logout_lb = create_item("退出账号", "logout_lb");

    layout->addWidget(_help_lb);
    layout->addWidget(_lock_lb);
    layout->addWidget(_settings_lb);
    layout->addWidget(_logout_lb);

    // 连接点击逻辑
    connect(_logout_lb, &ClickedLabel::clicked, [this]() {
        emit switch_login();
        this->close();
    });

    // 可以在这里根据需求扩展其他按钮的逻辑
    this->adjustSize();
}

void MoreDialog::hideEvent(QHideEvent *event)
{
    emit sig_closed();
    QFrame::hideEvent(event);
}

void MoreDialog::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    qApp->installEventFilter(this);
}

bool MoreDialog::eventFilter(QObject *obj, QEvent *event)
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

void MoreDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRect r = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 10, 10);

    // 设置背景色为浅浅蓝色 (AliceBlue 类似)
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, QColor(240, 254, 255, 245));
    g.setColorAt(1.0, QColor(230, 248, 255, 245));
    p.fillPath(path, g);

    // 绘制边框
    QPen pen(QColor(160, 220, 235, 220));
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.drawPath(path);
}
