#ifndef GROUPCHATDIALOG_H
#define GROUPCHATDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include "userdata.h"
#include <memory>
#include <QMap>

class GroupChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GroupChatDialog(QWidget *parent = nullptr);
    ~GroupChatDialog();

signals:
    void sig_create_group_chat(const QString& group_name, const QVector<int>& member_uids);

private slots:
    void on_search_text_changed(const QString& text);
    void on_item_checked(QTreeWidgetItem* item, int column);
    void on_confirm_clicked();
    void on_cancel_clicked();
    void on_remove_selected_item(int uid);

private:
    void initUi();
    void loadFriends();
    void updateSelectedCount();
    void addSelectedFriend(std::shared_ptr<UserInfo> user);
    void removeSelectedFriend(int uid);

    QLineEdit* m_searchEdit;
    QTreeWidget* m_friendsTree;
    QListWidget* m_selectedList;
    QLabel* m_countLabel;
    QPushButton* m_confirmBtn;
    QPushButton* m_cancelBtn;

    QTreeWidgetItem* m_recentGroup;
    QTreeWidgetItem* m_friendsGroup;

    // 已选中的好友：uid -> UserInfo
    QMap<int, std::shared_ptr<UserInfo>> m_selectedFriends;
    // 好友对应的树节点：uid -> QTreeWidgetItem
    QMap<int, QTreeWidgetItem*> m_uidToTreeItem;
};

// 自定义已选好友 Item 控件
class SelectedFriendItem : public QWidget {
    Q_OBJECT
public:
    explicit SelectedFriendItem(std::shared_ptr<UserInfo> user, QWidget* parent = nullptr);
signals:
    void sig_remove(int uid);
private:
    int m_uid;
};

#endif // GROUPCHATDIALOG_H
