#include "adduseritem.h"
#include "ui_adduseritem.h"
#include <QDebug>

AddUserItem::AddUserItem(QWidget *parent)
    : ListItemBase(parent)
    , ui(new Ui::AddUserItem)
{
    ui->setupUi(this);
    SetItemType(ListItemType::ADD_USER_TIP_ITEM);
    qDebug() << "adduseritem created !";
}

AddUserItem::~AddUserItem()
{
    delete ui;
}
