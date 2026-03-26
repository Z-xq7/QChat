#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QStatusBar>
#include <QMessageBox>
#include "filetcpmgr.h"
#include <QVBoxLayout>
#include "videocallmanager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //隐藏mainwindow底部边框
    this->statusBar()->hide();
    
    // 设置无边框窗口，这样系统菜单栏就不会显示
    setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    
    // 设置窗口大小
    this->setMinimumSize(QSize(300, 485));
    this->setMaximumSize(QSize(300, 485));
    this->resize(300, 485);

    // 设置主布局
    setupMainLayout();
    
    // 初始化自定义标题栏
    setupCustomTitleBar();

    //登录界面
    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    _login_dlg->show();
    _mainLayout->addWidget(_login_dlg);
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
    //连接资源服务器断开
    connect(FileTcpMgr::GetInstance().get(), &FileTcpMgr::sig_connection_closed,
            this, &MainWindow::SlotResServerConOffline);
    
    //测试
    //emit TcpMgr::GetInstance()->sig_switch_chatdlg();
    qDebug()<<"[MainWindow]: Init Success";
}

void MainWindow::setupMainLayout()
{
    // 创建主内容widget和布局
    _mainWidget = new QWidget(this);
    _mainLayout = new QVBoxLayout(_mainWidget);
    _mainLayout->setContentsMargins(0, 0, 0, 0);  // 设置边距为0
    _mainLayout->setSpacing(0);                   // 设置间距为0
    
    // 将主内容widget设置为中央部件
    setCentralWidget(_mainWidget);
}

void MainWindow::setupCustomTitleBar()
{
    // 创建自定义标题栏
    _title_bar = new TitleBar(this);
    _title_bar->setWindowTitle("QChat");
    _title_bar->setWindowIcon(QIcon(":/images/favicon.png"));
    _title_bar->setTheme("login"); // 设置为登录页面主题

    // 将标题栏添加到主布局的顶部
    _mainLayout->insertWidget(0, _title_bar);

    // 连接标题栏按钮信号
    connect(_title_bar, &TitleBar::minimizeClicked, this, [this]() {
        this->showMinimized();
    });
    connect(_title_bar, &TitleBar::maximizeClicked, this, [this]() {
        if (this->isMaximized()) {
            this->showNormal();
        } else {
            this->showMaximized();
        }
    });
    connect(_title_bar, &TitleBar::closeClicked, this, [this]() {
        this->close();
    });
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

    // 从布局中移除当前页面（如果存在）
    if (_login_dlg) {
        _mainLayout->removeWidget(_login_dlg);
        _login_dlg->hide();
    }
    
    // 添加新页面到布局
    _mainLayout->addWidget(_reg_dlg);
    _reg_dlg->show();
    
    // 更新标题栏主题为登录页面主题（注册页面使用相同主题）
    if (_title_bar) {
        _title_bar->setTheme("login");
    }
}

void MainWindow::SlotSwitchLogin()
{
    _ui_status = LOGIN_UI;

    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    
    // 从布局中移除当前页面（如果存在）
    if (_reg_dlg) {
        _mainLayout->removeWidget(_reg_dlg);
        _reg_dlg->hide();
    }
    
    // 添加新页面到布局
    _mainLayout->addWidget(_login_dlg);
    _login_dlg->show();

    //连接登录界面注册信号
    connect(_login_dlg,&LoginDialog::switchRegister,this,&MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dlg,&LoginDialog::switchReset,this,&MainWindow::SlotSwitchReset);
    
    // 更新标题栏主题为登录页面主题
    if (_title_bar) {
        _title_bar->setTheme("login");
    }
}

void MainWindow::SlotSwitchReset()
{
    _ui_status = RESET_UI;

    _reset_dlg = new ResetDialog(this);
    _reset_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    
    // 从布局中移除当前页面（如果存在）
    if (_login_dlg) {
        _mainLayout->removeWidget(_login_dlg);
        _login_dlg->hide();
    }
    
    // 添加新页面到布局
    _mainLayout->addWidget(_reset_dlg);
    _reset_dlg->show();
    
    //注册返回登录信号和槽函数
    connect(_reset_dlg, &ResetDialog::switchLogin, this, &MainWindow::SlotSwitchLogin2);
    
    // 更新标题栏主题为登录页面主题（重置页面使用相同主题）
    if (_title_bar) {
        _title_bar->setTheme("login");
    }
}

void MainWindow::SlotSwitchLogin2()
{
    _ui_status = LOGIN_UI;

    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    
    // 从布局中移除当前页面（如果存在）
    if (_reset_dlg) {
        _mainLayout->removeWidget(_reset_dlg);
        _reset_dlg->hide();
    }
    
    // 添加新页面到布局
    _mainLayout->addWidget(_login_dlg);
    _login_dlg->show();

    //连接登录界面注册信号
    connect(_login_dlg,&LoginDialog::switchRegister,this,&MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dlg,&LoginDialog::switchReset,this,&MainWindow::SlotSwitchReset);
    
    // 更新标题栏主题为登录页面主题
    if (_title_bar) {
        _title_bar->setTheme("login");
    }
}

void MainWindow::SlotSwitchChat()
{
    _ui_status = CHAT_UI;

    //隐藏mainwindow底部边框
    this->statusBar()->hide();

    _chat_dlg = new ChatDialog(this);
    _chat_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    
    // 从布局中移除当前页面（如果存在）
    if (_login_dlg) {
        _mainLayout->removeWidget(_login_dlg);
        _login_dlg->hide();
    }
    
    // 添加新页面到布局
    _mainLayout->addWidget(_chat_dlg);
    _chat_dlg->show();

    //连接退出登录信号
    connect(_chat_dlg, &ChatDialog::switch_login, this, &MainWindow::offlineLogin);

    this->setMinimumSize(QSize(970,720)); // 增加高度以适应标题栏
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    //加载聊天列表
    _chat_dlg->loadChatList();
    
    // 初始化视频通话管理器连接
    TcpMgr::GetInstance()->initializeVideoCallConnections();
    
    // 更新标题栏主题为聊天界面主题
    if (_title_bar) {
        _title_bar->setTheme("chat");
    }
}

void MainWindow::SlotOffline()
{
    //使用静态方法直接弹出一个信息框
    QMessageBox::information(this,"下线提示","同账号异地登录，该终端下线！");
    TcpMgr::GetInstance()->CloseConnection();
    FileTcpMgr::GetInstance()->CloseConnection();
    offlineLogin();
}

void MainWindow::SlotExcepConOffline()
{
    // 使用静态方法直接弹出一个信息框
    QMessageBox::information(this, "下线提示", "心跳超时或临界异常，该终端下线！");
    TcpMgr::GetInstance()->CloseConnection();
    FileTcpMgr::GetInstance()->CloseConnection();
    offlineLogin();
}

void MainWindow::SlotResServerConOffline(){
    // 使用静态方法直接弹出一个信息框
    QMessageBox::information(this, "下线提示", "与资源服务器断开连接！");
    TcpMgr::GetInstance()->CloseConnection();
    FileTcpMgr::GetInstance()->CloseConnection();
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
    
    // 从布局中移除当前页面（如果存在）
    if (_chat_dlg) {
        _mainLayout->removeWidget(_chat_dlg);
        _chat_dlg->hide();
    }

    this->setMinimumSize(QSize(300,485)); // 调整大小以包含标题栏
    this->setMaximumSize(QSize(300,485));
    this->resize(300,485);
    
    // 添加新页面到布局
    _mainLayout->addWidget(_login_dlg);
    _login_dlg->show();

    //连接登录界面注册信号
    connect(_login_dlg,&LoginDialog::switchRegister,this,&MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dlg,&LoginDialog::switchReset,this,&MainWindow::SlotSwitchReset);
    
    // 更新标题栏主题为登录页面主题
    if (_title_bar) {
        _title_bar->setTheme("login");
    }
}
