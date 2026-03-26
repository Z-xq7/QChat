#include "emojipopup.h"

#include <QApplication>
#include <QEvent>
#include <QGridLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <QPainterPath>
#include <QTimer>
#include <QWheelEvent>

static void setScrollBarActive(QScrollBar* bar, bool active)
{
    if (!bar) return;
    if (active) {
        bar->setStyleSheet(
            "QScrollBar:vertical{background:transparent;width:10px;margin:6px 4px 6px 0px;}"
            "QScrollBar::handle:vertical{background:rgba(120,190,220,210);border-radius:4px;min-height:30px;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
            "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}"
        );
    } else {
        bar->setStyleSheet(
            "QScrollBar:vertical{background:transparent;width:10px;margin:6px 4px 6px 0px;}"
            "QScrollBar::handle:vertical{background:rgba(120,190,220,0);border-radius:4px;min-height:30px;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
            "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}"
        );
    }
}

EmojiPopup::EmojiPopup(QWidget* parent)
    : QFrame(parent)
    , m_scrollArea(new QScrollArea(this))
    , m_gridHost(new QWidget(this))
    , m_scrollbarTimer(new QTimer(this))
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    setStyleSheet(
        "QScrollArea{background:transparent;border:none;}"
        "QWidget{background:transparent;}"
        "QToolButton{background-color:rgba(255,255,255,230);border:1px solid rgba(160,220,235,170);border-radius:10px;padding:4px;}"
        "QToolButton:hover{background-color:rgba(255,255,255,255);}"
        "QToolButton:pressed{background-color:rgba(245,250,255,255);}"
    );
    setObjectName("EmojiPopup");

    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_scrollArea->setWidget(m_gridHost);

    auto root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->addWidget(m_scrollArea);

    m_scrollbarTimer->setSingleShot(true);
    connect(m_scrollbarTimer, &QTimer::timeout, this, [this]() {
        setScrollBarActive(m_scrollArea->verticalScrollBar(), false);
    });
    setScrollBarActive(m_scrollArea->verticalScrollBar(), false);
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        setScrollBarActive(m_scrollArea->verticalScrollBar(), true);
        m_scrollbarTimer->start(700);
    });

    setEmojis(QStringList{
        "😀","😁","😂","🤣","😃","😄","😅","😆","😉","😊","😍","😘","😗","😙","😚","🙂",
        "🤗","🤩","🤔","🤨","😐","😑","😶","🙄","😏","😣","😥","😮","🤐","😯","😪","😫",
        "😴","😌","😛","😜","😝","🤤","😒","😓","😔","😕","🙃","🫠","🤑","😲","☹️","🙁",
        "😖","😞","😟","😤","😢","😭","😦","😧","😨","😩","🤯","😬","😰","😱","🥵","🥶",
        "😳","🤪","😵","🥴","😠","😡","🤬","😷","🤒","🤕","🤢","🤮","🤧","😇","🥳","🤠",
        "👍","👎","👏","🙏","💪","🔥","🎉","❤️","💔","⭐","🌙","☀️","🌧️","🍀","🍎","🍕"
    });
}

void EmojiPopup::setEmojis(const QStringList& emojis)
{
    m_emojis = emojis;
    rebuild();
}

void EmojiPopup::rebuild()
{
    const auto oldButtons = m_gridHost->findChildren<QToolButton*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* btn : oldButtons) {
        btn->deleteLater();
    }
    delete m_gridHost->layout();

    auto grid = new QGridLayout(m_gridHost);
    grid->setContentsMargins(0, 0, 12, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    const int cellSize = 40;
    const int spacing = 10;
    int viewportWidth = m_scrollArea->viewport()->width();
    if (viewportWidth <= 0) {
        viewportWidth = width() - 20;
    }
    const int cols = qMax(6, (viewportWidth + spacing) / (cellSize + spacing));
    QFont font("Segoe UI Emoji");
    font.setPointSize(16);

    for (int i = 0; i < m_emojis.size(); ++i) {
        auto btn = new QToolButton(m_gridHost);
        btn->setText(m_emojis[i]);
        btn->setFont(font);
        btn->setAutoRaise(true);
        btn->setFixedSize(cellSize, cellSize);
        connect(btn, &QToolButton::clicked, this, [this, i]() {
            emit emojiSelected(m_emojis[i]);
            close();
        });
        grid->addWidget(btn, i / cols, i % cols);
    }
    grid->setRowStretch((m_emojis.size() + cols - 1) / cols, 1);
}

bool EmojiPopup::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        const QPoint globalPos = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
        const QPoint localPos = mapFromGlobal(globalPos);
        if (!rect().contains(localPos)) {
            close();
            return false;
        }
    }
    if (event->type() == QEvent::Wheel) {
        const QPoint globalPos = static_cast<QWheelEvent*>(event)->globalPosition().toPoint();
        const QPoint localPos = mapFromGlobal(globalPos);
        if (rect().contains(localPos)) {
            setScrollBarActive(m_scrollArea->verticalScrollBar(), true);
            m_scrollbarTimer->start(700);
        }
    }
    return QFrame::eventFilter(obj, event);
}

void EmojiPopup::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    qApp->installEventFilter(this);
    setFocus(Qt::PopupFocusReason);
    rebuild();
}

void EmojiPopup::hideEvent(QHideEvent* event)
{
    qApp->removeEventFilter(this);
    QFrame::hideEvent(event);
    deleteLater();
}

void EmojiPopup::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QFrame::keyPressEvent(event);
}

void EmojiPopup::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    rebuild();
}

void EmojiPopup::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRect r = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 12, 12);

    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, QColor(238, 254, 255, 248));
    g.setColorAt(1.0, QColor(224, 248, 255, 248));
    p.fillPath(path, g);

    QPen pen(QColor(160, 220, 235, 220));
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.drawPath(path);
}
