#ifndef GROUPSIDEPAGE_H
#define GROUPSIDEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTimer>
#include "clickedlabel.h"
#include "userdata.h"

class GroupSidePage : public QWidget
{
    Q_OBJECT
public:
    explicit GroupSidePage(QWidget *parent = nullptr);

    // 设置群聊数据
    void SetGroupData(std::shared_ptr<ChatThreadData> group_data);
    // 获取当前用户角色 (0=普通成员, 1=管理员, 2=群主)
    int GetCurrentUserRole();

signals:
    // 查看群成员列表
    void sig_view_members(int thread_id);
    // 邀请好友
    void sig_invite_friend(int thread_id);
    // 清空聊天记录
    void sig_clear_history(int thread_id);
    // 解散群聊 (群主)
    void sig_dismiss_group(int thread_id);
    // 退出群聊 (普通成员)
    void sig_exit_group(int thread_id);
    // 举报群聊
    void sig_report_group(int thread_id);
    // 设置置顶
    void sig_set_top(int thread_id, bool top);
    // 设置免打扰
    void sig_set_mute(int thread_id, bool mute);
    // 转让群聊
    void sig_transfer_group(int thread_id);
    // 编辑群公告
    void sig_update_notice(int thread_id, const QString& notice);
    // 编辑本群昵称
    void sig_edit_nickname(int thread_id);
    // 发言权限设置
    void sig_speak_permission(int thread_id);
    // 开放设置
    void sig_open_settings(int thread_id);

private slots:
    void on_search_timeout();
    void on_view_members_clicked();
    void on_invite_clicked();
    void on_clear_history_clicked();
    void on_dismiss_exit_clicked();
    void on_report_clicked();
    void on_transfer_clicked();
    void on_notice_clicked();
    void on_nickname_clicked();
    void on_speak_permission_clicked();
    void on_open_settings_clicked();
    void on_profile_settings_clicked();
    void on_remark_clicked();

protected:
    // 让GroupSidePage具备样式表渲染能力
    void paintEvent(QPaintEvent *event) override;

private:
    void initUi();
    void updateUi();
    QWidget* createSwitchItem(const QString& text, bool checked, std::function<void(bool)> callback);
    QWidget* createNavItem(const QString& text, const QString& value = "", bool hasArrow = true);
    QPushButton* createButtonItem(const QString& text, const QString& style = "");
    QWidget* createSectionTitle(const QString& text);
    
    // 群聊信息区域
    QWidget* createGroupInfoSection();
    // 群成员区域
    QWidget* createMembersSection();
    // 设置区域
    QWidget* createSettingsSection();
    // 管理区域
    QWidget* createManageSection();

    QLineEdit* m_searchEdit;
    QTimer* m_searchTimer;
    
    std::shared_ptr<ChatThreadData> _group_data;
    int _current_role;  // 0=普通成员, 1=管理员, 2=群主
    
    // UI 元素
    QLabel* m_groupNameLabel;
    QLabel* m_groupIdLabel;
    QLabel* m_memberCountLabel;
    QPushButton* m_noticeBtn;
    QPushButton* m_nicknameBtn;
    QPushButton* m_remarkBtn;
    QPushButton* m_transferBtn;
    QPushButton* m_dismissExitBtn;
    QWidget* m_membersContainer;
    QHBoxLayout* m_membersLayout;
};

#endif // GROUPSIDEPAGE_H
