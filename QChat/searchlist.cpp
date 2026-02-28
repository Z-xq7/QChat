#include "searchlist.h"
#include <QScrollBar>
#include "tcpmgr.h"
#include "customizeedit.h"
#include "loadingdialog.h"
#include "adduseritem.h"
#include "findsuccessdialog.h"
#include <QDebug>
#include "FindFailDlg.h"
#include "usermgr.h"

SearchList::SearchList(QWidget *parent):QListWidget(parent),_find_dlg(nullptr), _search_edit(nullptr), _send_pending(false)
{
    Q_UNUSED(parent);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 安装事件过滤器
    this->viewport()->installEventFilter(this);
    //连接点击的信号和槽
    connect(this, &QListWidget::itemClicked, this, &SearchList::slot_item_clicked);
    //添加条目
    qDebug() << "addtipitem created !";
    addTipItem();
    //连接搜索条目
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_user_search, this, &SearchList::slot_user_search);
}

void SearchList::CloseFindDlg()
{
    if(_find_dlg){
        _find_dlg->hide();
        _find_dlg = nullptr;
    }
}

void SearchList::SetSearchEdit(QWidget *edit)
{
    _search_edit = edit;
}

void SearchList::waitPending(bool pending)
{
    if(pending){
        _loadingDialog = new LoadingDialog(this);
        _loadingDialog->setModal(true);
        _loadingDialog->show();
        _send_pending = pending;
    }else{
        _loadingDialog->hide();
        _loadingDialog->deleteLater();
        _send_pending = pending;
    }
}

void SearchList::addTipItem()
{
    auto *invalid_item = new QWidget();
    QListWidgetItem *item_tmp = new QListWidgetItem;
    qDebug()<<"item_tmp sizeHint is " << item_tmp->sizeHint();
    item_tmp->setSizeHint(QSize(230,65));
    this->addItem(item_tmp);

    // invalid_item->setObjectName("invalid_item");
    // this->setItemWidget(item_tmp, invalid_item);
    // item_tmp->setFlags(item_tmp->flags() & ~Qt::ItemIsSelectable);

    auto *add_user_item = new AddUserItem();
    QListWidgetItem *item = new QListWidgetItem;
    qDebug()<<"item sizeHint is " << item->sizeHint();
    item->setSizeHint(add_user_item->sizeHint());
    this->addItem(item);
    this->setItemWidget(item, add_user_item);
}

void SearchList::slot_item_clicked(QListWidgetItem *item)
{
    QWidget *widget = this->itemWidget(item); //获取自定义widget对象
    if(!widget){
        qDebug()<< "slot item clicked widget is nullptr";
        return;
    }
    // 对自定义widget进行操作， 将item 转化为基类ListItemBase
    ListItemBase *customItem = qobject_cast<ListItemBase*>(widget);
    if(!customItem){
        qDebug()<< "slot item clicked widget is nullptr";
        return;
    }

    qDebug() << "customItem is valid";
    auto itemType = customItem->GetItemType();
    qDebug() << "itemType is:" << itemType;

    if(itemType == ListItemType::INVALID_ITEM){
        qDebug()<< "slot invalid item clicked ";
        return;
    }
    if(itemType == ListItemType::ADD_USER_TIP_ITEM){
        if(_send_pending){
            return;
        }

        if(!_search_edit){
            qDebug() << "_search_edit is null";
            return;
        }

        waitPending(true);

        //获取search_edit中的信息
        auto search_edit = dynamic_cast<CustomizeEdit*>(_search_edit);
        auto uid_str = search_edit->text();
        QJsonObject jsonObj;
        jsonObj["uid"] = uid_str;
        QJsonDocument doc(jsonObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_SEARCH_USER_REQ,jsonData);

        return;
    }
    //清除弹出框
    CloseFindDlg();
}

void SearchList::slot_user_search(std::shared_ptr<SearchInfo> si)
{
    waitPending(false);
    if(si == nullptr){
        _find_dlg = std::make_shared<FindFailDlg>(this);
    }else{
        //如果查找的是自己,先直接返回，以后可以扩充
        auto self_uid = UserMgr::GetInstance()->GetUid();
        if(si->_uid == self_uid){
            qDebug() << "--- 查找的用户为您自己->直接返回 ---";
            return;
        }

        //分两种情况，一是搜到的已经是自己的好友，一种是未添加的好友
        //已经是好友 todo。。。
        bool bExist = UserMgr::GetInstance()->CheckFriendById(si->_uid);
        if(bExist){
            //已经是好友了->直接跳转到聊天界面
            qDebug() << "--- 查找的用户已是您的好友->直接跳转到聊天界面 ---";
            emit sig_jump_chat_item(si);
            return;
        }

        //不是好友
        _find_dlg = std::make_shared<FindSuccessDialog>(this);
        std::dynamic_pointer_cast<FindSuccessDialog>(_find_dlg)->SetSearchInfo(si);
    }

    _find_dlg->show();
}
