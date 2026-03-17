#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QTimer>
#include <QPixmap>

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);
    void setWindowTitle(const QString &title);
    void setWindowIcon(const QIcon &icon);
    void setTheme(const QString &theme); // 设置主题
    void updateBackground();
    void updateAnimation();
    
    // 使标题栏可以被拖动
    void setDraggable(bool draggable) { m_draggable = draggable; }
    bool isDraggable() const { return m_draggable; }

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onMinimizeClicked();
    void onMaximizeClicked();
    void onCloseClicked();

private:
    void updateButtonStyle(); // 更新按钮样式
    void updateMaximizeButton();

private:
    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QPushButton *m_minimizeBtn;
    QPushButton *m_maximizeBtn;
    QPushButton *m_closeBtn;
    QHBoxLayout *m_layout;
    
    QPoint m_dragPosition;
    bool m_dragging;
    bool m_draggable;
    QString m_theme; // 记录当前主题

    // 动态背景相关
    QTimer* _ani_timer;
    int _ani_offset;
    QPixmap _bg_pixmap;
    
    // 为聊天主题创建的背景
    void updateChatBackground();
};

#endif // TITLEBAR_H
