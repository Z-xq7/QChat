#ifndef IMAGEVIEWERDIALOG_H
#define IMAGEVIEWERDIALOG_H

#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QToolButton>
#include <QLabel>

class ZoomGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ZoomGraphicsView(QWidget* parent = nullptr);

    void setZoomRange(double minFactor, double maxFactor);
    double zoomFactor() const;
    void setZoomFactor(double factor);

signals:
    void zoomFactorChanged(double factor);

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    double m_minFactor;
    double m_maxFactor;
};

class ImageViewerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ImageViewerDialog(QWidget* parent = nullptr);

    void setImage(const QString& filePath, const QPixmap& fallback);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void zoomFit();
    void updateZoomLabel(double factor);

private:
    void applyPixmap(const QPixmap& pix);

private:
    ZoomGraphicsView* m_view;
    QGraphicsScene* m_scene;
    QGraphicsPixmapItem* m_item;
    QWidget* m_bottomBar;
    QToolButton* m_zoomOutBtn;
    QToolButton* m_zoomInBtn;
    QToolButton* m_resetBtn;
    QToolButton* m_fitBtn;
    QLabel* m_zoomLabel;
    QPixmap m_pixmap;
    bool m_pendingFit;
};

#endif // IMAGEVIEWERDIALOG_H
