#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QWidget>
#include <QIcon>

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget* parent = nullptr);
    //设置传输图片点击时的覆盖阴影
    void setIconOverlay(const QIcon& icon);
    //显示阴影
    void showIconOverlay(bool show);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

signals:
    void clicked();

private:
    QIcon m_overlayIcon;
    bool m_showOverlay;
    bool m_hovered;
};
#endif // CLICKABLELABEL_H
