# QChat 登录界面动态背景实现指南

## 概述
本指南将详细介绍如何为 QChat 项目的 LoginDialog 添加类似 QQ 登录界面的动态背景效果。该效果包括渐变色背景、动态波浪效果和漂浮动画元素。

## 实现步骤

### 步骤 1：修改头文件 (logindialog.h)

将以下内容替换原有头文件内容：

```cpp
#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QPixmap>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include "global.h"
#include "httpmgr.h"

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Ui::LoginDialog *ui;
    //头像框
    void initHead();
    void AddTipErr(TipErr te,QString tips);
    void DelTipErr(TipErr te);
    QMap<TipErr,QString> _tip_errs;
    void showTip(QString str,bool b_ok);
    bool checkUserValid();
    bool checkPwdValid();
    //允许、禁止点击按钮
    bool enableBtn(bool enabled);

    //处理http请求
    void initHttpHandler();
    QMap<ReqId,std::function<void(const QJsonObject&)>> _handlers;
    //缓存用户uid、token
    int _uid;
    QString _token;
    
    // 动态背景相关
    QTimer* _ani_timer;
    int _ani_offset;
    QPixmap _bg_pixmap;
    void updateBackground();

public slots:
    void slot_forget_pwd();

signals:
    //切换到注册页面
    void switchRegister();
    //切换到重置密码页面
    void switchReset();
    //连接聊天服务器（基于tcp长连接）
    void sig_connect_tcp(ServerInfo);

private slots:
    void on_login_btn_clicked();
    void slot_login_mod_finish(ReqId id,QString res,ErrorCodes err);
    void slot_tcp_con_finish(bool bsuccess);
    void slot_login_failed(int);
    void updateAnimation();
};

#endif // LOGINDIALOG_H
```

### 步骤 2：修改实现文件 (logindialog.cpp)

将以下内容替换原有实现文件内容：

```cpp
#include "logindialog.h"
#include "ui_logindialog.h"
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTimer>
#include "tcpmgr.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
    , _ani_timer(nullptr)
    , _ani_offset(0)
{
    ui->setupUi(this);
    
    // 设置窗口标志，保持透明背景
    setAttribute(Qt::WA_TranslucentBackground, true);
    
    // 创建动画定时器
    _ani_timer = new QTimer(this);
    connect(_ani_timer, &QTimer::timeout, this, &LoginDialog::updateAnimation);
    _ani_timer->start(50); // 每50ms更新一次动画，约20fps
    
    connect(ui->reg_btn,&QPushButton::clicked,this,&LoginDialog::switchRegister);
    //重置密码
    ui->forget_label->SetState("normal","hover","","selected","selected_hover","");
    connect(ui->forget_label, &ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);
    //初始化头像
    initHead();
    //初始化http连接
    initHttpHandler();
    //连接登录回包信号
    connect(HttpMgr::GetInstance().get(),&HttpMgr::sig_login_mod_finish,this,&LoginDialog::slot_login_mod_finish);

    //连接tcp连接请求的信号和槽函数
    connect(this,&LoginDialog::sig_connect_tcp,TcpMgr::GetInstance().get(),&TcpMgr::slot_tcp_connect);

    //连接tcp管理者发出的连接成功信号
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_con_success,this,&LoginDialog::slot_tcp_con_finish);

    //连接tcp管理者发出的登录失败信号
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_login_failed,this,&LoginDialog::slot_login_failed);

    //设置浮动显示手形状
    ui->pass_visible->setCursor(Qt::PointingHandCursor);

    ui->pass_visible->SetState("unvisible","unvisible_hover","","visible",
                               "visible_hover","");

    //设置密码不可见
    ui->pass_edit->setEchoMode(QLineEdit::Password);

    //点击事件,设置密码可见性
    connect(ui->pass_visible, &ClickedLabel::clicked, this, [this]() {
        // 获取当前的echoMode状态，而不是按钮状态
        if(ui->pass_edit->echoMode() == QLineEdit::Password) {
            // 当前是密码模式，切换到正常模式
            ui->pass_edit->setEchoMode(QLineEdit::Normal);
        } else {
            // 当前是正常模式，切换到密码模式
            ui->pass_edit->setEchoMode(QLineEdit::Password);
        }
        qDebug() << "Label was clicked!";
    });
    
    // 初始化背景
    updateBackground();
}

LoginDialog::~LoginDialog()
{
    qDebug() << "LoginDialog distructed ...";
    if (_ani_timer) {
        _ani_timer->stop();
    }
    delete ui;
}

void LoginDialog::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制动态背景
    painter.drawPixmap(0, 0, _bg_pixmap);
    
    // 绘制圆角矩形窗体内容（保持原UI）
    QPainterPath path;
    path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10); // 使用10像素的圆角
    painter.setClipPath(path);
    
    // 调用父类的绘制函数以保持UI内容
    QDialog::paintEvent(event);
}

void LoginDialog::updateBackground()
{
    // 创建与窗口大小相同的背景图片
    _bg_pixmap = QPixmap(size());
    _bg_pixmap.fill(Qt::transparent);
    
    QPainter painter(&_bg_pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 获取窗口大小
    QRect rect = this->rect();
    int width = rect.width();
    int height = rect.height();
    
    // 绘制渐变色背景
    QLinearGradient gradient(0, 0, width, height);
    gradient.setColorAt(0, QColor(173, 216, 230, 180));  // 淡蓝色
    gradient.setColorAt(0.5, QColor(135, 206, 250, 180)); // 天蓝色
    gradient.setColorAt(1, QColor(70, 130, 180, 180));    // 钢蓝色
    painter.fillRect(rect, gradient);
    
    // 绘制动态波浪效果
    QPainterPath wavePath;
    wavePath.moveTo(0, height * 0.6 + qSin(_ani_offset * 0.1) * 20);
    
    for (int x = 0; x <= width; x += 20) {
        wavePath.lineTo(x, height * 0.6 + qSin((_ani_offset + x) * 0.05) * 20);
    }
    
    wavePath.lineTo(width, height);
    wavePath.lineTo(0, height);
    wavePath.closeSubpath();
    
    painter.setBrush(QColor(255, 255, 255, 30)); // 半透明白色
    painter.setPen(Qt::NoPen);
    painter.drawPath(wavePath);
    
    // 绘制漂浮圆点
    painter.setPen(QColor(255, 255, 255, 80));
    painter.setBrush(QColor(255, 255, 255, 80));
    
    // 圆点1
    int x1 = (qSin(_ani_offset * 0.02) * 50 + width / 3) % width;
    int y1 = (qCos(_ani_offset * 0.02) * 30 + height / 4) % height;
    painter.drawEllipse(x1, y1, 15, 15);
    
    // 圆点2
    int x2 = (qCos(_ani_offset * 0.03) * 70 + 2 * width / 3) % width;
    int y2 = (qSin(_ani_offset * 0.03) * 40 + 2 * height / 3) % height;
    painter.drawEllipse(x2, y2, 20, 20);
    
    // 圆点3
    int x3 = (qSin(_ani_offset * 0.015) * 60 + width / 2) % width;
    int y3 = (qCos(_ani_offset * 0.015) * 50 + height / 2) % height;
    painter.drawEllipse(x3, y3, 12, 12);
    
    // 刷新显示
    update();
}

void LoginDialog::updateAnimation()
{
    _ani_offset++;
    updateBackground();
}

void LoginDialog::initHead()
{
    // 加载图片
    QPixmap originalPixmap(":/images/favicon.png");
    qDebug()<< originalPixmap.size() << ui->head_label->size();
    originalPixmap = originalPixmap.scaled(ui->head_label->size(),
        Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 创建一个和原始图片相同大小的QPixmap，用于绘制圆角图片
    QPixmap roundedPixmap(originalPixmap.size());
    roundedPixmap.fill(Qt::transparent); // 用透明色填充

    QPainter painter(&roundedPixmap);
    painter.setRenderHint(QPainter::Antialiasing); // 设置抗锯齿，使圆角更平滑
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 使用QPainterPath设置圆角
    QPainterPath path;
    path.addRoundedRect(0, 0, originalPixmap.width(), originalPixmap.height(), 10, 10); // 最后两个参数分别是x和y方向的圆角半径
    painter.setClipPath(path);

    // 将原始图片绘制到roundedPixmap上
    painter.drawPixmap(0, 0, originalPixmap);

    // 设置绘制好的圆角图片到QLabel上
    ui->head_label->setPixmap(roundedPixmap);
}

void LoginDialog::AddTipErr(TipErr te,QString tips){
    _tip_errs[te] = tips;
    showTip(tips, false);
}
void LoginDialog::DelTipErr(TipErr te){
    _tip_errs.remove(te);
    if(_tip_errs.empty()){
        ui->err_tip->clear();
        return;
    }
    showTip(_tip_errs.first(), false);
}

void LoginDialog::showTip(QString str, bool b_ok)
{
    if(b_ok){
        ui->err_tip->setProperty("state","normal");
    }else{
        ui->err_tip->setProperty("state","err");
    }

    ui->err_tip->setText(str);

    repolish(ui->err_tip);
}

bool LoginDialog::checkUserValid()
{
    auto email = ui->email_edit->text();
    if(email.isEmpty()){
        qDebug() << "email empty " ;
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱不能为空"));
        return false;
    }
    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool LoginDialog::checkPwdValid()
{
    auto pwd = ui->pass_edit->text();
    if(pwd.length() < 6 || pwd.length() > 15){
        qDebug() << "Pass length invalid";
        //提示长度不准确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }

    // 创建一个正则表达式对象，按照上述密码要求
    // 这个正则表达式解释：
    // ^[a-zA-Z0-9!@#$%^&*.]{6,15}$ 密码长度至少6，可以是字母、数字和特定的特殊字符
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*.]{6,15}$");
    bool match = regExp.match(pwd).hasMatch();
    if(!match){
        //提示字符非法
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符且长度为(6~15)"));
        return false;;
    }

    DelTipErr(TipErr::TIP_PWD_ERR);

    return true;
}

bool LoginDialog::enableBtn(bool enabled)
{
    ui->login_btn->setEnabled(enabled);
    ui->reg_btn->setEnabled(enabled);
    return true;
}

void LoginDialog::initHttpHandler()
{
    //注册获取登录回包逻辑
    _handlers.insert(ReqId::ID_LOGIN_USER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
            enableBtn(true);
            return;
        }
        auto email = jsonObj["email"].toString();

        //发送信号通知tcpMgr发送长链接
        ServerInfo si;
        si.Uid = jsonObj["uid"].toInt();
        si.Host = jsonObj["host"].toString();
        si.Port = jsonObj["port"].toString();
        si.Token = jsonObj["token"].toString();

        _uid = si.Uid;
        _token = si.Token;
        qDebug()<< "email is " << email << " uid is " << si.Uid <<" host is "
                 << si.Host << " Port is " << si.Port << " Token is " << si.Token;
        emit sig_connect_tcp(si);
    });
}

void LoginDialog::slot_forget_pwd()
{
    qDebug()<<"slot forget pwd";
    emit switchReset();
}

void LoginDialog::on_login_btn_clicked()
{
    qDebug()<<"login btn clicked";
    if(checkUserValid() == false){
        return;
    }

    if(checkPwdValid() == false){
        return ;
    }

    enableBtn(false);
    auto email = ui->email_edit->text();
    auto pwd = ui->pass_edit->text();
    //发送http请求登录
    QJsonObject json_obj;
    json_obj["email"] = email;
    json_obj["passwd"] = xorString(pwd);
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_login"),
        json_obj, ReqId::ID_LOGIN_USER,Modules::LOGINMOD);
}

void LoginDialog::slot_login_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    if(err != ErrorCodes::SUCCESS){
        showTip(tr("网络请求错误"),false);
        return;
    }

    // 解析 JSON 字符串,res需转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    //json解析错误
    if(jsonDoc.isNull()){
        showTip(tr("json解析错误"),false);
        return;
    }

    //json解析错误
    if(!jsonDoc.isObject()){
        showTip(tr("json解析错误"),false);
        return;
    }

    //调用对应的逻辑,根据id回调。
    _handlers[id](jsonDoc.object());

    return;
}

void LoginDialog::slot_tcp_con_finish(bool bsuccess)
{
    if(bsuccess){
        showTip(tr("聊天服务连接成功，正在登录..."),true);
        QJsonObject jsonObj;
        jsonObj["uid"] = _uid;
        jsonObj["token"] = _token;
        QJsonDocument doc(jsonObj);
        QByteArray jsonString = doc.toJson(QJsonDocument::Indented);
        //发送tcp请求给chat server
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_CHAT_LOGIN, jsonString);
    }else{
        showTip(tr("网络异常"),false);
        enableBtn(true);
    }
}

void LoginDialog::slot_login_failed(int err)
{
    QString result = QString("登录失败,err is %1").arg(err);
    showTip(result,false);
    enableBtn(true);
}
```

### 步骤 3：修改样式表 (stylesheet.qss)

更新样式表文件，将 LoginDialog 的背景设为透明：

```css
QDialog#LoginDialog,RegisterDialog,ResetDialog{
    background-color:transparent; /* 设置为透明背景 */
    border-radius: 10px; /* 添加圆角 */
}

QLineEdit {
    border: none;
    background-color: rgba(240,240,240, 180);
    border-radius: 6px;
    /*padding: 3px;*/
}

#user_label,#pass_label{
    color:white;
}

#err_tip[state='normal']{
    color:green;
}

#err_tip[state='err']{
    color:red;
}

#login_btn{
    border-radius: 9px; /* 设置圆角 */
    background-color:rgba(255,220,230, 200);
    border: none;
}

#reg_btn{
    border-radius: 9px; /* 设置圆角 */
    background-color:rgba(255,220,240, 200);
    border: none;
}

/* 其他样式保持不变... */

#forget_label[state='normal'],#forget_label[state='selected']{
    color:white;
}

#forget_label[state='hover'],#forget_label[state='selected_hover']{
    color:rgb(42,112,241);
}
```

## 效果说明

实现后的动态背景包含以下效果：

1. **渐变色背景**：从淡蓝色到钢蓝色的垂直渐变
2. **动态波浪效果**：在界面下半部分的流动波浪动画
3. **漂浮圆点**：3个不同大小的半透明白色圆点在背景中漂浮
4. **圆角窗体**：使窗体具有现代化的圆角外观

## 自定义选项

您可以根据需要调整以下参数来改变视觉效果：

- 动画速度：修改 `_ani_timer->start(50)` 中的毫秒值
- 颜色：修改 `QLinearGradient` 中的颜色值
- 波浪幅度：调整 `qSin()` 函数中的振幅参数
- 漂浮元素：修改 `painter.drawEllipse()` 函数中的参数