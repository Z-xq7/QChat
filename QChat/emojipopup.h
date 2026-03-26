#ifndef EMOJIPOPUP_H
#define EMOJIPOPUP_H

#include <QFrame>
#include <QStringList>

class QScrollArea;
class QTimer;

class EmojiPopup : public QFrame
{
    Q_OBJECT
public:
    explicit EmojiPopup(QWidget* parent = nullptr);
    void setEmojis(const QStringList& emojis);

signals:
    void emojiSelected(const QString& emoji);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void rebuild();

private:
    QStringList m_emojis;
    QScrollArea* m_scrollArea;
    QWidget* m_gridHost;
    QTimer* m_scrollbarTimer;
};

#endif // EMOJIPOPUP_H
