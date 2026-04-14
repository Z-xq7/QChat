# QChat 开发规范

## 代码风格

### 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| 类名 | 大驼峰 (PascalCase) | `ChatServer`, `UserManager` |
| 函数名 | 小驼峰 (camelCase) | `sendMessage()`, `getUserInfo()` |
| 变量名 | 小驼峰 | `userName`, `messageCount` |
| 成员变量 | 下划线前缀 + 小驼峰 | `_userName`, `_socket` |
| 宏定义 | 全大写 + 下划线 | `MAX_BUFFER_SIZE` |
| 枚举 | 大驼峰 + 全大写成员 | `enum class MsgType { TEXT, IMAGE }` |
| 文件名 | 小写 + 下划线 | `chat_server.cpp`, `user_mgr.h` |

### 类定义规范

```cpp
#ifndef CLASSNAME_H
#define CLASSNAME_H

#include <QtCore>

// 前置声明
class Dependency;

/**
 * @brief 类功能简要说明
 * 
 * 详细说明类的用途、设计思路、使用示例
 */
class ClassName : public QObject {
    Q_OBJECT
public:
    explicit ClassName(QObject* parent = nullptr);
    ~ClassName();
    
    // 公共接口
    void publicMethod();
    
signals:
    void somethingHappened();
    
public slots:
    void onSomething();
    
private:
    void privateMethod();
    
    // 成员变量
    int _memberVar;
    QString _name;
};

#endif // CLASSNAME_H
```

### 文件头模板

```cpp
/*******************************************************
 * @file         filename.cpp
 * @brief        文件功能简要说明
 *
 * @author       作者名
 * @date         2026/01/01
 * @copyright    Copyright (c) 2026
 *******************************************************/
```

## 项目结构规范

### 目录组织

```
QChat/
├── core/                    # 核心模块
│   ├── network/            # 网络相关
│   ├── database/           # 数据库访问
│   └── utils/              # 工具类
├── ui/                      # UI 组件
│   ├── widgets/            # 自定义控件
│   ├── dialogs/            # 对话框
│   └── pages/              # 页面
├── models/                  # 数据模型
├── services/                # 业务逻辑服务
├── resources/               # 资源文件
│   ├── images/
│   ├── qss/
│   └── knowledge_base/     # 知识库文档
└── third_party/             # 第三方库
```

### 头文件组织顺序

```cpp
// 1. 对应的头文件（如果是 .cpp）
#include "myclass.h"

// 2. Qt 头文件（按模块分组）
#include <QWidget>
#include <QString>
#include <QNetworkAccessManager>

// 3. 标准库头文件
#include <memory>
#include <vector>
#include <map>

// 4. 第三方库头文件
#include <boost/asio.hpp>

// 5. 项目内部头文件
#include "global.h"
#include "utils/helper.h"
```

## 内存管理规范

### 智能指针使用

```cpp
// 推荐使用
std::shared_ptr<UserInfo> userInfo;
std::unique_ptr<ChatSession> session;
QSharedPointer<Message> message;

// 避免使用裸指针（除非 Qt 对象树）
ChatPage* page = new ChatPage(this);  // Qt 对象树，父对象销毁时自动释放

// 禁止
UserInfo* rawPtr = new UserInfo();  // 谁释放？
```

### Qt 对象所有权

```cpp
// 正确：指定父对象，自动内存管理
QLabel* label = new QLabel(this);
QPushButton* btn = new QPushButton(widget);

// 错误：没有父对象，需要手动 delete
QLabel* label = new QLabel();  // 内存泄漏风险
```

## 信号槽规范

### 连接方式

```cpp
// Qt5 新语法（推荐）
connect(sender, &Sender::signal, receiver, &Receiver::slot);
connect(sender, &Sender::signal, this, &MyClass::slot);

// Lambda 表达式（简单逻辑）
connect(btn, &QPushButton::clicked, this, [this]() {
    doSomething();
});

// 避免使用旧语法
connect(sender, SIGNAL(signal()), receiver, SLOT(slot()));  // 不推荐
```

### 信号槽命名

```cpp
// 信号：以 sig_ 开头
signals:
    void sig_userLogin(int uid);
    void sig_messageReceived(const QString& content);
    void sig_connectionClosed();

// 槽：以 slot_ 或 on_ 开头
private slots:
    void slot_sendMessage();
    void on_btnSend_clicked();
    void onUserStatusChanged(int uid, bool online);
```

## 错误处理规范

### 返回值设计

```cpp
// 使用 bool 表示成功/失败
bool saveUserInfo(const UserInfo& info, QString* errorMsg = nullptr);

// 使用枚举表示具体错误
enum class ErrorCode {
    Success = 0,
    NetworkError,
    DatabaseError,
    InvalidParam
};

ErrorCode connectToServer(const QString& host, int port);
```

### 异常处理

```cpp
// 构造函数可能失败时，使用工厂模式
std::shared_ptr<DatabaseConnection> DatabaseConnection::create(
    const QString& connStr, 
    QString* errorMsg) 
{
    auto conn = std::make_shared<DatabaseConnection>();
    if (!conn->initialize(connStr)) {
        if (errorMsg) *errorMsg = conn->lastError();
        return nullptr;
    }
    return conn;
}
```

## 多线程规范

### 线程安全

```cpp
class ThreadSafeClass {
public:
    void addItem(const QString& item) {
        QMutexLocker locker(&_mutex);
        _items.append(item);
    }
    
    QStringList getItems() {
        QMutexLocker locker(&_mutex);
        return _items;  // 返回副本
    }
    
private:
    QStringList _items;
    QMutex _mutex;
};
```

### 跨线程通信

```cpp
// 工作线程
class Worker : public QObject {
    Q_OBJECT
public slots:
    void doWork() {
        // 耗时操作
        emit resultReady(result);
    }
signals:
    void resultReady(const QString& result);
};

// 主线程
QThread* thread = new QThread;
Worker* worker = new Worker;
worker->moveToThread(thread);

connect(thread, &QThread::started, worker, &Worker::doWork);
connect(worker, &Worker::resultReady, this, &MyClass::handleResult);
connect(worker, &Worker::resultReady, thread, &QThread::quit);
connect(worker, &Worker::resultReady, worker, &Worker::deleteLater);
connect(thread, &QThread::finished, thread, &QThread::deleteLater);

thread->start();
```

## 日志规范

### 日志级别

```cpp
// 使用 qDebug/qInfo/qWarning/qCritical
qDebug() << "[ClassName::methodName] 调试信息";
qInfo() << "[UserLogin] User" << uid << "logged in from" << ip;
qWarning() << "[Network] Connection timeout, retrying...";
qCritical() << "[Database] Failed to connect:" << errorString;
```

### 日志格式

```cpp
// 推荐格式
qDebug() << "[" << Q_FUNC_INFO << "]" << "描述信息" << 变量1 << 变量2;

// 示例
qDebug() << "[" << Q_FUNC_INFO << "]" << "Sending message to" << uid << "content:" << content.left(50);
```

## 配置管理规范

### 配置文件读取

```cpp
// 使用 QSettings
QSettings settings("config.ini", QSettings::IniFormat);

QString dbHost = settings.value("MySQL/Host", "localhost").toString();
int dbPort = settings.value("MySQL/Port", 3306).toInt();
```

### 环境变量

```cpp
// 敏感信息使用环境变量
QString dbPassword = qEnvironmentVariable("QCHAT_DB_PASSWORD");
if (dbPassword.isEmpty()) {
    qCritical() << "Database password not set in environment!";
}
```

## 代码审查清单

### 提交前检查

- [ ] 代码可以编译通过，无警告
- [ ] 新代码有适当的注释
- [ ] 内存管理正确，无泄漏风险
- [ ] 信号槽连接正确，无悬空指针
- [ ] 多线程代码有适当的锁保护
- [ ] 日志信息清晰有用
- [ ] 遵循命名规范

### 审查关注点

1. **正确性**：逻辑是否正确，边界条件处理
2. **性能**：是否有不必要的拷贝，算法复杂度
3. **安全**：输入验证，SQL 注入防护，缓冲区溢出
4. **可维护性**：代码清晰度，注释完整性
5. **一致性**：是否符合项目规范

## Git 提交规范

### 提交信息格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Type 类型

- `feat`: 新功能
- `fix`: 修复 bug
- `docs`: 文档更新
- `style`: 代码格式调整（不影响功能）
- `refactor`: 重构
- `perf`: 性能优化
- `test`: 测试相关
- `chore`: 构建/工具相关

### 示例

```
feat(chat): 添加图片消息发送功能

- 实现图片压缩和上传
- 添加图片预览功能
- 支持点击查看大图

Closes #123
```

```
fix(network): 修复重连时的崩溃问题

当网络断开时，重连逻辑中使用了已释放的指针，
现在使用智能指针管理连接生命周期。

Fixes #456
```

## 文档规范

### 代码注释

```cpp
/**
 * @brief 计算两个用户的共同好友数
 * 
 * 使用数据库查询获取两个用户的好友列表，
 * 然后计算交集大小。
 * 
 * @param uid1 用户1的ID
 * @param uid2 用户2的ID
 * @return int 共同好友数量，-1表示查询失败
 * 
 * @example
 * int count = getCommonFriends(1001, 1002);
 * if (count >= 0) {
 *     qDebug() << "共同好友数：" << count;
 * }
 */
int getCommonFriends(int uid1, int uid2);
```

### 复杂逻辑注释

```cpp
// 使用滑动窗口算法查找最长无重复字符子串
// 时间复杂度：O(n)，空间复杂度：O(min(m, n))
// 其中 n 是字符串长度，m 是字符集大小
int lengthOfLongestSubstring(const QString& s) {
    QHash<QChar, int> charIndex;
    int maxLen = 0;
    int start = 0;
    
    for (int i = 0; i < s.length(); ++i) {
        QChar c = s[i];
        // 如果字符已存在且在窗口内，移动窗口左边界
        if (charIndex.contains(c) && charIndex[c] >= start) {
            start = charIndex[c] + 1;
        }
        charIndex[c] = i;
        maxLen = qMax(maxLen, i - start + 1);
    }
    return maxLen;
}
```
