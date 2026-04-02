#include "toolspage.h"
#include <QGridLayout>
#include <QMessageBox>
#include <QFont>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonArray>
#include <QSslSocket>
#include <QScrollArea>
#include <QScrollBar>

// --- PomodoroTimer Implementation ---
PomodoroTimer::PomodoroTimer(QWidget *parent) : QWidget(parent), _remainingTime(WORK_TIME), _isRunning(false) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(50, 50, 50, 50);
    layout->setAlignment(Qt::AlignCenter);

    QLabel* title = new QLabel("专注时钟 (25min 工作 / 5min 休息)", this);
    title->setStyleSheet("font-size: 18px; color: #333; font-weight: bold;");
    layout->addWidget(title);

    _timeLabel = new QLabel("25:00", this);
    _timeLabel->setStyleSheet("font-size: 72px; color: #00bfff; font-family: 'Consolas', 'Courier New'; margin: 20px;");
    layout->addWidget(_timeLabel);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    _startBtn = new QPushButton("开始", this);
    _startBtn->setFixedSize(100, 40);
    _startBtn->setStyleSheet("QPushButton { background-color: #00bfff; color: white; border-radius: 20px; font-weight: bold; }"
                             "QPushButton:hover { background-color: #009acd; }");
    
    QPushButton* resetBtn = new QPushButton("重置", this);
    resetBtn->setFixedSize(100, 40);
    resetBtn->setStyleSheet("QPushButton { background-color: #f0f0f0; color: #333; border-radius: 20px; font-weight: bold; }"
                            "QPushButton:hover { background-color: #e5e5e5; }");

    btnLayout->addWidget(_startBtn);
    btnLayout->addWidget(resetBtn);
    layout->addLayout(btnLayout);

    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, this, &PomodoroTimer::updateTimer);
    connect(_startBtn, &QPushButton::clicked, this, &PomodoroTimer::onStartStop);
    connect(resetBtn, &QPushButton::clicked, this, &PomodoroTimer::onReset);
}

void PomodoroTimer::onStartStop() {
    if (_isRunning) {
        _timer->stop();
        _startBtn->setText("继续");
    } else {
        _timer->start(1000);
        _startBtn->setText("暂停");
    }
    _isRunning = !_isRunning;
}

void PomodoroTimer::onReset() {
    _timer->stop();
    _isRunning = false;
    _remainingTime = WORK_TIME;
    _startBtn->setText("开始");
    _timeLabel->setText("25:00");
}

void PomodoroTimer::updateTimer() {
    if (_remainingTime > 0) {
        _remainingTime--;
        int mins = _remainingTime / 60;
        int secs = _remainingTime % 60;
        _timeLabel->setText(QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0')));
    } else {
        _timer->stop();
        _isRunning = false;
        QMessageBox::information(this, "时间到", "专注时间结束，休息一下吧！");
        onReset();
    }
}

// --- TicTacToe Implementation ---
TicTacToe::TicTacToe(QWidget *parent) : QWidget(parent), _isXTurn(true) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    QLabel* title = new QLabel("井字棋 (迷你游戏)", this);
    title->setStyleSheet("font-size: 24px; color: #333; font-weight: bold; margin-bottom: 20px;");
    layout->addWidget(title);

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(5);
    for (int i = 0; i < 9; ++i) {
        _buttons[i] = new QPushButton("", this);
        _buttons[i]->setFixedSize(100, 100);
        _buttons[i]->setStyleSheet("QPushButton { background-color: white; border: 2px solid #ddd; font-size: 48px; border-radius: 10px; }"
                                   "QPushButton:hover { background-color: #f9f9f9; }");
        grid->addWidget(_buttons[i], i / 3, i % 3);
        connect(_buttons[i], &QPushButton::clicked, this, &TicTacToe::onBtnClicked);
    }
    layout->addLayout(grid);

    QPushButton* resetBtn = new QPushButton("重新开始", this);
    resetBtn->setFixedSize(150, 40);
    resetBtn->setStyleSheet("QPushButton { background-color: #ff6347; color: white; border-radius: 20px; margin-top: 20px; font-weight: bold; }");
    connect(resetBtn, &QPushButton::clicked, this, &TicTacToe::resetGame);
    layout->addWidget(resetBtn, 0, Qt::AlignCenter);
}

void TicTacToe::onBtnClicked() {
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (btn->text().isEmpty()) {
        btn->setText(_isXTurn ? "X" : "O");
        btn->setStyleSheet(_isXTurn ? "QPushButton { color: #00bfff; background-color: white; border: 2px solid #ddd; font-size: 48px; border-radius: 10px; }" 
                                   : "QPushButton { color: #ff6347; background-color: white; border: 2px solid #ddd; font-size: 48px; border-radius: 10px; }");
        _isXTurn = !_isXTurn;
        checkWinner();
    }
}

void TicTacToe::checkWinner() {
    const int winPatterns[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8}, {0,3,6}, {1,4,7}, {2,5,8}, {0,4,8}, {2,4,6}
    };
    for (auto& pattern : winPatterns) {
        if (!_buttons[pattern[0]]->text().isEmpty() &&
            _buttons[pattern[0]]->text() == _buttons[pattern[1]]->text() &&
            _buttons[pattern[0]]->text() == _buttons[pattern[2]]->text()) {
            QMessageBox::information(this, "获胜", "玩家 " + _buttons[pattern[0]]->text() + " 赢了！");
            resetGame();
            return;
        }
    }
    bool draw = true;
    for (int i = 0; i < 9; ++i) if (_buttons[i]->text().isEmpty()) draw = false;
    if (draw) {
        QMessageBox::information(this, "平局", "握手言和吧！");
        resetGame();
    }
}

void TicTacToe::resetGame() {
    for (int i = 0; i < 9; ++i) {
        _buttons[i]->setText("");
        _buttons[i]->setStyleSheet("QPushButton { background-color: white; border: 2px solid #ddd; font-size: 48px; border-radius: 10px; }");
    }
    _isXTurn = true;
}

#include <QTextEdit>

// --- QuickNote Implementation ---
QuickNote::QuickNote(QWidget *parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 30, 30, 30);

    QLabel* title = new QLabel("灵感随记", this);
    title->setStyleSheet("font-size: 20px; color: #333; font-weight: bold; margin-bottom: 10px;");
    layout->addWidget(title);

    QTextEdit* note = new QTextEdit(this);
    note->setPlaceholderText("在这里记录你的灵感或学习笔记...");
    note->setStyleSheet("QTextEdit { background-color: #fff9e6; border: 1px solid #ffe4b5; border-radius: 10px; padding: 15px; font-size: 16px; color: #5d4037; }"
                        "QTextEdit:focus { border: 1px solid #ffcc80; }");
    layout->addWidget(note);

    QPushButton* saveBtn = new QPushButton("保存到本地", this);
    saveBtn->setFixedSize(120, 35);
    saveBtn->setStyleSheet("QPushButton { background-color: #ffb74d; color: white; border-radius: 17px; margin-top: 10px; font-weight: bold; }");
    layout->addWidget(saveBtn, 0, Qt::AlignRight);
}

// --- WeatherWidget Implementation ---
WeatherWidget::WeatherWidget(QWidget *parent) : QWidget(parent), 
    _apiKey("9418850b2dae4c119a62fe6f0036a9e5"),
    _apiHost("ne3aaqqru5.re.qweatherapi.com") {
    _networkMgr = new QNetworkAccessManager(this);

    // 诊断 SSL
    qDebug() << "[Weather]: SSL Support: " << QSslSocket::supportsSsl();
    qDebug() << "[Weather]: SSL Version: " << QSslSocket::sslLibraryBuildVersionString();

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setAlignment(Qt::AlignCenter);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; } "
                              "QScrollArea > QWidget > QWidget { background: transparent; }");

    QWidget* contentWid = new QWidget(scrollArea);
    QGridLayout* outerLayout = new QGridLayout(contentWid);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    
    // 设置行列比例，使中间部分居中
    outerLayout->setRowStretch(0, 1);
    outerLayout->setRowStretch(2, 1);
    outerLayout->setColumnStretch(0, 1);
    outerLayout->setColumnStretch(2, 1);

    // 内部垂直容器，固定宽度 350，与天气卡片一致
    QWidget* innerContent = new QWidget(contentWid);
    innerContent->setFixedWidth(350);
    QVBoxLayout* layout = new QVBoxLayout(innerContent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(15);

    // 搜索栏
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(10);
    searchLayout->setAlignment(Qt::AlignCenter);

    _searchEdit = new QLineEdit(innerContent);
    _searchEdit->setPlaceholderText("搜索城市 (如: 北京)");
    _searchEdit->setFixedWidth(200);
    _searchEdit->setStyleSheet("QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 15px; background: white; font-size: 14px; }"
                               "QLineEdit:focus { border: 1px solid #4facfe; }");

    QPushButton* searchBtn = new QPushButton("搜索", innerContent);
    searchBtn->setFixedSize(60, 32);
    searchBtn->setStyleSheet("QPushButton { background-color: #4facfe; color: white; border-radius: 15px; font-weight: bold; }"
                             "QPushButton:hover { background-color: #00c6ff; }");

    searchLayout->addWidget(_searchEdit);
    searchLayout->addWidget(searchBtn);
    layout->addLayout(searchLayout);

    // 搜索结果列表
    _resultsList = new QListWidget(innerContent);
    _resultsList->setFixedWidth(260);
    _resultsList->setMaximumHeight(150);
    _resultsList->hide();
    _resultsList->setStyleSheet("QListWidget { background: white; border: 1px solid #ddd; border-radius: 8px; }"
                                "QListWidget::item { padding: 8px; color: #333; border-bottom: 1px solid #f0f0f0; }"
                                "QListWidget::item:last { border-bottom: none; }"
                                "QListWidget::item:hover { background: #e0f0ff; }");
    layout->addWidget(_resultsList, 0, Qt::AlignCenter);

    // 天气卡片
    QWidget* card = new QWidget(innerContent);
    card->setObjectName("weather_card");
    card->setFixedWidth(350);
    card->setStyleSheet("#weather_card { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #4facfe, stop:1 #00f2fe); border-radius: 20px; }");

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 25, 20, 25);
    cardLayout->setSpacing(5);

    _cityLabel = new QLabel("请输入城市", card);
    _cityLabel->setStyleSheet("font-size: 26px; color: white; font-weight: bold;");
    _cityLabel->setAlignment(Qt::AlignCenter);

    _iconLabel = new QLabel(card);
    _iconLabel->setFixedSize(80, 80);
    _iconLabel->setPixmap(QPixmap(":/images/weather.png").scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    _iconLabel->setAlignment(Qt::AlignCenter);

    _tempLabel = new QLabel("--°C", card);
    _tempLabel->setStyleSheet("font-size: 52px; color: white; font-weight: 200;");
    _tempLabel->setAlignment(Qt::AlignCenter);

    _weatherLabel = new QLabel("等待查询...", card);
    _weatherLabel->setStyleSheet("font-size: 18px; color: rgba(255,255,255,0.9);");
    _weatherLabel->setAlignment(Qt::AlignCenter);

    QFrame* line = new QFrame(card);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: rgba(255,255,255,0.3);");

    QHBoxLayout* infoLayout = new QHBoxLayout();
    _humidityLabel = new QLabel("湿度: --%", card);
    _humidityLabel->setStyleSheet("color: white; font-size: 13px;");
    _windLabel = new QLabel("风力: --级", card);
    _windLabel->setStyleSheet("color: white; font-size: 13px;");

    infoLayout->addWidget(_humidityLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(_windLabel);

    cardLayout->addWidget(_cityLabel);
    cardLayout->addWidget(_iconLabel, 0, Qt::AlignCenter);
    cardLayout->addWidget(_tempLabel);
    cardLayout->addWidget(_weatherLabel);
    cardLayout->addSpacing(10);
    cardLayout->addWidget(line);
    cardLayout->addLayout(infoLayout);

    // 7 天预报区域
    _forecastContainer = new QWidget(card);
    _forecastContainer->setStyleSheet("background: transparent;");
    QVBoxLayout* forecastLayout = new QVBoxLayout(_forecastContainer);
    forecastLayout->setContentsMargins(0, 10, 0, 0);
    forecastLayout->setSpacing(8);
    cardLayout->addWidget(_forecastContainer);

    layout->addWidget(card);

    // 提示
    QLabel* tip = new QLabel("数据来源: 和风天气", innerContent);
    tip->setStyleSheet("color: #999; font-size: 11px; margin-top: 5px;");
    layout->addWidget(tip, 0, Qt::AlignCenter);

    // 将内部内容放入网格布局中心
    outerLayout->addWidget(innerContent, 1, 1);

    scrollArea->setWidget(contentWid);
    mainLayout->addWidget(scrollArea);

    // 信号连接
    connect(_searchEdit, &QLineEdit::returnPressed, this, &WeatherWidget::onSearchCity);
    connect(searchBtn, &QPushButton::clicked, this, &WeatherWidget::onSearchCity);
    connect(_resultsList, &QListWidget::itemClicked, this, &WeatherWidget::onResultSelected);

    // 默认查询北京
    _searchEdit->setText("北京");
    onSearchCity();
}

void WeatherWidget::onSearchCity() {
    QString city = _searchEdit->text().trimmed();
    if (city.isEmpty()) return;

    _resultsList->hide();
    _weatherLabel->setText("正在查询...");
    _cityLabel->setText("查询中...");

    // 清空旧预报
    QLayoutItem* item;
    while ((item = _forecastContainer->layout()->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    // 严格按照官方 URL 结构：https://{host}/geo/v2/city/lookup
    QUrl url("https://" + _apiHost + "/geo/v2/city/lookup");
    QUrlQuery query;
    query.addQueryItem("location", city);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "QChat/1.0 (Qt6; Windows)");
    // 严格按照标头方式验证：X-QW-Api-Key
    request.setRawHeader("X-QW-Api-Key", _apiKey.toUtf8());
    
    QNetworkReply* reply = _networkMgr->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onLocationFinished(reply);
    });
}

void WeatherWidget::onLocationFinished(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        _weatherLabel->setText("查询失败: " + reply->errorString());
        qWarning() << "[Weather] 城市查询错误:" << reply->errorString();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    if (obj["code"].toString() != "200") {
        _weatherLabel->setText("未找到该城市");
        qDebug() << "API返回码：" << obj["code"].toString();
        return;
    }

    QJsonArray locationArray = obj["location"].toArray();
    if (locationArray.isEmpty()) {
        _weatherLabel->setText("未找到该城市");
        return;
    }

    if (locationArray.size() > 1) {
        _resultsList->clear();
        for (int i = 0; i < locationArray.size(); ++i) {
            QJsonObject loc = locationArray[i].toObject();
            QString name = loc["name"].toString();
            QString adm1 = loc["adm1"].toString();
            QString adm2 = loc["adm2"].toString();
            QString id = loc["id"].toString();

            QString display = QString("%1, %2, %3").arg(name).arg(adm2).arg(adm1);
            QListWidgetItem* item = new QListWidgetItem(display, _resultsList);
            item->setData(Qt::UserRole, id);
            item->setData(Qt::UserRole + 1, name);
        }
        _resultsList->show();
        _weatherLabel->setText("请选择具体地区");
        return;
    }

    QJsonObject location = locationArray[0].toObject();
    QString locationId = location["id"].toString();
    QString cityName = location["name"].toString();
    fetchWeatherData(locationId, cityName);
}

void WeatherWidget::fetchWeatherData(const QString& locationId, const QString& cityName) {
    _cityLabel->setText(cityName);
    _resultsList->hide();

    // 第二步: 获取实时天气 结构：https://{host}/v7/weather/now
    QUrl urlNow("https://" + _apiHost + "/v7/weather/now");
    QUrlQuery queryNow;
    queryNow.addQueryItem("location", locationId);
    urlNow.setQuery(queryNow);

    QNetworkRequest reqNow(urlNow);
    reqNow.setHeader(QNetworkRequest::UserAgentHeader, "QChat/1.0 (Qt6; Windows)");
    reqNow.setRawHeader("X-QW-Api-Key", _apiKey.toUtf8());
    QNetworkReply* repNow = _networkMgr->get(reqNow);
    connect(repNow, &QNetworkReply::finished, this, [this, repNow]() {
        onWeatherFinished(repNow);
    });

    // 第三步: 获取 7 天预报 结构：https://{host}/v7/weather/7d
    QUrl url7d("https://" + _apiHost + "/v7/weather/7d");
    QUrlQuery query7d;
    query7d.addQueryItem("location", locationId);
    url7d.setQuery(query7d);

    QNetworkRequest req7d(url7d);
    req7d.setHeader(QNetworkRequest::UserAgentHeader, "QChat/1.0 (Qt6; Windows)");
    req7d.setRawHeader("X-QW-Api-Key", _apiKey.toUtf8());
    QNetworkReply* rep7d = _networkMgr->get(req7d);
    connect(rep7d, &QNetworkReply::finished, this, [this, rep7d]() {
        onForecastFinished(rep7d);
    });
}

void WeatherWidget::onResultSelected(QListWidgetItem *item) {
    QString id = item->data(Qt::UserRole).toString();
    QString name = item->data(Qt::UserRole + 1).toString();
    fetchWeatherData(id, name);
}

void WeatherWidget::onWeatherFinished(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        _weatherLabel->setText("实时天气查询失败");
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    if (obj["code"].toString() != "200") return;

    QJsonObject now = obj["now"].toObject();
    _tempLabel->setText(now["temp"].toString() + "°C");
    _weatherLabel->setText(now["text"].toString());
    _humidityLabel->setText("湿度: " + now["humidity"].toString() + "%");
    _windLabel->setText("风力: " + now["windScale"].toString() + "级");
}

void WeatherWidget::onForecastFinished(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) return;

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    if (obj["code"].toString() != "200") return;

    QJsonArray daily = obj["daily"].toArray();
    updateForecastUI(daily);
}

void WeatherWidget::updateForecastUI(const QJsonArray &dailyArray) {
    for (int i = 1; i < dailyArray.size(); ++i) {
        QJsonObject day = dailyArray[i].toObject();
        QString date = day["fxDate"].toString().mid(5);
        QString temp = day["tempMin"].toString() + "~" + day["tempMax"].toString() + "°C";
        QString text = day["textDay"].toString();

        QWidget* itemWid = new QWidget(_forecastContainer);
        QHBoxLayout* itemLayout = new QHBoxLayout(itemWid);
        itemLayout->setContentsMargins(10, 2, 10, 2);

        QLabel* dateLb = new QLabel(date, itemWid);
        dateLb->setStyleSheet("color: rgba(255,255,255,0.8); font-size: 13px;");

        QLabel* textLb = new QLabel(text, itemWid);
        textLb->setStyleSheet("color: white; font-size: 13px; font-weight: bold;");
        textLb->setAlignment(Qt::AlignCenter);

        QLabel* tempLb = new QLabel(temp, itemWid);
        tempLb->setStyleSheet("color: rgba(255,255,255,0.8); font-size: 13px;");
        tempLb->setAlignment(Qt::AlignRight);

        itemLayout->addWidget(dateLb);
        itemLayout->addStretch();
        itemLayout->addWidget(textLb);
        itemLayout->addStretch();
        itemLayout->addWidget(tempLb);

        _forecastContainer->layout()->addWidget(itemWid);
    }
}

// --- ToolsPage Implementation ---

// --- ToolsPage Implementation ---
ToolsPage::ToolsPage(QWidget *parent) : QWidget(parent) {
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 左侧二级菜单
    QWidget* toolSide = new QWidget(this);
    toolSide->setFixedWidth(120);
    toolSide->setStyleSheet("background-color: #f5fcff; border-right: 1px solid #e1e1e1;");
    QVBoxLayout* sideLayout = new QVBoxLayout(toolSide);
    sideLayout->setContentsMargins(10, 20, 10, 20);
    sideLayout->setSpacing(10);

    auto createToolBtn = [&](const QString& text, int index) {
        QPushButton* btn = new QPushButton(text, this);
        btn->setFixedSize(100, 40);
        btn->setStyleSheet("QPushButton { border: none; background: transparent; color: #555; border-radius: 8px; font-weight: bold; }"
                           "QPushButton:hover { background-color: #e0f0ff; color: #00bfff; }"
                           "QPushButton:checked { background-color: #00bfff; color: white; }");
        btn->setCheckable(true);
        if (index == 0) btn->setChecked(true);
        connect(btn, &QPushButton::clicked, this, [this, index, btn]() {
            _toolStack->setCurrentIndex(index);
            // 这里还可以处理互斥逻辑
            for (auto* child : findChildren<QPushButton*>()) {
                if (child != btn && child->isCheckable()) child->setChecked(false);
            }
            btn->setChecked(true);
        });
        return btn;
    };

    sideLayout->addWidget(createToolBtn("专注时钟", 0));
    sideLayout->addWidget(createToolBtn("井字棋", 1));
    sideLayout->addWidget(createToolBtn("灵感笔记", 2));
    sideLayout->addWidget(createToolBtn("天气预报", 3));
    sideLayout->addStretch();
    mainLayout->addWidget(toolSide);

    // 右侧内容堆栈
    _toolStack = new QStackedWidget(this);
    _toolStack->addWidget(new PomodoroTimer(this));
    _toolStack->addWidget(new TicTacToe(this));
    _toolStack->addWidget(new QuickNote(this));
    _toolStack->addWidget(new WeatherWidget(this));
    mainLayout->addWidget(_toolStack);
}
