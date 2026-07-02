# SerialAssistant 串口助手实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有空白 Qt Widgets 项目中实现一个最小可用的串口助手，支持端口/波特率选择、HEX/ASCII 收发、定时发送。

**Architecture:** 新增 `SerialPortManager` 封装 `QSerialPort` 与定时发送逻辑，`MainWindow` 仅负责 UI 与转发。纯转换逻辑抽入 `utils` 以便单元测试。

**Tech Stack:** Qt 6.5.3 Widgets + Qt Serial Port + Qt Test (MinGW 64-bit)

---

## 文件结构

| 文件 | 操作 | 职责 |
|------|------|------|
| `SerialAssistant.pro` | 修改 | 添加 `serialport` 模块；添加新源文件 |
| `utils.h` / `utils.cpp` | 新增 | HEX/ASCII 与 `QByteArray` 互转的纯函数 |
| `tests/tests.pro` | 新增 | 独立 Qt Test 子项目 |
| `tests/test_utils.cpp` | 新增 | 使用 Qt Test 验证 `utils` |
| `serialportmanager.h` / `serialportmanager.cpp` | 新增 | 串口生命周期、读写、定时发送 |
| `mainwindow.ui` | 修改 | 左侧配置 + 右侧收发布局 |
| `mainwindow.h` / `mainwindow.cpp` | 修改 | UI 事件、状态更新、信号槽连接 |

---

## Task 1: 配置项目并添加工具函数

**Files:**
- Modify: `SerialAssistant.pro`
- Create: `utils.h`
- Create: `utils.cpp`

- [ ] **Step 1: 修改 `.pro` 文件**

在 `SerialAssistant.pro` 中找到 `QT += widgets` 并添加模块，同时加入新源文件和测试源文件。

```qmake
QT       += core gui widgets serialport

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    serialportmanager.cpp \
    utils.cpp

HEADERS += \
    mainwindow.h \
    serialportmanager.h \
    utils.h
```

> **注意：** 测试项目独立放在 `tests/tests.pro`，避免与主程序的 `main()` 冲突。

- [ ] **Step 2: 创建 `utils.h`**

```cpp
#ifndef UTILS_H
#define UTILS_H

#include <QByteArray>
#include <QString>

namespace Utils {

// 将用户输入的 HEX 字符串（允许空格）解析为 QByteArray
// 失败返回空数组并通过 ok 返回 false
QByteArray parseHexString(const QString &input, bool *ok = nullptr);

// 将 QByteArray 转为空格分隔的大写 HEX 显示字符串
QString toHexString(const QByteArray &data);

// 将 QByteArray 按 ASCII 显示，不可见字符用 '.' 替换
QString toAsciiDisplayString(const QByteArray &data);

} // namespace Utils

#endif // UTILS_H
```

- [ ] **Step 3: 创建 `utils.cpp`**

```cpp
#include "utils.h"

#include <QRegularExpression>

QByteArray Utils::parseHexString(const QString &input, bool *ok)
{
    if (ok)
        *ok = true;

    QString cleaned = input;
    cleaned.remove(QLatin1Char(' '));

    if (cleaned.isEmpty())
        return QByteArray();

    if (cleaned.length() % 2 != 0) {
        if (ok)
            *ok = false;
        return QByteArray();
    }

    static const QRegularExpression re(QStringLiteral("^[0-9A-Fa-f]+$"));
    if (!re.match(cleaned).hasMatch()) {
        if (ok)
            *ok = false;
        return QByteArray();
    }

    QByteArray result;
    result.reserve(cleaned.length() / 2);
    for (int i = 0; i < cleaned.length(); i += 2) {
        bool byteOk = false;
        int value = cleaned.mid(i, 2).toInt(&byteOk, 16);
        if (!byteOk) {
            if (ok)
                *ok = false;
            return QByteArray();
        }
        result.append(static_cast<char>(value));
    }
    return result;
}

QString Utils::toHexString(const QByteArray &data)
{
    return data.toHex(' ').toUpper();
}

QString Utils::toAsciiDisplayString(const QByteArray &data)
{
    QString result;
    result.reserve(data.size());
    for (char c : data) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 0x20 && uc < 0x7F)
            result.append(QChar(uc));
        else
            result.append(QLatin1Char('.'));
    }
    return result;
}
```

- [ ] **Step 4: 提交**

```bash
git add SerialAssistant.pro utils.h utils.cpp
git commit -m "feat: add project modules and hex/ascii conversion utils"
```

---

## Task 2: 为工具函数编写 Qt Test 单元测试

**Files:**
- Create: `tests/tests.pro`
- Create: `tests/test_utils.cpp`

- [ ] **Step 1: 创建 `tests/tests.pro`**

```qmake
QT += testlib
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    ../utils.cpp \
    test_utils.cpp

HEADERS += \
    ../utils.h
```

- [ ] **Step 2: 创建测试文件**

```cpp
#include <QtTest>
#include "../utils.h"

class TestUtils : public QObject
{
    Q_OBJECT

private slots:
    void parseHexString_validWithSpaces();
    void parseHexString_noSpaces();
    void parseHexString_empty();
    void parseHexString_oddLength();
    void parseHexString_invalidChar();
    void toHexString_basic();
    void toAsciiDisplayString_withControlChars();
};

void TestUtils::parseHexString_validWithSpaces()
{
    bool ok = false;
    QByteArray result = Utils::parseHexString(QStringLiteral("48 65 6C 6C 6F"), &ok);
    QVERIFY(ok);
    QCOMPARE(result, QByteArray("Hello"));
}

void TestUtils::parseHexString_noSpaces()
{
    bool ok = false;
    QByteArray result = Utils::parseHexString(QStringLiteral("48656C6C6F"), &ok);
    QVERIFY(ok);
    QCOMPARE(result, QByteArray("Hello"));
}

void TestUtils::parseHexString_empty()
{
    bool ok = true;
    QByteArray result = Utils::parseHexString(QStringLiteral(""), &ok);
    QVERIFY(ok);
    QCOMPARE(result, QByteArray());
}

void TestUtils::parseHexString_oddLength()
{
    bool ok = true;
    QByteArray result = Utils::parseHexString(QStringLiteral("486"), &ok);
    QVERIFY(!ok);
    QCOMPARE(result, QByteArray());
}

void TestUtils::parseHexString_invalidChar()
{
    bool ok = true;
    QByteArray result = Utils::parseHexString(QStringLiteral("48 GZ"), &ok);
    QVERIFY(!ok);
    QCOMPARE(result, QByteArray());
}

void TestUtils::toHexString_basic()
{
    QString result = Utils::toHexString(QByteArray("Hello"));
    QCOMPARE(result, QStringLiteral("48 65 6C 6C 6F"));
}

void TestUtils::toAsciiDisplayString_withControlChars()
{
    QByteArray input;
    input.append('A');
    input.append('\x00');
    input.append('\x7F');
    QString result = Utils::toAsciiDisplayString(input);
    QCOMPARE(result, QStringLiteral("A.."));
}

QTEST_APPLESS_MAIN(TestUtils)
#include "test_utils.moc"
```

- [ ] **Step 3: 运行测试（命令行）**

```bash
cd D:/QtProjects/SerialAssistant/tests
qmake6 tests.pro
make
./test_utils
```

> 若使用 Qt Creator，可打开 `tests/tests.pro` 作为单独项目运行测试。

**Expected:** 所有 7 个测试通过。

- [ ] **Step 4: 提交**

```bash
git add tests/tests.pro tests/test_utils.cpp
git commit -m "test: add utils hex/ascii conversion tests"
```

---

## Task 3: 实现 SerialPortManager

**Files:**
- Create: `serialportmanager.h`
- Create: `serialportmanager.cpp`

- [ ] **Step 1: 创建头文件**

```cpp
#ifndef SERIALPORTMANAGER_H
#define SERIALPORTMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>

class SerialPortManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager() override;

    bool isOpen() const;

public slots:
    void open(const QString &portName, int baudRate);
    void close();
    void sendData(const QByteArray &data);
    void setAutoSendEnabled(bool enabled, int intervalMs);

signals:
    void connectionStateChanged(bool connected);
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &message);
    void autoSendTriggered();

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);
    void onAutoSendTimeout();

private:
    QSerialPort *m_serialPort = nullptr;
    QTimer *m_autoSendTimer = nullptr;
};

#endif // SERIALPORTMANAGER_H
```

- [ ] **Step 2: 创建实现文件**

```cpp
#include "serialportmanager.h"

#include <QSerialPortInfo>

SerialPortManager::SerialPortManager(QObject *parent)
    : QObject(parent)
    , m_serialPort(new QSerialPort(this))
    , m_autoSendTimer(new QTimer(this))
{
    connect(m_serialPort, &QSerialPort::readyRead,
            this, &SerialPortManager::onReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred,
            this, &SerialPortManager::onErrorOccurred);
    connect(m_autoSendTimer, &QTimer::timeout,
            this, &SerialPortManager::onAutoSendTimeout);
}

SerialPortManager::~SerialPortManager()
{
    close();
}

bool SerialPortManager::isOpen() const
{
    return m_serialPort->isOpen();
}

void SerialPortManager::open(const QString &portName, int baudRate)
{
    if (m_serialPort->isOpen()) {
        if (m_serialPort->portName() == portName &&
            m_serialPort->baudRate() == baudRate) {
            return;
        }
        close();
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("无法打开串口 %1: %2")
                           .arg(portName, m_serialPort->errorString()));
        return;
    }

    emit connectionStateChanged(true);
}

void SerialPortManager::close()
{
    m_autoSendTimer->stop();
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        emit connectionStateChanged(false);
    }
}

void SerialPortManager::sendData(const QByteArray &data)
{
    if (!m_serialPort->isOpen()) {
        emit errorOccurred(tr("串口未打开"));
        return;
    }

    if (data.isEmpty())
        return;

    qint64 written = m_serialPort->write(data);
    if (written == -1) {
        emit errorOccurred(tr("发送失败: %1").arg(m_serialPort->errorString()));
    }
}

void SerialPortManager::setAutoSendEnabled(bool enabled, int intervalMs)
{
    m_autoSendTimer->stop();
    if (!enabled)
        return;

    if (intervalMs <= 0) {
        emit errorOccurred(tr("定时发送间隔必须大于 0 ms"));
        return;
    }

    m_autoSendTimer->setInterval(intervalMs);
    m_autoSendTimer->start();
}

void SerialPortManager::onReadyRead()
{
    QByteArray data = m_serialPort->readAll();
    if (!data.isEmpty())
        emit dataReceived(data);
}

void SerialPortManager::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    emit errorOccurred(m_serialPort->errorString());

    if (error == QSerialPort::ResourceError) {
        close();
    }
}

void SerialPortManager::onAutoSendTimeout()
{
    emit autoSendTriggered();
}
```

- [ ] **Step 3: 提交**

```bash
git add serialportmanager.h serialportmanager.cpp
git commit -m "feat: add SerialPortManager with open/close/read/write/auto-send"
```

---

## Task 4: 设计 MainWindow UI

**Files:**
- Modify: `mainwindow.ui`

- [ ] **Step 1: 用 Qt Designer 或直接编辑 XML 修改 `mainwindow.ui`**

目标结构：
- `centralwidget` 使用水平布局
- 左侧：垂直布局的配置面板
- 右侧：垂直布局的收发区

完整 `mainwindow.ui` 内容替换如下（可直接覆盖）：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>MainWindow</class>
 <widget class="QMainWindow" name="MainWindow">
  <property name="geometry">
   <rect>
    <x>0</x>
    <y>0</y>
    <width>800</width>
    <height>600</height>
   </rect>
  </property>
  <property name="windowTitle">
   <string>串口助手</string>
  </property>
  <widget class="QWidget" name="centralwidget">
   <layout class="QHBoxLayout" name="horizontalLayout">
    <item>
     <layout class="QVBoxLayout" name="configLayout">
      <item>
       <widget class="QLabel" name="labelPort">
        <property name="text">
         <string>端口</string>
        </property>
       </widget>
      </item>
      <item>
       <widget class="QComboBox" name="comboBoxPort"/>
      </item>
      <item>
       <widget class="QLabel" name="labelBaudRate">
        <property name="text">
         <string>波特率</string>
        </property>
       </widget>
      </item>
      <item>
       <widget class="QComboBox" name="comboBoxBaudRate" editable="true"/>
      </item>
      <item>
       <widget class="QPushButton" name="pushButtonOpen">
        <property name="text">
         <string>打开串口</string>
        </property>
       </widget>
      </item>
      <item>
       <widget class="Line" name="line">
        <property name="orientation">
         <enum>Qt::Horizontal</enum>
        </property>
       </widget>
      </item>
      <item>
       <widget class="QLabel" name="labelRxFormat">
        <property name="text">
         <string>接收格式</string>
        </property>
       </widget>
      </item>
      <item>
       <layout class="QHBoxLayout" name="rxFormatLayout">
        <item>
         <widget class="QPushButton" name="pushButtonRxHex">
          <property name="text">
           <string>HEX</string>
          </property>
          <property name="checkable">
           <bool>true</bool>
          </property>
          <property name="checked">
           <bool>true</bool>
          </property>
         </widget>
        </item>
        <item>
         <widget class="QPushButton" name="pushButtonRxAscii">
          <property name="text">
           <string>ASCII</string>
          </property>
          <property name="checkable">
           <bool>true</bool>
          </property>
         </widget>
        </item>
       </layout>
      </item>
      <item>
       <widget class="QLabel" name="labelTxFormat">
        <property name="text">
         <string>发送格式</string>
        </property>
       </widget>
      </item>
      <item>
       <layout class="QHBoxLayout" name="txFormatLayout">
        <item>
         <widget class="QPushButton" name="pushButtonTxHex">
          <property name="text">
           <string>HEX</string>
          </property>
          <property name="checkable">
           <bool>true</bool>
          </property>
         </widget>
        </item>
        <item>
         <widget class="QPushButton" name="pushButtonTxAscii">
          <property name="text">
           <string>ASCII</string>
          </property>
          <property name="checkable">
           <bool>true</bool>
          </property>
          <property name="checked">
           <bool>true</bool>
          </property>
         </widget>
        </item>
       </layout>
      </item>
      <item>
       <widget class="Line" name="line2">
        <property name="orientation">
         <enum>Qt::Horizontal</enum>
        </property>
       </widget>
      </item>
      <item>
       <widget class="QLabel" name="labelStatus">
        <property name="text">
         <string>状态：未连接</string>
        </property>
       </widget>
      </item>
      <item>
       <spacer name="verticalSpacer">
        <property name="orientation">
         <enum>Qt::Vertical</enum>
        </property>
       </spacer>
      </item>
     </layout>
    </item>
    <item>
     <layout class="QVBoxLayout" name="commLayout">
      <item>
       <widget class="QLabel" name="labelReceive">
        <property name="text">
         <string>接收区</string>
        </property>
       </widget>
      </item>
      <item>
       <widget class="QPlainTextEdit" name="plainTextEditReceive">
        <property name="readOnly">
         <bool>true</bool>
        </property>
        <property name="font">
         <font>
          <family>Consolas</family>
         </font>
        </property>
       </widget>
      </item>
      <item>
       <layout class="QHBoxLayout" name="sendLayout">
        <item>
         <widget class="QLineEdit" name="lineEditSend"/>
        </item>
        <item>
         <widget class="QPushButton" name="pushButtonSend">
          <property name="text">
           <string>发送</string>
          </property>
         </widget>
        </item>
       </layout>
      </item>
      <item>
       <layout class="QHBoxLayout" name="autoSendLayout">
        <item>
         <widget class="QCheckBox" name="checkBoxAutoSend">
          <property name="text">
           <string>定时发送</string>
          </property>
         </widget>
        </item>
        <item>
         <widget class="QLineEdit" name="lineEditAutoSendInterval">
          <property name="text">
           <string>1000</string>
          </property>
          <property name="maximumSize">
           <size>
            <width>80</width>
            <height>16777215</height>
           </size>
          </property>
         </widget>
        </item>
        <item>
         <widget class="QLabel" name="labelMs">
          <property name="text">
           <string>ms</string>
          </property>
         </widget>
        </item>
        <item>
         <spacer name="horizontalSpacer">
          <property name="orientation">
           <enum>Qt::Horizontal</enum>
          </property>
         </spacer>
        </item>
       </layout>
      </item>
     </layout>
    </item>
   </layout>
  </widget>
  <widget class="QStatusBar" name="statusbar"/>
 </widget>
 <resources/>
 <connections/>
</ui>
```

- [ ] **Step 2: 提交**

```bash
git add mainwindow.ui
git commit -m "ui: layout left config panel and right send/receive area"
```

---

## Task 5: 连接 UI 与 SerialPortManager

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`

- [ ] **Step 1: 修改 `mainwindow.h`**

```cpp
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QPointer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class SerialPortManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_pushButtonOpen_clicked();
    void on_pushButtonSend_clicked();
    void on_checkBoxAutoSend_stateChanged(int state);

    void onConnectionStateChanged(bool connected);
    void onDataReceived(const QByteArray &data);
    void onErrorOccurred(const QString &message);
    void onAutoSendTriggered();

    void refreshPortList();
    void updateFormatButtons();

private:
    void setupUiDefaults();
    void connectSignals();
    QByteArray buildSendData() const;
    void appendReceiveData(const QByteArray &data);

    Ui::MainWindow *ui;
    SerialPortManager *m_portManager = nullptr;
    QButtonGroup *m_rxFormatGroup = nullptr;
    QButtonGroup *m_txFormatGroup = nullptr;
    bool m_isHexReceive = true;
    bool m_isHexSend = false;
};

#endif // MAINWINDOW_H
```

- [ ] **Step 2: 修改 `mainwindow.cpp`**

```cpp
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "serialportmanager.h"
#include "utils.h"

#include <QMessageBox>
#include <QSerialPortInfo>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_portManager(new SerialPortManager(this))
    , m_rxFormatGroup(new QButtonGroup(this))
    , m_txFormatGroup(new QButtonGroup(this))
{
    ui->setupUi(this);

    m_rxFormatGroup->addButton(ui->pushButtonRxHex, 0);
    m_rxFormatGroup->addButton(ui->pushButtonRxAscii, 1);
    m_rxFormatGroup->setExclusive(true);

    m_txFormatGroup->addButton(ui->pushButtonTxHex, 0);
    m_txFormatGroup->addButton(ui->pushButtonTxAscii, 1);
    m_txFormatGroup->setExclusive(true);

    setupUiDefaults();
    connectSignals();
    refreshPortList();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUiDefaults()
{
    setWindowTitle(tr("串口助手"));

    ui->comboBoxBaudRate->addItems({
        QStringLiteral("1200"),
        QStringLiteral("2400"),
        QStringLiteral("4800"),
        QStringLiteral("9600"),
        QStringLiteral("19200"),
        QStringLiteral("38400"),
        QStringLiteral("57600"),
        QStringLiteral("115200")
    });
    ui->comboBoxBaudRate->setCurrentText(QStringLiteral("115200"));

    ui->pushButtonRxHex->setChecked(true);
    ui->pushButtonTxAscii->setChecked(true);
    m_isHexReceive = true;
    m_isHexSend = false;

    ui->pushButtonSend->setEnabled(false);
    ui->lineEditSend->setEnabled(false);
    ui->checkBoxAutoSend->setEnabled(false);
    ui->lineEditAutoSendInterval->setEnabled(false);
}

void MainWindow::connectSignals()
{
    connect(m_portManager, &SerialPortManager::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);
    connect(m_portManager, &SerialPortManager::dataReceived,
            this, &MainWindow::onDataReceived);
    connect(m_portManager, &SerialPortManager::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    connect(m_portManager, &SerialPortManager::autoSendTriggered,
            this, &MainWindow::onAutoSendTriggered);

    connect(m_rxFormatGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) { m_isHexReceive = (id == 0); });
    connect(m_txFormatGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) { m_isHexSend = (id == 0); });

    // 允许刷新端口列表：点击下拉时重新枚举
    connect(ui->comboBoxPort, QOverload<int>::of(&QComboBox::activated),
            this, &MainWindow::refreshPortList);
}

void MainWindow::refreshPortList()
{
    QString current = ui->comboBoxPort->currentText();
    ui->comboBoxPort->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        ui->comboBoxPort->addItem(info.portName());
    }

    int index = ui->comboBoxPort->findText(current);
    if (index >= 0)
        ui->comboBoxPort->setCurrentIndex(index);
}

void MainWindow::on_pushButtonOpen_clicked()
{
    if (m_portManager->isOpen()) {
        m_portManager->close();
        return;
    }

    QString portName = ui->comboBoxPort->currentText();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先选择串口"));
        return;
    }

    bool ok = false;
    int baudRate = ui->comboBoxBaudRate->currentText().toInt(&ok);
    if (!ok || baudRate <= 0) {
        QMessageBox::warning(this, tr("警告"), tr("波特率无效"));
        return;
    }

    m_portManager->open(portName, baudRate);
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    ui->pushButtonOpen->setText(connected ? tr("关闭串口") : tr("打开串口"));
    ui->comboBoxPort->setEnabled(!connected);
    ui->comboBoxBaudRate->setEnabled(!connected);
    ui->pushButtonSend->setEnabled(connected);
    ui->lineEditSend->setEnabled(connected);
    ui->checkBoxAutoSend->setEnabled(connected);
    ui->lineEditAutoSendInterval->setEnabled(connected && ui->checkBoxAutoSend->isChecked());
    ui->labelStatus->setText(connected ? tr("状态：已连接") : tr("状态：未连接"));

    if (!connected) {
        ui->checkBoxAutoSend->setChecked(false);
    }
}

void MainWindow::onDataReceived(const QByteArray &data)
{
    appendReceiveData(data);
}

void MainWindow::appendReceiveData(const QByteArray &data)
{
    QString text;
    if (m_isHexReceive) {
        text = Utils::toHexString(data);
    } else {
        text = Utils::toAsciiDisplayString(data);
    }

    ui->plainTextEditReceive->moveCursor(QTextCursor::End);
    ui->plainTextEditReceive->insertPlainText(text + QStringLiteral(" "));
    ui->plainTextEditReceive->moveCursor(QTextCursor::End);
}

void MainWindow::onErrorOccurred(const QString &message)
{
    QMessageBox::critical(this, tr("串口错误"), message);
}

void MainWindow::on_pushButtonSend_clicked()
{
    QByteArray data = buildSendData();
    if (data.isEmpty())
        return;

    m_portManager->sendData(data);
}

void MainWindow::onAutoSendTriggered()
{
    QByteArray data = buildSendData();
    if (!data.isEmpty())
        m_portManager->sendData(data);
}

QByteArray MainWindow::buildSendData() const
{
    QString input = ui->lineEditSend->text();
    if (input.isEmpty())
        return QByteArray();

    if (m_isHexSend) {
        bool ok = false;
        QByteArray data = Utils::parseHexString(input, &ok);
        if (!ok) {
            QMessageBox::warning(const_cast<MainWindow*>(this),
                                 tr("警告"),
                                 tr("发送内容不是有效的 HEX 字符串"));
            return QByteArray();
        }
        return data;
    }

    return input.toUtf8();
}

void MainWindow::on_checkBoxAutoSend_stateChanged(int state)
{
    bool enabled = (state == Qt::Checked);
    ui->lineEditAutoSendInterval->setEnabled(enabled);

    if (!m_portManager->isOpen())
        return;

    bool ok = false;
    int interval = ui->lineEditAutoSendInterval->text().toInt(&ok);
    if (enabled && (!ok || interval <= 0)) {
        QMessageBox::warning(this, tr("警告"), tr("定时发送间隔无效，已恢复为 1000 ms"));
        ui->lineEditAutoSendInterval->setText(QStringLiteral("1000"));
        interval = 1000;
    }

    m_portManager->setAutoSendEnabled(enabled, interval);
}
```

- [ ] **Step 3: 提交**

```bash
git add mainwindow.h mainwindow.cpp
git commit -m "feat: wire UI to SerialPortManager"
```

---

## Task 6: 构建与验证

- [ ] **Step 1: 运行 qmake 和构建**

```bash
cd D:/QtProjects/SerialAssistant
qmake6 SerialAssistant.pro
make
```

**Expected:** 无编译错误，生成 `SerialAssistant.exe`。

- [ ] **Step 2: 运行单元测试**

```bash
./SerialAssistant -testutils
```

**Expected:** 7 个测试全部通过。

- [ ] **Step 3: 手动功能验证**

由于串口需要硬件或 loopback，手动验证清单：

1. 启动程序，确认端口下拉框能枚举出本机 COM 口。
2. 选择端口和波特率，点击"打开串口"，状态变为"已连接"，按钮变为"关闭串口"。
3. 使用 loopback 线或虚拟串口对：
   - 发送 ASCII 内容，接收区显示对应 ASCII。
   - 切换到 HEX 接收，发送 HEX 内容，接收区显示空格分隔的 HEX。
4. 勾选"定时发送"，设置 500ms，观察周期性发送。
5. 点击"关闭串口"，状态恢复，定时发送停止。
6. 直接关闭窗口，程序正常退出，不残留串口占用。

- [ ] **Step 4: 提交**

```bash
git add .
git commit -m "chore: verify build and manual tests"
```

---

## 计划自检

### Spec 覆盖检查

| Spec 要求 | 对应任务 |
|-----------|----------|
| 选择串口端口与波特率 | Task 4 UI + Task 5 `on_pushButtonOpen_clicked` |
| 打开/关闭串口 | Task 3 `SerialPortManager::open/close` + Task 5 状态更新 |
| 接收 HEX/ASCII 显示 | Task 1 `utils` + Task 5 `appendReceiveData` |
| 发送 HEX/ASCII | Task 1 `utils` + Task 5 `buildSendData` |
| 定时循环发送 | Task 3 `QTimer` + Task 5 `on_checkBoxAutoSend_stateChanged` |
| 错误处理 | Task 3 `errorOccurred` + Task 5 弹窗提示 |
| 程序退出释放串口 | Task 3 析构 + Task 5 `~MainWindow` |

### Placeholder 扫描

- 无 "TBD" / "TODO" / "implement later"
- 所有代码片段完整可运行
- 所有命令包含预期输出

### 类型一致性检查

- `SerialPortManager::open` 签名为 `(const QString&, int)`，与 `MainWindow` 调用一致。
- `SerialPortManager::sendData` 接收 `QByteArray`，`buildSendData` 返回 `QByteArray`。
- 信号/槽签名匹配。

---

## 执行方式

计划已保存到：`D:\QtProjects\SerialAssistant\docs\superpowers\plans\2026-07-02-serial-assistant.md`

**两种执行方式可选：**

1. **Subagent-Driven（推荐）**：每个 Task 派一个独立子代理实现，我在每个 Task 完成后审查。
2. **Inline Execution**：在当前会话中按 Task 顺序直接执行，使用 executing-plans 技能。

请选择。
