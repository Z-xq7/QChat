#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "global.h"
#include "httpmgr.h"

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
    , _countdown(5)
    , _ani_timer(nullptr)
    , _ani_offset(0)
{
    ui->setupUi(this);

    // 设置窗口标志，保持透明背景
    setAttribute(Qt::WA_TranslucentBackground, true);
    
    // 创建动画定时器
    _ani_timer = new QTimer(this);
    connect(_ani_timer, &QTimer::timeout, this, &RegisterDialog::updateAnimation);
    _ani_timer->start(50); // 每50ms更新一次动画，约20fps

    //设置错误提示状态
    ui->err_tip->setProperty("state","normal");
    //刷新错误提示
    repolish(ui->err_tip);
    //连接http通知(注册)
    connect(HttpMgr::GetInstance().get(),&HttpMgr::sig_reg_mod_finish,
        this,&RegisterDialog::slot_reg_mod_finish);
    //初始化验证码处理
    initHttpHandlers();
    //初始阶段不显示错误提示
    ui->err_tip->clear();

    connect(ui->user_edit,&QLineEdit::editingFinished,this,[this](){
        checkUserValid();
    });

    connect(ui->email_edit, &QLineEdit::editingFinished, this, [this](){
        checkEmailValid();
    });

    connect(ui->pass_edit, &QLineEdit::editingFinished, this, [this](){
        checkPassValid();
    });

    connect(ui->confirm_edit, &QLineEdit::editingFinished, this, [this](){
        checkConfirmValid();
    });

    connect(ui->varify_edit, &QLineEdit::editingFinished, this, [this](){
        checkVarifyValid();
    });

    //设置浮动显示手形状
    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->confirm_visible->setCursor(Qt::PointingHandCursor);

    ui->pass_visible->SetState("unvisible","unvisible_hover","","visible",
                               "visible_hover","");
    ui->confirm_visible->SetState("unvisible","unvisible_hover","","visible",
                                  "visible_hover","");

    //设置密码不可见
    ui->pass_edit->setEchoMode(QLineEdit::Password);
    ui->confirm_edit->setEchoMode(QLineEdit::Password);

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

    connect(ui->confirm_visible, &ClickedLabel::clicked, this, [this]() {
        // 获取当前的echoMode状态，而不是按钮状态
        if(ui->confirm_edit->echoMode() == QLineEdit::Password) {
            // 当前是密码模式，切换到正常模式
            ui->confirm_edit->setEchoMode(QLineEdit::Normal);
        } else {
            // 当前是正常模式，切换到密码模式
            ui->confirm_edit->setEchoMode(QLineEdit::Password);
        }
        qDebug() << "Label was clicked!";
    });

    //创建定时器
    _countdown_timer = new QTimer(this);
    // 连接信号和槽
    connect(_countdown_timer, &QTimer::timeout, [this](){
        if(_countdown==0){
            _countdown_timer->stop();
            emit sigSwitchLogin();
            return;
        }
        _countdown--;
        auto str = QString("注册成功，%1 s后返回登录").arg(_countdown);
        ui->tip_lb->setText(str);
    });
    
    // 初始化背景
    updateBackground();
}

RegisterDialog::~RegisterDialog()
{
    qDebug() << "RegisterDialog distructed ...";
    delete ui;
}

//邮箱验证
void RegisterDialog::on_get_code_clicked()
{
    auto email = ui->email_edit->text();
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    bool match = regex.match(email).hasMatch();
    if(match){
        //发送验证码
        QJsonObject json_obj;
        json_obj["email"] = email;
        HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix + "/get_code"),
            json_obj,ReqId::ID_GET_VARIFY_CODE,Modules::REGISTERMOD);

    }else{
        showTip(tr("邮箱地址不正确"),false);
    }
}

void RegisterDialog::slot_reg_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    if(err != ErrorCodes::SUCCESS){
        showTip(tr("网络请求错误"),false);
        return;
    }

    //解析Json字符串：res转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());  //相当于转换为json文档格式（QJsonDocument）
    if(jsonDoc.isNull()){
        showTip(tr("JSON解析失败"),false);
        return;
    }
    //json解析错误
    if(!jsonDoc.isObject()){    //判断能不能转为json对象
        showTip(tr("JSON解析失败"),false);
        return;
    }

    //将json文档转为json对象，调用回调函数
    _handlers[id](jsonDoc.object());
    return;
}

//设置错误提示样式
void RegisterDialog::showTip(QString str,bool b_ok)
{
    if(b_ok){
        ui->err_tip->setProperty("state","normal");
    }else{
        ui->err_tip->setProperty("state","err");
    }
    ui->err_tip->setText(str);
    repolish(ui->err_tip);
}

void RegisterDialog::initHttpHandlers()
{
    //注册获取验证码回包的逻辑
    _handlers.insert(ReqId::ID_GET_VARIFY_CODE,[this](const QJsonObject& jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
            return;
        }

        auto email = jsonObj["email"].toString();
        showTip(tr("验证码已经发送到邮箱，注意查收"),true);
        qDebug() << "email is: " << email;
    });

    //注册用户回包逻辑
    _handlers.insert(ReqId::ID_REG_USER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
            return;
        }
        auto email = jsonObj["email"].toString();
        showTip(tr("用户注册成功"), true);
        qDebug()<< "user uuid is: " << jsonObj["uid"].toString();
        qDebug()<< "email is " << email ;
        ChangeTipPage();
    });
}

void RegisterDialog::AddTipErr(TipErr te, QString tips)
{
    _tip_errs[te] = tips;
    showTip(tips, false);
}

void RegisterDialog::DelTipErr(TipErr te)
{
    _tip_errs.remove(te);
    if(_tip_errs.empty()){
        ui->err_tip->clear();
        return;
    }

    showTip(_tip_errs.first(), false);
}

bool RegisterDialog::checkUserValid()
{
    if(ui->user_edit->text() == ""){
        AddTipErr(TipErr::TIP_USER_ERR, tr("用户名不能为空"));
        return false;
    }

    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

bool RegisterDialog::checkPassValid()
{
    auto pass = ui->pass_edit->text();
    auto confirm = ui->confirm_edit->text();

    if(pass.length() < 6 || pass.length()>15){
        //提示长度不准确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }

    // 创建一个正则表达式对象，按照上述密码要求
    // 这个正则表达式解释：
    // ^[a-zA-Z0-9!@#$%^&*]{6,15}$ 密码长度至少6，可以是字母、数字和特定的特殊字符
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*]{6,15}$");
    bool match = regExp.match(pass).hasMatch();
    if(!match){
        //提示字符非法
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符"));
        return false;;
    }

    DelTipErr(TipErr::TIP_PWD_ERR);

    if(pass != confirm){
        //提示密码不匹配
        AddTipErr(TipErr::TIP_PWD_CONFIRM,tr("密码和确认密码不匹配"));
        return false;
    }else{
        DelTipErr(TipErr::TIP_PWD_CONFIRM);
    }

    return true;
}

bool RegisterDialog::checkConfirmValid()
{
    auto pass = ui->pass_edit->text();
    auto confirm = ui->confirm_edit->text();

    if(confirm.length() < 6 || confirm.length()>15){
        //提示长度不准确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }

    // 创建一个正则表达式对象，按照上述密码要求
    // 这个正则表达式解释：
    // ^[a-zA-Z0-9!@#$%^&*]{6,15}$ 密码长度至少6，可以是字母、数字和特定的特殊字符
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*]{6,15}$");
    bool match = regExp.match(confirm).hasMatch();
    if(!match){
        //提示字符非法
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符"));
        return false;;
    }

    DelTipErr(TipErr::TIP_PWD_ERR);

    if(pass != confirm){
        //提示密码不匹配
        AddTipErr(TipErr::TIP_PWD_CONFIRM,tr("密码和确认密码不匹配"));
        return false;
    }else{
        DelTipErr(TipErr::TIP_PWD_CONFIRM);
    }

    return true;
}

bool RegisterDialog::checkEmailValid()
{
    //验证邮箱的地址正则表达式
    auto email = ui->email_edit->text();
    // 邮箱地址的正则表达式
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    bool match = regex.match(email).hasMatch(); // 执行正则表达式匹配
    if(!match){
        //提示邮箱不正确
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱地址不正确"));
        return false;
    }

    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool RegisterDialog::checkVarifyValid()
{
    auto pass = ui->varify_edit->text();
    if(pass.isEmpty()){
        AddTipErr(TipErr::TIP_VARIFY_ERR, tr("验证码不能为空"));
        return false;
    }

    DelTipErr(TipErr::TIP_VARIFY_ERR);
    return true;
}

void RegisterDialog::ChangeTipPage()
{
    _countdown_timer->stop();
    ui->stackedWidget->setCurrentWidget(ui->page_2);

    // 启动定时器，设置间隔为1000毫秒（1秒）
    _countdown_timer->start(1000);
}

void RegisterDialog::on_sure_btn_clicked()
{
    if(ui->user_edit->text() == ""){
        showTip(tr("用户名不能为空"), false);
        return;
    }

    if(ui->email_edit->text() == ""){
        showTip(tr("邮箱不能为空"), false);
        return;
    }

    if(ui->pass_edit->text() == ""){
        showTip(tr("密码不能为空"), false);
        return;
    }

    if(ui->confirm_edit->text() == ""){
        showTip(tr("确认密码不能为空"), false);
        return;
    }

    if(ui->confirm_edit->text() != ui->pass_edit->text()){
        showTip(tr("密码和确认密码不匹配"), false);
        return;
    }

    if(ui->varify_edit->text() == ""){
        showTip(tr("验证码不能为空"), false);
        return;
    }

    //发送http请求注册用户
    QJsonObject json_obj;
    json_obj["user"] = ui->user_edit->text();
    json_obj["email"] = ui->email_edit->text();
    json_obj["passwd"] = xorString(ui->pass_edit->text());
    json_obj["confirm"] = xorString(ui->confirm_edit->text());
    json_obj["varifycode"] = ui->varify_edit->text();
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_register"),
                                        json_obj, ReqId::ID_REG_USER,Modules::REGISTERMOD);
}


void RegisterDialog::on_return_btn_clicked()
{
    _countdown_timer->stop();
    emit sigSwitchLogin();
}

void RegisterDialog::on_cancel_btn_clicked()
{
    _countdown_timer->stop();
    emit sigSwitchLogin();
}

void RegisterDialog::paintEvent(QPaintEvent *event)
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

void RegisterDialog::updateBackground()
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
    wavePath.moveTo(0, static_cast<int>(height * 0.6 + qSin(_ani_offset * 0.1) * 20));
    
    for (int x = 0; x <= width; x += 20) {
        wavePath.lineTo(x, static_cast<int>(height * 0.6 + qSin((_ani_offset + x) * 0.05) * 20));
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
    int x1 = static_cast<int>(qSin(_ani_offset * 0.02) * 50 + width / 3) % width;
    int y1 = static_cast<int>(qCos(_ani_offset * 0.02) * 30 + height / 4) % height;
    painter.drawEllipse(x1, y1, 15, 15);
    
    // 圆点2
    int x2 = static_cast<int>(qCos(_ani_offset * 0.03) * 70 + 2 * width / 3) % width;
    int y2 = static_cast<int>(qSin(_ani_offset * 0.03) * 40 + 2 * height / 3) % height;
    painter.drawEllipse(x2, y2, 20, 20);
    
    // 圆点3
    int x3 = static_cast<int>(qSin(_ani_offset * 0.015) * 60 + width / 2) % width;
    int y3 = static_cast<int>(qCos(_ani_offset * 0.015) * 50 + height / 2) % height;
    painter.drawEllipse(x3, y3, 12, 12);
    
    // 刷新显示
    update();
}

void RegisterDialog::updateAnimation()
{
    _ani_offset++;
    updateBackground();
}

