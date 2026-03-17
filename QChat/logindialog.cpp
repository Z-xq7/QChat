#include "logindialog.h"
#include "ui_logindialog.h"
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QFocusEvent>
#include "tcpmgr.h"
#include "httpmgr.h"
#include "filetcpmgr.h"
#include "cute_pet_widget.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
    , _ani_timer(nullptr)
    , _ani_offset(0)
    , _petWidget(nullptr)
{
    ui->setupUi(this);

    // 设置窗口标志，保持透明背景
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true); // 启用鼠标追踪，使鼠标未按下时也能触发mouseMoveEvent

    // 创建Q版小宠物控件
    _petWidget = new CutePetWidget(this);
    // 调整宠物控件尺寸，覆盖整个对话框区域，这样鼠标在任何位置都能触发宠物跟随
    _petWidget->resize(width(), height());
    _petWidget->move(0, 0);
    _petWidget->lower(); // 将宠物控件置于底层，让UI控件在上层

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

    //连接tcp连接资源服务器请求的信号和槽函数
    connect(this, &LoginDialog::sig_connect_res_server,
            FileTcpMgr::GetInstance().get(), &FileTcpMgr::slot_tcp_connect);

    //连接资源管理tcp发出的连接成功信号
    connect(FileTcpMgr::GetInstance().get(), &FileTcpMgr::sig_con_success, this, &LoginDialog::slot_res_con_finish);

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
        qDebug() << "[LoginDialog]: Label was clicked!";
    });

    // 安装事件过滤器到输入框，监听焦点事件
    ui->email_edit->installEventFilter(this);
    ui->pass_edit->installEventFilter(this);
    
    // 为UI中的其他控件也安装事件过滤器以捕获鼠标移动事件
    ui->head_widget->installEventFilter(this);
    ui->head_widget->setAttribute(Qt::WA_TransparentForMouseEvents, true); // 设置头像容器为鼠标事件透明
    ui->email_label->installEventFilter(this);
    ui->pass_label->installEventFilter(this);
    ui->pass_visible->installEventFilter(this);
    ui->forget_label->installEventFilter(this);
    ui->login_btn->installEventFilter(this);
    ui->reg_btn->installEventFilter(this);
    ui->head_label->installEventFilter(this);
    ui->head_label->setAttribute(Qt::WA_TransparentForMouseEvents, true); // 设置头像标签为鼠标事件透明
    ui->err_tip->installEventFilter(this);
    ui->err_tip->setAttribute(Qt::WA_TransparentForMouseEvents, true); // 设置提示标签为鼠标事件透明
    
    // 为布局中的widget安装事件过滤器
    QWidgetList widgets = this->findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (widget != _petWidget) {  // 不为宠物控件安装过滤器，避免冲突
            widget->installEventFilter(this);
        }
    }

    // 设置整体样式
    setStyleSheet(
        "QDialog {"
        "    background: transparent;"
        "}"
        "QLineEdit {"
        "    border: 2px solid #d0d0d0;"
        "    border-radius: 10px;"
        "    padding: 0px 5px 0px 5px;"
        "    background: rgba(255, 255, 255, 0.8);"
        "    font-size: 12px;"
        "}"
        "QLineEdit:focus {"
        "    border: 2px solid #ff69b4;"
        "    background: rgba(255, 255, 255, 0.9);"
        "}"
        "QLabel {"
        "    color: #333;"
        "    font-weight: bold;"
        "    font-size: 13px;"
        "    background: transparent;"
        "}"
        "#err_tip[state='normal']{"
        "color: green;"
        "}"
        "#err_tip[state='err']{"
        "color: red;"
        "}"
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ff69b4, stop:1 #d74177);"
        "    border: none;"
        "    border-radius: 10px;"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    padding: 5px;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ff7eb3, stop:1 #ff4f8b);"
        "}"
        "QPushButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #d74177, stop:1 #ff69b4);"
        "}"
        "QPushButton:disabled {"
        "    background: #cccccc;"
        "}"
    );

    // 初始化背景
    updateBackground();
}

LoginDialog::~LoginDialog()
{
    qDebug() << "[LoginDialog]: LoginDialog distructed ...";
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

void LoginDialog::enterEvent(QEnterEvent *event)
{
    // 鼠标进入登录窗口时，激活宠物的鼠标跟随
    if (_petWidget) {
        _petWidget->setMousePosition(QCursor::pos());
    }
    QDialog::enterEvent(event);
}

void LoginDialog::leaveEvent(QEvent *event)
{
    // 鼠标离开登录窗口时，重置宠物到原始状态
    if (_petWidget) {
        _petWidget->resetToOriginalState();
    }
    QDialog::leaveEvent(event);
}

void LoginDialog::mouseMoveEvent(QMouseEvent *event)
{
    // 将鼠标位置传递给宠物控件，以实现跟随效果
    if (_petWidget) {
        _petWidget->setMousePosition(event->globalPos());
    }
    
    // 调用父类的鼠标移动事件
    QDialog::mouseMoveEvent(event);
}

bool LoginDialog::eventFilter(QObject *obj, QEvent *event)
{
    // 捕获所有控件上的鼠标移动事件
    if (event->type() == QEvent::MouseMove) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (_petWidget) {
            _petWidget->setMousePosition(mouseEvent->globalPos());
        }
        // 重要：不要返回true，让事件继续传递到控件
    }
    
    // 处理其他特定事件
    if (obj == ui->email_edit || obj == ui->pass_edit) {
        if (event->type() == QEvent::FocusIn) {
            // 当输入框获得焦点时，触发宠物偷看动画
            if (_petWidget) {
                _petWidget->startPeek();
            }
        }
        else if (event->type() == QEvent::MouseButtonPress) {
            // 当点击输入框时，也触发宠物偷看动画
            if (_petWidget) {
                _petWidget->startPeek();
            }
        }
    }
    else if (obj == ui->login_btn) {
        if (event->type() == QEvent::MouseButtonPress) {
            // 当点击登录按钮时，触发宠物兴奋动画
            if (_petWidget) {
                // 暂时使用偷看动画，实际项目中可以添加更丰富的动画
                _petWidget->startPeek();
            }
        }
    }
    else if (obj == ui->reg_btn) {
        if (event->type() == QEvent::MouseButtonPress) {
            // 当点击注册按钮时，触发宠物友好动画
            if (_petWidget) {
                _petWidget->startPeek();
            }
        }
    }
    // 重要：总是返回基类的eventFilter结果，确保事件正常传递
    return QDialog::eventFilter(obj, event);
}

void LoginDialog::on_email_edit_focusIn()
{
    // 当邮箱输入框获得焦点时，触发宠物偷看动画
    if (_petWidget) {
        _petWidget->startPeek();
    }
}

void LoginDialog::on_pass_edit_focusIn()
{
    // 当密码输入框获得焦点时，触发宠物偷看动画
    if (_petWidget) {
        _petWidget->startPeek();
    }
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

    // 绘制更美观的渐变色背景 - 粉紫色系
    QLinearGradient gradient(0, 0, width, height);
    gradient.setColorAt(0, QColor(255, 182, 193, 200));  // 浅粉色
    gradient.setColorAt(0.5, QColor(221, 160, 221, 200)); // 萝兰紫
    gradient.setColorAt(1, QColor(176, 196, 222, 200));   // 钢蓝色
    painter.fillRect(rect, gradient);

    // 绘制动态装饰效果 - 细线网格
    painter.setPen(QColor(255, 255, 255, 40));
    // 垂直线
    for (int x = 0; x < width; x += 30) {
        int offset = static_cast<int>(qSin(_ani_offset * 0.05 + x * 0.1) * 10);
        painter.drawLine(x, 0, x + offset, height);
    }
    // 水平线
    for (int y = 0; y < height; y += 30) {
        int offset = static_cast<int>(qCos(_ani_offset * 0.05 + y * 0.1) * 10);
        painter.drawLine(0, y, width, y + offset);
    }

    // 绘制漂浮粒子效果
    for (int i = 0; i < 5; ++i) {
        int size = 8 + (i % 3); // 不同大小
        int speed = 1 + (i % 3); // 不同速度
        int x = static_cast<int>(qSin(_ani_offset * 0.02 * speed + i) * (width / 4) + width / 2);
        int y = static_cast<int>(qCos(_ani_offset * 0.02 * speed + i) * (height / 4) + height / 2);
        
        QRadialGradient particleGradient(x, y, size);
        particleGradient.setColorAt(0, QColor(255, 255, 255, 100));
        particleGradient.setColorAt(1, QColor(255, 255, 255, 0));
        
        painter.setBrush(particleGradient);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(x - size/2, y - size/2, size, size);
    }

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
    qDebug()<< "[LoginDialog]: " << originalPixmap.size() << ui->head_label->size();
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
        qDebug() << "[LoginDialog]: email empty " ;
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
        qDebug() << "[LoginDialog]: Pass length invalid";
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
        _si = std::make_shared<ServerInfo>();

        _si->_uid = jsonObj["uid"].toInt();
        _si->_chat_host = jsonObj["chathost"].toString();
        _si->_chat_port = jsonObj["chatport"].toString();
        _si->_token = jsonObj["token"].toString();

        _si->_res_host = jsonObj["reshost"].toString();
        _si->_res_port = jsonObj["resport"].toString();


        qDebug()<< "[LoginDialog]: email is " << email << " uid is " << _si->_uid <<" chat host is "
                 << _si->_chat_host << " chat port is "
                 << _si->_chat_port << " token is " << _si->_token
                 << " res host is " << _si->_res_host
                 << " res port is " << _si->_res_port;
        emit sig_connect_tcp(_si);
    });
}

void LoginDialog::slot_forget_pwd()
{
    qDebug()<<"[LoginDialog]: slot forget pwd";
    emit switchReset();
}

void LoginDialog::on_login_btn_clicked()
{
    qDebug()<<"[LoginDialog]: login btn clicked";
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
        showTip(tr("聊天服务连接成功，正在连接资源服务器..."),true);
        emit sig_connect_res_server(_si);
    }else{
        showTip(tr("网络异常"), false);
    }
}

void LoginDialog::slot_login_failed(int err)
{
    QString result = QString("登录失败,err is %1").arg(err);
    showTip(result,false);
    enableBtn(true);
}

void LoginDialog::slot_res_con_finish(bool bsuccess)
{
    if(bsuccess){
        showTip(tr("聊天服务连接成功，正在登录..."),true);
        QJsonObject jsonObj;
        jsonObj["uid"] = _si->_uid;
        jsonObj["token"] = _si->_token;

        QJsonDocument doc(jsonObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Indented);

        //发送tcp请求给chat server
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_CHAT_LOGIN, jsonData);

    }else{
        showTip(tr("网络异常"),false);
        enableBtn(true);
    }

}



