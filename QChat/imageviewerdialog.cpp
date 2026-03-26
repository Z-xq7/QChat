#include "imageviewerdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QShowEvent>
#include <QFileInfo>
#include <QPainter>
#include <QTimer>

static QIcon makePlusIcon()
{
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor("#f5f5f5"));
    pen.setWidthF(2.2);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(QPointF(12, 6.5), QPointF(12, 17.5));
    p.drawLine(QPointF(6.5, 12), QPointF(17.5, 12));
    return QIcon(pix);
}

static QIcon makeMinusIcon()
{
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor("#f5f5f5"));
    pen.setWidthF(2.2);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(QPointF(6.5, 12), QPointF(17.5, 12));
    return QIcon(pix);
}

ZoomGraphicsView::ZoomGraphicsView(QWidget* parent)
    : QGraphicsView(parent)
    , m_minFactor(0.05)
    , m_maxFactor(20.0)
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ZoomGraphicsView::setZoomRange(double minFactor, double maxFactor)
{
    m_minFactor = minFactor;
    m_maxFactor = maxFactor;
}

double ZoomGraphicsView::zoomFactor() const
{
    return transform().m11();
}

void ZoomGraphicsView::setZoomFactor(double factor)
{
    if (factor < m_minFactor) factor = m_minFactor;
    if (factor > m_maxFactor) factor = m_maxFactor;

    QTransform t;
    t.scale(factor, factor);
    setTransform(t);
    emit zoomFactorChanged(zoomFactor());
}

void ZoomGraphicsView::wheelEvent(QWheelEvent* event)
{
    const QPoint angleDelta = event->angleDelta();
    if (angleDelta.isNull()) {
        event->ignore();
        return;
    }

    const double step = (angleDelta.y() > 0) ? 1.15 : (1.0 / 1.15);
    const double current = zoomFactor();
    double next = current * step;
    if (next < m_minFactor) next = m_minFactor;
    if (next > m_maxFactor) next = m_maxFactor;

    if (qFuzzyCompare(current, next)) {
        event->accept();
        return;
    }

    setZoomFactor(next);
    event->accept();
}

ImageViewerDialog::ImageViewerDialog(QWidget* parent)
    : QDialog(parent)
    , m_view(new ZoomGraphicsView(this))
    , m_scene(new QGraphicsScene(this))
    , m_item(new QGraphicsPixmapItem())
    , m_bottomBar(new QWidget(this))
    , m_zoomOutBtn(new QToolButton(this))
    , m_zoomInBtn(new QToolButton(this))
    , m_resetBtn(new QToolButton(this))
    , m_fitBtn(new QToolButton(this))
    , m_zoomLabel(new QLabel(this))
    , m_pendingFit(false)
{
    setWindowFlags(Qt::Window);
    setMinimumSize(540, 620);

    m_bottomBar->setObjectName("bottomBar");

    setStyleSheet(
        "QDialog{background-color:#111111;}"
        "QWidget{color:#eaeaea;}"
        "QWidget#bottomBar{background-color:#161616;}"
        "QToolButton{background-color:#2a2a2a;border:1px solid #3a3a3a;border-radius:6px;padding:6px;color:#f5f5f5;}"
        "QToolButton:hover{background-color:#343434;}"
        "QToolButton:pressed{background-color:#3c3c3c;}"
        "QLabel{color:#eaeaea;}"
    );

    m_scene->addItem(m_item);
    m_view->setScene(m_scene);
    m_view->setBackgroundBrush(QBrush(QColor("#111111")));

    m_zoomOutBtn->setIcon(makeMinusIcon());
    m_zoomInBtn->setIcon(makePlusIcon());
    m_zoomOutBtn->setIconSize(QSize(18, 18));
    m_zoomInBtn->setIconSize(QSize(18, 18));
    m_resetBtn->setText("1:1");
    m_fitBtn->setText("适合窗口");
    m_zoomLabel->setText("100%");
    m_zoomLabel->setMinimumWidth(52);
    m_zoomLabel->setAlignment(Qt::AlignCenter);

    connect(m_zoomInBtn, &QToolButton::clicked, this, &ImageViewerDialog::zoomIn);
    connect(m_zoomOutBtn, &QToolButton::clicked, this, &ImageViewerDialog::zoomOut);
    connect(m_resetBtn, &QToolButton::clicked, this, &ImageViewerDialog::zoomReset);
    connect(m_fitBtn, &QToolButton::clicked, this, &ImageViewerDialog::zoomFit);
    connect(m_view, &ZoomGraphicsView::zoomFactorChanged, this, &ImageViewerDialog::updateZoomLabel);

    auto* barLayout = new QHBoxLayout(m_bottomBar);
    barLayout->setContentsMargins(16, 10, 16, 10);
    barLayout->setSpacing(10);
    barLayout->addStretch();
    barLayout->addWidget(m_zoomOutBtn);
    barLayout->addWidget(m_zoomInBtn);
    barLayout->addWidget(m_zoomLabel);
    barLayout->addWidget(m_resetBtn);
    barLayout->addWidget(m_fitBtn);
    barLayout->addStretch();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_view, 1);
    root->addWidget(m_bottomBar, 0);
}

void ImageViewerDialog::setImage(const QString& filePath, const QPixmap& fallback)
{
    QPixmap pix;
    if (!filePath.isEmpty() && QFileInfo::exists(filePath)) {
        pix.load(filePath);
        if (!pix.isNull()) {
            setWindowTitle(QFileInfo(filePath).fileName());
        }
    }
    if (pix.isNull()) {
        pix = fallback;
    }
    applyPixmap(pix);
    m_pendingFit = true;
    if (isVisible()) {
        QTimer::singleShot(0, this, &ImageViewerDialog::zoomFit);
        m_pendingFit = false;
    }
}

void ImageViewerDialog::applyPixmap(const QPixmap& pix)
{
    m_pixmap = pix;
    m_item->setPixmap(m_pixmap);
    m_scene->setSceneRect(m_item->boundingRect());
    m_view->centerOn(m_item);
}

void ImageViewerDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_pendingFit) {
        QTimer::singleShot(0, this, &ImageViewerDialog::zoomFit);
        m_pendingFit = false;
    }
}

void ImageViewerDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
}

void ImageViewerDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QDialog::keyPressEvent(event);
}

void ImageViewerDialog::zoomIn()
{
    m_view->setZoomFactor(m_view->zoomFactor() * 1.15);
}

void ImageViewerDialog::zoomOut()
{
    m_view->setZoomFactor(m_view->zoomFactor() / 1.15);
}

void ImageViewerDialog::zoomReset()
{
    m_view->setZoomFactor(1.0);
    m_view->centerOn(m_item);
}

void ImageViewerDialog::zoomFit()
{
    if (m_pixmap.isNull()) return;
    m_view->fitInView(m_item, Qt::KeepAspectRatio);
    updateZoomLabel(m_view->zoomFactor());
}

void ImageViewerDialog::updateZoomLabel(double factor)
{
    const int percent = qRound(factor * 100.0);
    m_zoomLabel->setText(QString::number(percent) + "%");
}
