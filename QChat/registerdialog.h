#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QPixmap>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include "global.h"

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void on_get_code_clicked();
    void slot_reg_mod_finish(ReqId id,QString res,ErrorCodes err);
    void on_sure_btn_clicked();
    void on_return_btn_clicked();
    void on_cancel_btn_clicked();

private:
    Ui::RegisterDialog *ui;
    QMap<ReqId,std::function<void(const QJsonObject&)>> _handlers;
    QMap<TipErr, QString> _tip_errs;

    void showTip(QString str,bool b_ok);
    void initHttpHandlers();
    void AddTipErr(TipErr te, QString tips);
    void DelTipErr(TipErr te);
    bool checkUserValid();
    bool checkPassValid();
    bool checkConfirmValid();
    bool checkEmailValid();
    bool checkVarifyValid();

    QTimer* _countdown_timer;
    int _countdown;

    void ChangeTipPage();
    
    // 动态背景相关
    QTimer* _ani_timer;
    int _ani_offset;
    QPixmap _bg_pixmap;
    void updateBackground();

signals:
    void sigSwitchLogin();

private slots:
    void updateAnimation();

};

#endif // REGISTERDIALOG_H
