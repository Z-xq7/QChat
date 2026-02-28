#include "global.h"

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
