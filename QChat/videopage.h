#ifndef VIDEOPAGE_H
#define VIDEOPAGE_H

#include <QWidget>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QList>
#include <QPropertyAnimation>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QMenu>
#include "clickedlabel.h"

class VideoItem : public QWidget {
    Q_OBJECT
public:
    explicit VideoItem(const QString& videoPath, const QString& title, QWidget *parent = nullptr);

    void play();
    void stop();
    void pause();

    QString videoPath() const { return _videoPath; }

    void applyState(bool liked, bool saved, int likeCount = 0, int commentCount = 0, int saveCount = 0);
    QJsonObject toStateJson() const;

signals:
    void stateChanged();

private slots:
    void onDurationChanged(qint64 duration);
    void onPositionChanged(qint64 position);
    void onSliderMoved(int position);

    void onLikeToggled(bool checked);
    void onSaveToggled(bool checked);
    void onShareClicked();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void updateCounterLabels();

    QString _videoPath;

    QMediaPlayer* _player{};
    QVideoWidget* _videoWidget{};
    QAudioOutput* _audioOutput{};

    bool _isLiked{false};
    bool _isSaved{false};

    int _likeCount{0};
    int _commentCount{0};
    int _saveCount{0};

    // UI
    QPushButton* _likeBtn{};
    QPushButton* _commentBtn{};
    QPushButton* _saveBtn{};
    QPushButton* _shareBtn{};

    QLabel* _likeCountLabel{};
    QLabel* _commentCountLabel{};
    QLabel* _saveCountLabel{};

    QLabel* _avatarLabel{};
    QLabel* _userLabel{};
    QLabel* _titleLabel{};

    QSlider* _progressBar{};
};

class VideoPage : public QWidget {
    Q_OBJECT
public:
    explicit VideoPage(QWidget *parent = nullptr);
    void playCurrent();
    void stopAll();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void slot_scroll_value_changed(int value);
    void onItemStateChanged();

private:
    QString stateFilePath() const;
    void loadStates();
    void saveStates() const;

    QScrollArea* _scrollArea{};
    QWidget* _contentWidget{};
    QVBoxLayout* _contentLayout{};
    QList<VideoItem*> _videoItems;
    int _currentIndex{0};
    QTimer* _scrollTimer{};
    bool _bScrolling{false};

    QHash<QString, QJsonObject> _stateByPath;

    void addVideo(const QString& path, const QString& title);
    void scrollToItem(int index);
};

#endif // VIDEOPAGE_H
