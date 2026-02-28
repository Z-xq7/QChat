#ifndef ADDUSERITEM_H
#define ADDUSERITEM_H
#include <QWidget>
#include "listitembase.h"

namespace Ui {
class AddUserItem;
}

class AddUserItem : public ListItemBase
{
    Q_OBJECT
public:
    explicit AddUserItem(QWidget *parent = nullptr);
    ~AddUserItem();
    QSize sizeHint() const override {
        return QSize(230, 65); // 返回自定义的尺寸
    }
protected:
private:
    Ui::AddUserItem *ui;
};

#endif // ADDUSERITEM_H
