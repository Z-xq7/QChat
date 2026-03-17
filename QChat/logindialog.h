#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QPixmap>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include "global.h"

// 前向声明
class CutePetWidget;

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
    void enterEvent(QEnterEvent *event) override;  // 处理鼠标进入事件
    void leaveEvent(QEvent *event) override;       // 处理鼠标离开事件
    void mouseMoveEvent(QMouseEvent *event) override;  // 处理鼠标移动事件
    bool eventFilter(QObject *obj, QEvent *event) override;  // 事件过滤器

private:
    Ui::LoginDialog *ui;
    CutePetWidget *_petWidget;  // Q版小宠物控件
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
    //缓存用户uid、token等信息
    std::shared_ptr<ServerInfo> _si;

    // 动态背景相关
    QTimer* _ani_timer;
    int _ani_offset;
    QPixmap _bg_pixmap;

    void updateBackground();
    void updateAnimation();

public slots:
    void slot_forget_pwd();

signals:
    //切换到注册页面
    void switchRegister();
    //切换到重置密码页面
    void switchReset();
    //连接聊天服务器（基于tcp长连接）
    void sig_connect_tcp(std::shared_ptr<ServerInfo>);
    //连接资源服务器（基于tcp长连接）
    void sig_connect_res_server(std::shared_ptr<ServerInfo>);

private slots:
    //点击登录
    void on_login_btn_clicked();
    void slot_login_mod_finish(ReqId id,QString res,ErrorCodes err);
    //聊天服务器连接完毕处理
    void slot_tcp_con_finish(bool bsuccess);
    //登录失败
    void slot_login_failed(int);
    //资源服务器连接完毕处理
    void slot_res_con_finish(bool bsuccess);
    // 输入框焦点事件处理
    void on_email_edit_focusIn();
    void on_pass_edit_focusIn();
};

#endif // LOGINDIALOG_H
