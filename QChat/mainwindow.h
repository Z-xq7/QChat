/*******************************************************
* @file         mainwindow.h
* @brief        主窗口
*
* @author       祁七七
* @date          2026/02/09
* @history
*******************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "logindialog.h"
#include "registerdialog.h"
#include "resetdialog.h"
#include "chatdialog.h"
#include "tcpmgr.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

enum UIStatus{
    LOGIN_UI,
    REGISTER_UI,
    RESET_UI,
    CHAT_UI
};

class MainWindow : public QMainWindow
{
    Q_OBJECT 
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void offlineLogin();        //由SlotOffline调用退回到登录页面

private:
    Ui::MainWindow *ui;
    LoginDialog* _login_dlg;
    RegisterDialog* _reg_dlg;
    ResetDialog* _reset_dlg;
    ChatDialog* _chat_dlg;
    UIStatus _ui_status;

public slots:
    void SlotSwitchReg();       //由登录页面切换到注册页面
    void SlotSwitchLogin();     //由注册页面切换到登录页面
    void SlotSwitchReset();     //由登录页面切换到重置密码页面
    void SlotSwitchLogin2();    //由重置密码页面切换到登录页面
    void SlotSwitchChat();      //由登录页面切换到聊天页面
    void SlotOffline();         //异地登录收到通知退回到登录页面
    void SlotExcepConOffline(); //连接异常通知下线

};

#endif // MAINWINDOW_H
