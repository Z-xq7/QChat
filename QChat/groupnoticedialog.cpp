#include "groupnoticedialog.h"
#include <QStyleOption>
#include <QPainter>

GroupNoticeDialog::GroupNoticeDialog(const QString& current_notice, QWidget *parent)
    : QDialog(parent)
{
    this->setWindowTitle("编辑群公告");
    this->setFixedSize(400, 300);
    this->setObjectName("GroupNoticeDialog");
    this->setStyleSheet("QDialog#GroupNoticeDialog { background-color: #ffffff; border-radius: 10px; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    QLabel* titleLabel = new QLabel("群公告", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #333333;");
    mainLayout->addWidget(titleLabel);

    m_noticeEdit = new QTextEdit(this);
    m_noticeEdit->setPlaceholderText("在这里输入群公告...");
    m_noticeEdit->setText(current_notice);
    m_noticeEdit->setStyleSheet(
        "QTextEdit { background-color: #f5f5f5; border: 1px solid #e0e0e0; border-radius: 5px; "
        "padding: 10px; font-size: 14px; color: #333333; }"
        "QTextEdit:focus { border: 1px solid #00c0ff; }"
    );
    mainLayout->addWidget(m_noticeEdit);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_cancelBtn = new QPushButton("取消", this);
    m_cancelBtn->setFixedSize(80, 32);
    m_cancelBtn->setStyleSheet(
        "QPushButton { background-color: #f0f0f0; border-radius: 5px; color: #333333; border: none; }"
        "QPushButton:hover { background-color: #e0e0e0; }"
    );
    
    m_saveBtn = new QPushButton("保存", this);
    m_saveBtn->setFixedSize(80, 32);
    m_saveBtn->setStyleSheet(
        "QPushButton { background-color: #00c0ff; border-radius: 5px; color: #ffffff; border: none; }"
        "QPushButton:hover { background-color: #00b0ee; }"
    );

    btnLayout->addWidget(m_saveBtn);
    btnLayout->addWidget(m_cancelBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_saveBtn, &QPushButton::clicked, this, &GroupNoticeDialog::on_save_clicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &GroupNoticeDialog::on_cancel_clicked);
}

QString GroupNoticeDialog::GetNotice() const
{
    return m_noticeEdit->toPlainText();
}

void GroupNoticeDialog::on_save_clicked()
{
    this->accept();
}

void GroupNoticeDialog::on_cancel_clicked()
{
    this->reject();
}
