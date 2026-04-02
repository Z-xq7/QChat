#include "videopage.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStandardPaths>
#include <QUrl>
#include <QWheelEvent>
#include <QJsonArray>

namespace {
class BottomScrimWidget final : public QWidget {
public:
    explicit BottomScrimWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0.0, QColor(0, 0, 0, 0));
        g.setColorAt(0.4, QColor(0, 0, 0, 50));
        g.setColorAt(1.0, QColor(0, 0, 0, 180));
        p.fillRect(rect(), g);
    }
};

static QString floatingIconButtonStyle(const QString& normal, const QString& checked = QString()) {
    QString s;
    s += "QPushButton { background-color: transparent; border: none; padding:0px; }";
    s += "QPushButton:hover { background-color: transparent; }";
    s += "QPushButton:pressed { background-color: transparent; }";
    s += "QPushButton:focus { outline: none; }";
    s += QString("QPushButton { border-image: url(%1); }").arg(normal);
    if (!checked.isEmpty()) {
        s += QString("QPushButton:checked { border-image: url(%1); }").arg(checked);
    }
    return s;
}

static QString counterLabelStyle() {
    return "QLabel{background-color: transparent;color:white;font-size:12px;font-weight:600;}";
}

static QString parseUserFromTitle(const QString& title) {
    // 约定：title 形如 "@xxx" 或 "xxx"
    return title.trimmed().startsWith('@') ? title.trimmed() : ("@" + title.trimmed());
}

static QString parseDescFromTitle(const QString& title) {
    // 目前只有 title，一个简单的占位描述
    return QStringLiteral("精彩内容，关注了解更多");
}

static QString formatCount(int n) {
    if (n >= 10000) {
        return QString::number(n / 10000.0, 'f', 1) + "w";
    }
    return QString::number(n);
}
} // namespace

// --- VideoItem Implementation ---
VideoItem::VideoItem(const QString& videoPath, const QString& title, QWidget *parent)
    : QWidget(parent), _videoPath(videoPath)
{
    _player = new QMediaPlayer(this);
    _audioOutput = new QAudioOutput(this);
    _player->setAudioOutput(_audioOutput);

    _videoWidget = new QVideoWidget(this);
    _videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    _player->setVideoOutput(_videoWidget);

    _player->setSource(QUrl::fromLocalFile(videoPath));
    _audioOutput->setVolume(0.5f);
    _player->stop();

    connect(_player, &QMediaPlayer::errorOccurred,
            [videoPath](QMediaPlayer::Error error, const QString &errorString) {
                Q_UNUSED(error);
                qDebug() << "Video Error [" << videoPath << "]:" << errorString;
            });

    connect(_player, &QMediaPlayer::mediaStatusChanged, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            _player->play();
        }
    });

    _videoWidget->show();
    _videoWidget->lower();

    // 底部渐变遮罩：改为挂在 _videoWidget 上，这样透明区域下面是真正的视频
    auto scrim = new BottomScrimWidget(_videoWidget);
    scrim->setObjectName("bottomScrim");
    scrim->raise();

    // 右侧按钮：全部改为 _videoWidget 的子控件
    _likeBtn = new QPushButton(_videoWidget);
    _likeBtn->setFixedSize(45, 45);
    _likeBtn->setCursor(Qt::PointingHandCursor);
    _likeBtn->setCheckable(true);
    _likeBtn->setStyleSheet(floatingIconButtonStyle(":/images/like.png", ":/images/like_press.png"));
    _likeBtn->raise();

    _likeCountLabel = new QLabel(_videoWidget);
    _likeCountLabel->setStyleSheet(counterLabelStyle());
    _likeCountLabel->setAlignment(Qt::AlignHCenter);
    _likeCountLabel->raise();

    _commentBtn = new QPushButton(_videoWidget);
    _commentBtn->setFixedSize(45, 45);
    _commentBtn->setCursor(Qt::PointingHandCursor);
    _commentBtn->setStyleSheet(floatingIconButtonStyle(":/images/comment.png"));
    _commentBtn->raise();

    _commentCountLabel = new QLabel(_videoWidget);
    _commentCountLabel->setStyleSheet(counterLabelStyle());
    _commentCountLabel->setAlignment(Qt::AlignHCenter);
    _commentCountLabel->raise();

    _saveBtn = new QPushButton(_videoWidget);
    _saveBtn->setFixedSize(45, 45);
    _saveBtn->setCursor(Qt::PointingHandCursor);
    _saveBtn->setCheckable(true);
    _saveBtn->setStyleSheet(floatingIconButtonStyle(":/images/save.png", ":/images/save_press.png"));
    _saveBtn->raise();

    _saveCountLabel = new QLabel(_videoWidget);
    _saveCountLabel->setStyleSheet(counterLabelStyle());
    _saveCountLabel->setAlignment(Qt::AlignHCenter);
    _saveCountLabel->raise();

    _shareBtn = new QPushButton(_videoWidget);
    _shareBtn->setFixedSize(45, 45);
    _shareBtn->setCursor(Qt::PointingHandCursor);
    _shareBtn->setStyleSheet(floatingIconButtonStyle(":/images/share.png"));
    _shareBtn->raise();

    // 左下：头像/用户名/描述，同样作为视频控件的子控件
    _avatarLabel = new QLabel(_videoWidget);
    _avatarLabel->setFixedSize(44, 44);
    _avatarLabel->setStyleSheet("QLabel{background-color: rgba(255,255,255,30); border-radius: 22px;}");
    _avatarLabel->raise();

    _userLabel = new QLabel(parseUserFromTitle(title), _videoWidget);
    _userLabel->setStyleSheet("QLabel{background-color: transparent;color:white;font-size:16px;font-weight:700;}");
    _userLabel->raise();

    _titleLabel = new QLabel(parseDescFromTitle(title), _videoWidget);
    _titleLabel->setStyleSheet("QLabel{background-color: transparent;color:white;font-size:14px;font-weight:500;}");
    _titleLabel->setWordWrap(true);
    _titleLabel->raise();

    // 进度条也挂在 _videoWidget 上
    _progressBar = new QSlider(Qt::Horizontal, _videoWidget);
    _progressBar->setRange(0, 1000);
    _progressBar->setFixedHeight(12);
    _progressBar->setCursor(Qt::PointingHandCursor);
    _progressBar->setStyleSheet(
        "QSlider::groove:horizontal { border: none; height: 2px; background: rgba(255, 255, 255, 60); }"
        "QSlider::handle:horizontal { background: white; width: 10px; height: 10px; margin: -4px 0; border-radius: 5px; }"
        "QSlider::sub-page:horizontal { background: white; }");
    _progressBar->raise();

    connect(_player, &QMediaPlayer::durationChanged, this, &VideoItem::onDurationChanged);
    connect(_player, &QMediaPlayer::positionChanged, this, &VideoItem::onPositionChanged);
    connect(_progressBar, &QSlider::sliderMoved, this, &VideoItem::onSliderMoved);
    connect(_progressBar, &QSlider::sliderPressed, [this]() { _player->pause(); });
    connect(_progressBar, &QSlider::sliderReleased, [this]() { _player->play(); });

    connect(_likeBtn, &QPushButton::toggled, this, &VideoItem::onLikeToggled);
    connect(_saveBtn, &QPushButton::toggled, this, &VideoItem::onSaveToggled);
    connect(_shareBtn, &QPushButton::clicked, this, &VideoItem::onShareClicked);

    // 初始计数
    if (_likeCount == 0) _likeCount = 123;
    if (_commentCount == 0) _commentCount = 45;
    if (_saveCount == 0) _saveCount = 9;
    updateCounterLabels();
}

void VideoItem::applyState(bool liked, bool saved, int likeCount, int commentCount, int saveCount) {
    _isLiked = liked;
    _isSaved = saved;

    if (likeCount > 0) _likeCount = likeCount;
    if (commentCount > 0) _commentCount = commentCount;
    if (saveCount > 0) _saveCount = saveCount;

    // 避免触发 toggled 两次
    _likeBtn->blockSignals(true);
    _saveBtn->blockSignals(true);
    _likeBtn->setChecked(_isLiked);
    _saveBtn->setChecked(_isSaved);
    _likeBtn->blockSignals(false);
    _saveBtn->blockSignals(false);

    updateCounterLabels();
}

QJsonObject VideoItem::toStateJson() const {
    QJsonObject o;
    o["path"] = _videoPath;
    o["liked"] = _isLiked;
    o["saved"] = _isSaved;
    o["likeCount"] = _likeCount;
    o["commentCount"] = _commentCount;
    o["saveCount"] = _saveCount;
    return o;
}

void VideoItem::updateCounterLabels() {
    _likeCountLabel->setText(formatCount(_likeCount));
    _commentCountLabel->setText(formatCount(_commentCount));
    _saveCountLabel->setText(formatCount(_saveCount));
}

void VideoItem::onLikeToggled(bool checked) {
    _isLiked = checked;
    _likeCount = qMax(0, _likeCount + (checked ? 1 : -1));
    updateCounterLabels();
    emit stateChanged();
}

void VideoItem::onSaveToggled(bool checked) {
    _isSaved = checked;
    _saveCount = qMax(0, _saveCount + (checked ? 1 : -1));
    updateCounterLabels();
    emit stateChanged();
}

void VideoItem::onShareClicked() {
    QMenu menu(this);
    auto copyPath = menu.addAction(QStringLiteral("复制视频路径"));
    auto openDir = menu.addAction(QStringLiteral("打开所在文件夹"));

    QAction* act = menu.exec(QCursor::pos());
    if (!act) return;

    if (act == copyPath) {
        if (auto cb = QGuiApplication::clipboard()) {
            cb->setText(QDir::toNativeSeparators(_videoPath));
        }
    } else if (act == openDir) {
        QFileInfo info(_videoPath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
    }
}

void VideoItem::onDurationChanged(qint64 duration) {
    if (duration > 0) {
        _progressBar->setRange(0, static_cast<int>(duration));
    }
}

void VideoItem::onPositionChanged(qint64 position) {
    if (!_progressBar->isSliderDown()) {
        _progressBar->setValue(static_cast<int>(position));
    }
}

void VideoItem::onSliderMoved(int position) {
    _player->setPosition(position);
}

void VideoItem::play() {
    if (_player->playbackState() != QMediaPlayer::PlayingState) {
        _player->play();
    }
}

void VideoItem::stop() {
    _player->stop();
}

void VideoItem::pause() {
    _player->pause();
}

void VideoItem::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    _videoWidget->setGeometry(rect());

    const int scrimH = 150;
    if (auto scrim = _videoWidget->findChild<QWidget*>("bottomScrim")) {
        scrim->setGeometry(0, height() - scrimH, width(), scrimH);
        scrim->raise();
    }

    const int rightMargin = 20;
    const int bottomMargin = 110;
    const int btn = 45;
    const int gap = 22;
    const int labelH = 16;

    int x = width() - rightMargin - btn;
    int y = height() - bottomMargin - (btn * 4 + gap * 3 + labelH * 3);
    y = qMax(40, y);

    auto placeButtonWithLabel = [&](QPushButton* b, QLabel* l, int row) {
        int by = y + row * (btn + gap + labelH);
        b->move(x, by);

        l->adjustSize();
        int lx = x + (btn - l->width())/2;
        int ly = by + btn;
        l->move(lx, ly);
    };

    placeButtonWithLabel(_likeBtn, _likeCountLabel, 0);
    placeButtonWithLabel(_commentBtn, _commentCountLabel, 1);
    placeButtonWithLabel(_saveBtn, _saveCountLabel, 2);

    int shareY = y + 3 * (btn + gap + labelH);
    _shareBtn->move(x, shareY);

    const int leftMargin = 20;
    int infoBaseY = height() - 120;
    _avatarLabel->move(leftMargin, infoBaseY);
    _userLabel->move(leftMargin + 54, infoBaseY + 2);
    _titleLabel->setFixedWidth(width() - leftMargin * 2 - 140);
    _titleLabel->move(leftMargin + 54, infoBaseY + 26);

    _progressBar->setGeometry(0, height() - 12, width(), 12);
    _videoWidget->lower();
    _progressBar->raise();
}

void VideoItem::mousePressEvent(QMouseEvent* event) {
    if (_likeBtn->geometry().contains(event->pos()) ||
        _commentBtn->geometry().contains(event->pos()) ||
        _saveBtn->geometry().contains(event->pos()) ||
        _shareBtn->geometry().contains(event->pos()) ||
        _progressBar->geometry().contains(event->pos())) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (_player->playbackState() == QMediaPlayer::PlayingState) {
        _player->pause();
    } else {
        _player->play();
    }
}

bool VideoItem::eventFilter(QObject *obj, QEvent *event) {
    return QWidget::eventFilter(obj, event);
}

// --- VideoPage Implementation ---
VideoPage::VideoPage(QWidget *parent) : QWidget(parent), _currentIndex(0), _bScrolling(false) {
    _scrollTimer = new QTimer(this);
    _scrollTimer->setSingleShot(true);
    connect(_scrollTimer, &QTimer::timeout, [this]() { _bScrolling = false; });

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    _scrollArea = new QScrollArea(this);
    _scrollArea->setWidgetResizable(false);
    _scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scrollArea->verticalScrollBar()->setStyleSheet("QScrollBar:vertical { width: 0px; }");
    _scrollArea->setStyleSheet("QScrollArea { border: none; background: black; }");
    _scrollArea->viewport()->installEventFilter(this);

    _contentWidget = new QWidget(_scrollArea);
    _contentWidget->setStyleSheet("background: black;");
    _contentLayout = new QVBoxLayout(_contentWidget);
    _contentLayout->setContentsMargins(0, 0, 0, 0);
    _contentLayout->setSpacing(0);

//     QString videoDir = qApp->applicationDirPath() + "/video";
// #ifdef QT_DEBUG
//     QDir dir(videoDir);
//     if (!dir.exists()) {
//         videoDir = "E:/VS_QChat/QChat/video";
//     }
// #endif
    QString videoDir = "E:/VS_QChat/QChat/video";

    addVideo(videoDir + "/v1.mp4", "@古风合集");
    addVideo(videoDir + "/v2.mp4", "@对对子战神");
    addVideo(videoDir + "/v3.mp4", "@国服三连");
    addVideo(videoDir + "/v4.mp4", "@法老金字塔");
    addVideo(videoDir + "/v5.mp4", "@纸巾还是萝卜");

    _scrollArea->setWidget(_contentWidget);
    mainLayout->addWidget(_scrollArea);

    loadStates();
}

QString VideoPage::stateFilePath() const {
    // 需求：程序目录下 ./video/video_state.json
    //const QString dir = qApp->applicationDirPath() + "/video";
    const QString dir = "E:/VS_QChat/QChat/video";
    QDir().mkpath(dir);
    return dir + "/video_state.json";
}

void VideoPage::loadStates() {
    _stateByPath.clear();

    QFile f(stateFilePath());
    if (!f.exists()) return;
    if (!f.open(QIODevice::ReadOnly)) return;

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;

    const auto root = doc.object();
    const auto arr = root.value("items").toArray();
    for (const auto& v : arr) {
        const auto o = v.toObject();
        const auto path = o.value("path").toString();
        if (!path.isEmpty()) {
            _stateByPath.insert(path, o);
        }
    }

    for (auto* item : _videoItems) {
        auto it = _stateByPath.constFind(item->videoPath());
        if (it != _stateByPath.constEnd()) {
            const auto o = it.value();
            item->applyState(o.value("liked").toBool(),
                             o.value("saved").toBool(),
                             o.value("likeCount").toInt(),
                             o.value("commentCount").toInt(),
                             o.value("saveCount").toInt());
        }
    }
}

void VideoPage::saveStates() const {
    QJsonArray arr;
    for (auto* item : _videoItems) {
        arr.append(item->toStateJson());
    }

    QJsonObject root;
    root["version"] = 1;
    root["updatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["items"] = arr;

    QFile f(stateFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void VideoPage::onItemStateChanged() {
    saveStates();
}

void VideoPage::addVideo(const QString& path, const QString& title) {
    VideoItem* item = new VideoItem(path, title, _contentWidget);
    _contentLayout->addWidget(item);
    _videoItems.append(item);
    connect(item, &VideoItem::stateChanged, this, &VideoPage::onItemStateChanged);

    // 如已有状态，立即应用
    auto it = _stateByPath.constFind(path);
    if (it != _stateByPath.constEnd()) {
        const auto o = it.value();
        item->applyState(o.value("liked").toBool(),
                         o.value("saved").toBool(),
                         o.value("likeCount").toInt(),
                         o.value("commentCount").toInt(),
                         o.value("saveCount").toInt());
    }
}

void VideoPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    playCurrent();
}

void VideoPage::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    stopAll();
}

void VideoPage::playCurrent() {
    if (_currentIndex >= 0 && _currentIndex < _videoItems.size()) {
        _videoItems[_currentIndex]->play();
    }
}

void VideoPage::stopAll() {
    for (auto item : _videoItems) {
        item->stop();
    }
}

void VideoPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (_videoItems.isEmpty()) return;

    _contentWidget->setFixedSize(width(), height() * _videoItems.size());
    for (auto item : _videoItems) {
        item->setFixedSize(size());
    }

    int targetY = _currentIndex * height();
    _scrollArea->verticalScrollBar()->setValue(targetY);
}

void VideoPage::wheelEvent(QWheelEvent *event) {
    if (_bScrolling) return;

    if (event->angleDelta().y() < 0) {
        if (_currentIndex < _videoItems.size() - 1) {
            _bScrolling = true;
            scrollToItem(_currentIndex + 1);
            _scrollTimer->start(600);
        }
    } else {
        if (_currentIndex > 0) {
            _bScrolling = true;
            scrollToItem(_currentIndex - 1);
            _scrollTimer->start(600);
        }
    }
    event->accept();
}

bool VideoPage::eventFilter(QObject *obj, QEvent *event) {
    if (obj == _scrollArea->viewport() && event->type() == QEvent::Wheel) {
        this->wheelEvent(static_cast<QWheelEvent*>(event));
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void VideoPage::scrollToItem(int index) {
    if (index < 0 || index >= _videoItems.size()) {
        _bScrolling = false;
        return;
    }

    _videoItems[_currentIndex]->stop();
    _currentIndex = index;

    int targetY = index * height();

    QPropertyAnimation* animation = new QPropertyAnimation(_scrollArea->verticalScrollBar(), "value");
    animation->setDuration(400);
    animation->setStartValue(_scrollArea->verticalScrollBar()->value());
    animation->setEndValue(targetY);
    animation->setEasingCurve(QEasingCurve::OutQuad);

    connect(animation, &QPropertyAnimation::finished, [this, animation]() {
        _videoItems[_currentIndex]->play();
        _bScrolling = false;
        animation->deleteLater();
    });

    animation->start();
}

void VideoPage::slot_scroll_value_changed(int value) {
    Q_UNUSED(value);
}
