# Qt-SerialAssistant

基于 Qt Widgets 的串口调试助手，面向日常串口联调与简易协议分析。

## 功能

- 串口参数配置：端口、波特率、数据位、校验位、停止位、流控
- 打开 / 关闭串口，端口列表定时刷新
- 收发显示：HEX / ASCII 切换
- 手动发送、定时循环发送
- 异步写队列：发送排队、完成/失败回调、超时取消
- 协议工作台：
  - 分帧：静默间隔 / 分隔符 / 固定长度
  - 帧列表：时间戳、方向、HEX、摘要
  - 筛选：方向、协议、站号、功能码、CRC、关键字
  - Modbus RTU 解码（含 CRC 校验）
- 收发字节计数
- 设置记忆（`QSettings`）
- 中英文界面（默认中文，可选 `en_US`）

## 架构

```
MainWindow
    └── SerialSessionController
            ├── SerialPortManager ── QSerialPort
            │         └── AsyncWriteQueue
            ├── FrameAssembler
            ├── ModbusRtuDecoder
            └── FrameLogModel
```

| 模块 | 职责 |
|------|------|
| `SerialPortManager` | 串口生命周期、读写、定时发送触发 |
| `AsyncWriteQueue` | 非阻塞写队列 |
| `SerialSessionController` | 会话编排：分帧、计数、日志、自动发送 |
| `FrameAssembler` | 按配置把字节流切成帧 |
| `ModbusRtuDecoder` | Modbus RTU 帧解析 |
| `FrameLogModel` | 帧日志表格模型 |
| `MainWindow` | UI 与用户交互 |

## 环境要求

- Qt 6（开发环境验证：Qt 6.5.3 + MinGW 64-bit）
- 模块：`core` / `gui` / `widgets` / `serialport`
- C++17
- Windows / Linux / macOS（依赖系统串口驱动）

## 构建

### Qt Creator

1. 打开 `SerialAssistant.pro`
2. 选择带 Serial Port 模块的 Kit
3. 构建并运行

### 命令行（qmake）

```bash
qmake SerialAssistant.pro
make            # Windows MinGW: mingw32-make
```

### 单元测试

```bash
cd tests
qmake tests.pro
make
./tests         # 或 tests.exe
```

覆盖 HEX/ASCII 转换、分帧、写队列、会话与日志等逻辑。

## 使用提示

1. 左侧选择端口与通信参数，点击「打开串口」
2. 右侧输入发送内容，切换 HEX/ASCII 后发送
3. 需要循环发包时勾选定时发送并设置间隔（ms）
4. 协议工作台可切换分帧方式，在「帧列表」中查看解析结果
5. 未打开串口时发送/定时发送会被拦截并提示

## 目录结构

```
.
├── main.cpp / mainwindow.*     # 入口与 UI
├── serialportmanager.*         # 串口封装
├── asyncwritequeue.*           # 异步写队列
├── serialsessioncontroller.*   # 会话控制
├── frameassembler.*            # 分帧
├── framelogmodel.*             # 帧日志模型
├── modbusrtudecoder.*          # Modbus RTU 解码
├── frametypes.h / utils.*      # 类型与工具
├── tests/                      # 单元测试
└── docs/                       # 设计与验证文档
```

## 许可证

未指定许可证。若要开源分发，请自行补充 `LICENSE`。
