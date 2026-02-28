#ifndef RESETDIALOG_H
#define RESETDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QPixmap>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include "global.h"

namespace Ui {
class ResetDialog;
}

class ResetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResetDialog(QWidget *parent = nullptr);
    ~ResetDialog();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void on_return_btn_clicked();

    void on_varify_btn_clicked();

    void slot_reset_mod_finish(ReqId id, QString res, ErrorCodes err);
    void on_sure_btn_clicked();

private:
    bool checkUserValid();
    bool checkPassValid();
    void showTip(QString str,bool b_ok);
    bool checkEmailValid();
    bool checkVarifyValid();
    void AddTipErr(TipErr te,QString tips);
    void DelTipErr(TipErr te);
    void initHandlers();
    Ui::ResetDialog *ui;
    QMap<TipErr, QString> _tip_errs;
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;
    
    // 动态背景相关
    QTimer* _ani_timer;
    int _ani_offset;
    QPixmap _bg_pixmap;
    void updateBackground();

signals:
    void switchLogin();

private slots:
    void updateAnimation();
};

#endif // RESETDIALOG_H
