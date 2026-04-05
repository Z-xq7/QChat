#ifndef GROUPMEMBERLISTPAGE_H
#define GROUPMEMBERLISTPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QMap>
#include "userdata.h"

// 群成员列表项
class GroupMemberItem : public QWidget
{
    Q_OBJECT
public:
    explicit GroupMemberItem(QWidget *parent = nullptr);
    
    // 设置成员数据
    void SetMemberData(const std::shared_ptr<GroupMemberInfo>& member);
    // 获取成员ID
    int GetMemberId() const { return _member ? _member->_uid : 0; }

signals:
    void sig_item_clicked(int uid);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    std::shared_ptr<GroupMemberInfo> _member;
    QLabel* m_iconLabel;
    QLabel* m_nameLabel;
    QLabel* m_roleLabel;
};

// 群成员列表页面
class GroupMemberListPage : public QWidget
{
    Q_OBJECT
public:
    explicit GroupMemberListPage(int thread_id, QWidget *parent = nullptr);
    
    // 设置成员列表
    void SetMembers(const std::vector<std::shared_ptr<GroupMemberInfo>>& members);
    // 获取当前用户角色
    int GetCurrentUserRole() const { return _current_role; }

signals:
    // 返回按钮点击
    void sig_back();
    // 成员点击
    void sig_member_clicked(int uid);
    // 邀请好友
    void sig_invite_friend(int thread_id);
    // 移除成员 (管理员/群主)
    void sig_remove_member(int thread_id, int uid);

private slots:
    void on_search_text_changed(const QString& text);
    void on_back_clicked();
    void on_invite_clicked();
    void on_member_item_clicked(int uid);

private:
    void initUi();
    void updateMemberList();
    void filterMembers(const QString& keyword);

protected:
    // 让GroupMemberListPage具备样式表渲染能力
    void paintEvent(QPaintEvent *event) override;

    int _thread_id;
    int _current_role;
    QLineEdit* m_searchEdit;
    QListWidget* m_memberList;
    QLabel* m_titleLabel;
    QPushButton* m_inviteBtn;
    QPushButton* m_removeBtn;
    
    std::vector<std::shared_ptr<GroupMemberInfo>> _all_members;
    std::vector<std::shared_ptr<GroupMemberInfo>> _filtered_members;
};

#endif // GROUPMEMBERLISTPAGE_H
