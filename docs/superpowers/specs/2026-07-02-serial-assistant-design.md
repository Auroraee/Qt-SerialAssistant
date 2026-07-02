# SerialAssistant 串口助手设计文档

**日期：** 2026-07-02  
**主题：** 基于空白 Qt Widgets 项目实现串口助手  
**状态：** 已批准，待实施

---

## 1. 目标

基于现有空白 Qt 项目 `D:\QtProjects\SerialAssistant`，实现一个最小可用的串口助手，支持：

- 选择串口端口与波特率
- 打开/关闭串口
- 接收数据并以 HEX 或 ASCII 显示
- 手动发送数据，支持 HEX / ASCII 输入切换
- 定时循环发送

---

## 2. 架构

采用 **SerialPortManager 封装方案**：串口通信逻辑与 UI 解耦。

```
┌─────────────────┐      信号/槽       ┌──────────────────────┐
│   MainWindow    │  ◄──────────────►  │  SerialPortManager   │
│   (UI + 控制)    │                   │   (QSerialPort 封装)  │
└─────────────────┘                   └──────────────────────┘
```

### 2.1 类职责

#### `SerialPortManager`

- 持有 `QSerialPort` 实例，管理串口生命周期
- 枚举可用端口
- 实现 `open(portName, baudRate)` / `close()`
- 实现 `sendData(QByteArray)`
- 内部维护 `QTimer` 实现定时发送
- 发送以下信号：
  - `dataReceived(QByteArray)`：收到原始字节数据
  - `errorOccurred(QString)`：串口错误提示
  - `connectionStateChanged(bool)`：连接状态变化

#### `MainWindow`

- 负责所有 UI 控件的布局与状态更新
- 将用户操作转发给 `SerialPortManager`
- 接收 `SerialPortManager` 信号并更新界面

---

## 3. UI 布局

采用 **左侧配置 + 右侧收发** 布局（方案 A）。

### 3.1 左侧配置面板

| 控件 | 类型 | 说明 |
|------|------|------|
| 端口 | `QComboBox` | 枚举系统可用串口，可刷新 |
| 波特率 | `QComboBox` | 预设 9600 / 19200 / 38400 / 57600 / 115200 / 自定义 |
| 打开/关闭 | `QPushButton` | 切换串口连接状态 |
| 接收格式 | `QPushButton` ×2 | HEX / ASCII 切换 |
| 发送格式 | `QPushButton` ×2 | HEX / ASCII 切换 |
| 状态 | `QLabel` | 显示"已连接/未连接" |

### 3.2 右侧收发区

| 控件 | 类型 | 说明 |
|------|------|------|
| 接收区 | `QPlainTextEdit` | 只读，等宽字体，追加显示 |
| 发送输入框 | `QLineEdit` | 用户输入待发送内容 |
| 发送按钮 | `QPushButton` | 触发单次发送 |
| 定时发送 | `QCheckBox` + `QLineEdit` | 启用/禁用，间隔 ms |

---

## 4. 数据流

### 4.1 打开串口

1. 用户选择端口、波特率，点击"打开"
2. `MainWindow` 调用 `SerialPortManager::open(portName, baudRate)`
3. 成功：emit `connectionStateChanged(true)` → UI 更新为"关闭串口"、状态"已连接"
4. 失败：emit `errorOccurred(QString)` → 弹 `QMessageBox` 提示

### 4.2 接收数据

1. `QSerialPort::readyRead` 触发
2. `SerialPortManager` 读取 `QByteArray` 并 emit `dataReceived(QByteArray)`
3. `MainWindow` 根据当前接收格式（HEX/ASCII）转换并追加到接收区

### 4.3 发送数据

1. 用户点击"发送"
2. `MainWindow` 根据发送格式把输入框内容转换为 `QByteArray`
3. 调用 `SerialPortManager::sendData(QByteArray)`
4. 失败通过 `errorOccurred` 提示

### 4.4 定时发送

- `SerialPortManager` 内部使用 `QTimer`
- UI 勾选"定时发送"并输入间隔后启动
- 关闭串口或取消勾选时停止定时器
- 定时器触发时复用 `sendData()` 发送当前输入框内容

---

## 5. 错误处理

| 场景 | 处理方式 |
|------|----------|
| 端口不存在 / 被占用 | 打开失败时弹 `QMessageBox`，显示具体错误 |
| 未连接时点击发送 | 禁用发送按钮或提示"请先打开串口" |
| HEX 输入非法 | 提示"发送内容包含无效 HEX 字符" |
| 定时间隔非法 | 自动恢复为 1000ms 并提示 |
| 程序退出 | `MainWindow` 析构时调用 `close()`，释放串口 |

---

## 6. 文件变更

| 文件 | 操作 | 说明 |
|------|------|------|
| `SerialAssistant.pro` | 修改 | 添加 `serialport` 模块和新源文件 |
| `mainwindow.ui` | 修改 | 按方案 A 布置控件 |
| `mainwindow.h` | 修改 | 添加槽函数和成员变量 |
| `mainwindow.cpp` | 修改 | 实现 UI 与 `SerialPortManager` 交互 |
| `serialportmanager.h` | 新增 | 串口管理类声明 |
| `serialportmanager.cpp` | 新增 | 串口管理类实现 |

---

## 7. 非目标（明确排除）

以下功能不在本次范围内，避免 scope creep：

- 数据位 / 校验位 / 停止位配置（使用 Qt 默认值）
- 串口日志保存到文件
- 多串口同时连接
- 发送历史 / 快捷指令
- 十六进制自动空格补全

---

## 8. 成功标准

- [ ] 能正确枚举本机串口
- [ ] 能选择波特率并打开/关闭串口
- [ ] 能接收数据并按 HEX/ASCII 显示
- [ ] 能按 HEX/ASCII 格式发送数据
- [ ] 能启用定时发送并调节间隔
- [ ] 程序退出时不残留串口占用
