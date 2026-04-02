#ifndef TOOLSPAGE_H
#define TOOLSPAGE_H

#include <QWidget>
#include <QTimer>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLineEdit>
#include <QListWidget>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QJsonArray>

class PomodoroTimer : public QWidget {
    Q_OBJECT
public:
    explicit PomodoroTimer(QWidget *parent = nullptr);
private slots:
    void onStartStop();
    void onReset();
    void updateTimer();
private:
    QTimer* _timer;
    int _remainingTime;
    bool _isRunning;
    QLabel* _timeLabel;
    QPushButton* _startBtn;
    const int WORK_TIME = 25 * 60;
};

class TicTacToe : public QWidget {
    Q_OBJECT
public:
    explicit TicTacToe(QWidget *parent = nullptr);
private slots:
    void onBtnClicked();
    void resetGame();
private:
    QPushButton* _buttons[9];
    bool _isXTurn;
    void checkWinner();
};

class QuickNote : public QWidget {
    Q_OBJECT
public:
    explicit QuickNote(QWidget *parent = nullptr);
};

class WeatherWidget : public QWidget {
    Q_OBJECT
public:
    explicit WeatherWidget(QWidget *parent = nullptr);

private slots:
    void onSearchCity();
    void fetchWeatherData(const QString& locationId, const QString& cityName);
    void onLocationFinished(QNetworkReply* reply);
    void onWeatherFinished(QNetworkReply* reply);
    void onForecastFinished(QNetworkReply* reply);
    void onResultSelected(QListWidgetItem* item);

private:
    QLineEdit* _searchEdit;
    QListWidget* _resultsList;
    QLabel* _cityLabel;
    QLabel* _tempLabel;
    QLabel* _weatherLabel;
    QLabel* _iconLabel;
    QLabel* _humidityLabel;
    QLabel* _windLabel;

    QWidget* _forecastContainer;
    QList<QWidget*> _forecastItems;

    QNetworkAccessManager* _networkMgr;
    QString _apiKey;
    QString _apiHost;

    void updateForecastUI(const QJsonArray& dailyArray);
};

class ToolsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ToolsPage(QWidget *parent = nullptr);

private:
    QStackedWidget* _toolStack;
};

#endif // TOOLSPAGE_H
