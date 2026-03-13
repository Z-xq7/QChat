#include "global.h"
#include <QPainter>

QString gate_url_prefix = "";

std::function<void(QWidget*)> repolish = [](QWidget* w){
    w->style()->unpolish(w);
    w->style()->polish(w);
};

std::function<QString(QString)> xorString = [](QString input){
    QString result = input;
    int length = input.length();
    length = length % 255;
    for(int i= 0;i<length;++i){
        //对每个字符进行异或操作
        result[i] = QChar(static_cast<ushort>(input[i].unicode()^static_cast<ushort>(length)));
    }
    return result;
};

QString generateUniqueFileName(const QString& originalName){

    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QFileInfo fileInfo(originalName);
    QString extension = fileInfo.suffix();
    return uuid + (extension.isEmpty() ? "" : "." + extension);
}

QString generateUniqueIconName(){
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return uuid + ".png";
}

QString calculateFileHash(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    QCryptographicHash hash(QCryptographicHash::Md5);

    // 分块计算哈希，避免大文件占用过多内存
    const qint64 chunkSize = 1024 * 1024; // 1MB
    while (!file.atEnd())
    {
        hash.addData(file.read(chunkSize));
    }
    file.close();

    return hash.result().toHex();
}

QPixmap CreateLoadingPlaceholder(int width, int height)
{
    QPixmap placeholder(width, height);
    placeholder.fill(QColor(240, 240, 240)); // 浅灰色背景

    QPainter painter(&placeholder);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制边框
    painter.setPen(QPen(QColor(200, 200, 200), 2));
    painter.drawRect(1, 1, width - 2, height - 2);

    // 绘制加载图标（简单的旋转圆圈或文字）
    QFont font;
    font.setPointSize(12);
    painter.setFont(font);
    painter.setPen(QColor(150, 150, 150));
    painter.drawText(placeholder.rect(), Qt::AlignCenter, "加载中...");

    // 可选：添加图片图标
    painter.setPen(QColor(180, 180, 180));
    QRect iconRect(width / 2 - 20, height / 2 - 40, 40, 30);
    painter.drawRect(iconRect);
    painter.drawLine(iconRect.topLeft(), iconRect.bottomRight());
    painter.drawLine(iconRect.topRight(), iconRect.bottomLeft());

    return placeholder;
}

// 全局测试数据
std::vector<QString> strs = {"hello world !",
                             "nice to meet u",
                             "New year，new life",
                             "You have to love yourself",
                             "My love is written in the wind ever since the whole world is you"};
                             
std::vector<QString> heads = {
    ":/images/head_1.jpg",
    ":/images/head_2.jpg",
    ":/images/head_3.jpg",
    ":/images/head_4.jpg",
    ":/images/head_5.jpg"
};

std::vector<QString> names = {
    "曾祥奇",
    "张静怡",
    "golang",
    "cpp",
    "java",
    "nodejs",
    "python",
    "rust"
};


