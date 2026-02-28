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
