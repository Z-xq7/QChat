#include "groupnoticedlg.h"
#include <QStyleOption>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QFrame>

GroupNoticeViewDialog::GroupNoticeViewDialog(const QString& notice, QWidget *parent)
    : QDialog(parent)
{
    this->setWindowTitle("群公告");
    this->setFixedSize(420, 320);
    this->setObjectName("GroupNoticeViewDialog");
    this->setStyleSheet(
        "QDialog#GroupNoticeViewDialog { background-color: #ffffff; border-radius: 10px; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(25, 25, 25, 20);
    mainLayout->setSpacing(15);

    // 标题
    QLabel* titleLabel = new QLabel("📢 群公告", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #333333;");
    mainLayout->addWidget(titleLabel);

    // 分割线
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("QFrame { color: #e8e8e8; max-height: 1px; }");
    mainLayout->addWidget(line);

    // 公告内容（只读）
    QTextEdit* noticeDisplay = new QTextEdit(this);
    noticeDisplay->setReadOnly(true);
    noticeDisplay->setPlaceholderText("暂无群公告");

    if (notice.isEmpty()) {
        noticeDisplay->setHtml(
            "<p style='color: #999999; font-size: 14px; text-align: center; padding-top: 30px;'>暂无群公告</p>"
        );
    } else {
        noticeDisplay->setPlainText(notice);
    }

    noticeDisplay->setStyleSheet(
        "QTextEdit { background-color: #f8f9fa; border: 1px solid #e8e8e8; border-radius: 8px; "
        "padding: 15px; font-size: 14px; color: #333333; line-height: 1.6; }"
    );
    mainLayout->addWidget(noticeDisplay);

    // 关闭按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton* closeBtn = new QPushButton("我知道了", this);
    closeBtn->setFixedSize(100, 36);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { background-color: #00c0ff; border-radius: 6px; color: #ffffff; "
        "border: none; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #00b0ee; }"
        "QPushButton:pressed { background-color: #0099d6; }"
    );
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);
}
