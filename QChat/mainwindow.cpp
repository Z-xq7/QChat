#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QStatusBar>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //隐藏mainwindow底部边框
    this->statusBar()->hide();

    //登录界面
    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    _login_dlg->show();
    setCentralWidget(_login_dlg);
    _ui_status = LOGIN_UI;

    //创建和注册消息链接
    connect(_login_dlg,&LoginDialog::switchRegister,this,&MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dlg,&LoginDialog::switchReset,this,&MainWindow::SlotSwitchReset);
    //连接创建聊天界面信号
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_switch_chatdlg, this, &MainWindow::SlotSwitchChat);
    //链接服务器踢人消息（异地登录）
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_notify_offline, this, &MainWindow::SlotOffline);
    //连接服务器断开心跳超时或异常连接信息
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_connection_closed, this, &MainWindow::SlotExcepConOffline);

    //测试
    //emit TcpMgr::GetInstance()->sig_switch_chatdlg();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::SlotSwitchReg()
{
    _ui_status = REGISTER_UI;

    _reg_dlg = new RegisterDialog(this);

    _reg_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);

    //连接注册界面返回登录信号
    connect(_reg_dlg,&RegisterDialog::sigSwitchLogin,this,&MainWindow::SlotSwitchLogin);

    setCentralWidget(_reg_dlg);
    _reg_dlg->show();
    _reg_dlg->resize(this->size()); // 调整对话框大小以填充满主窗口
    
    if (_login_dlg) {
        _login_dlg->hide();
    }
}

void MainWindow::SlotSwitchLogin()
{
    _ui_status = LOGIN_UI;

    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    setCentralWidget(_login_dlg);
    _login_dlg->show();
    _login_dlg->resize(this->size()); // 调整对话框大小以填充满主窗口
    
    if (_reg_dlg) {
        _reg_dlg->hide();
    }

    //连接登录界面注册信号
    connect(_login_dlg,&LoginDialog::switchRegister,this,&MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dlg,&LoginDialog::switchReset,this,&MainWindow::SlotSwitchReset);
}

void MainWindow::SlotSwitchReset()
{
    _ui_status = RESET_UI;

    //创建一个CentralWidget, 并将其设置为MainWindow的中心部件
    _reset_dlg = new ResetDialog(this);
    _reset_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    setCentralWidget(_reset_dlg);
    _reset_dlg->show();
    _reset_dlg->resize(this->size()); // 调整对话框大小以填充满主窗口
    
    if (_login_dlg) {
        _login_dlg->hide();
    }
    //注册返回登录信号和槽函数
    connect(_reset_dlg, &ResetDialog::switchLogin, this, &MainWindow::SlotSwitchLogin2);
}

void MainWindow::SlotSwitchLogin2()
{
    _ui_status = LOGIN_UI;

    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    setCentralWidget(_login_dlg);
    if (_reset_dlg) {
        _reset_dlg->hide();
    }
    _login_dlg->show();

    //连接登录界面注册信号
    connect(_login_dlg,&LoginDialog::switchRegister,this,&MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dlg,&LoginDialog::switchReset,this,&MainWindow::SlotSwitchReset);
}

void MainWindow::SlotSwitchChat()
{
    _ui_status = CHAT_UI;

    //隐藏mainwindow底部边框
    this->statusBar()->hide();

    _chat_dlg = new ChatDialog(this);
    _chat_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    setCentralWidget(_chat_dlg);
    
    // 检查_login_dlg是否有效，避免空指针访问
    if (_login_dlg) {
        _login_dlg->hide();
    }
    _chat_dlg->show();

    this->setMinimumSize(QSize(880,650));
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    //加载聊天列表
    _chat_dlg->loadChatList();
}

void MainWindow::SlotOffline()
{
    //使用静态方法直接弹出一个信息框
    QMessageBox::information(this,"下线提示","同账号异地登录，该终端下线！");
    TcpMgr::GetInstance()->CloseConnection();
    offlineLogin();
}

void MainWindow::SlotExcepConOffline()
{
    // 使用静态方法直接弹出一个信息框
    QMessageBox::information(this, "下线提示", "心跳超时或临界异常，该终端下线！");
    TcpMgr::GetInstance()->CloseConnection();
    offlineLogin();
}

void MainWindow::offlineLogin()
{
    if(_ui_status == LOGIN_UI){
        return;
    }else{
        _ui_status = LOGIN_UI;
    }
    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    setCentralWidget(_login_dlg);

    if (_chat_dlg) {
        _chat_dlg->hide();
    }

    this->setMinimumSize(QSize(300,455));
    this->setMaximumSize(QSize(300,455));
    this->resize(300,455);
    _login_dlg->show();

    //连接登录界面注册信号
    connect(_login_dlg,&LoginDialog::switchRegister,this,&MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dlg,&LoginDialog::switchReset,this,&MainWindow::SlotSwitchReset);
}
