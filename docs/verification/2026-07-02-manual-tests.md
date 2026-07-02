# SerialAssistant 手动验证报告

**日期：** 2026-07-02
**环境：** Qt 6.5.3 MinGW 64-bit, Windows 10
**限制：** 本机无可用 COM 口或虚拟串口对，部分步骤无法实际执行

## 验证清单

| # | 检查项 | 结果 | 说明 |
|---|--------|------|------|
| 1 | 端口下拉框枚举 | 通过 | 代码路径验证；本机无 COM 口，下拉框为空 |
| 2 | 打开/关闭串口，状态更新 | 未执行 | 无可用串口 |
| 3 | ASCII/HEX 收发 | 未执行 | 无 loopback 或虚拟串口对 |
| 4 | 定时发送 | 未执行 | 无可用串口 |
| 5 | 关闭串口停止定时发送 | 代码验证 | `SerialPortManager::close()` 停止定时器 |
| 6 | 关闭窗口正常退出 | 部分验证 | 程序可在 offscreen 平台启动；正常窗口关闭退出路径未在此模式下实际执行 |

## 单元测试

- `tests/release/tests.exe`: 15 test slots passed, 0 failed (Qt Test reports 17 results including `initTestCase` and `cleanupTestCase`)
- 测试执行时已将 `D:\Software\Qt\6.5.3\mingw_64\bin` 加入 `PATH`，否则可能因找不到 Qt DLL 而失败。

## 构建

- `qmake6 SerialAssistant.pro` + `mingw32-make -j4`: 成功（Qt 6.5.3 头文件会发出一条 C++20 兼容性警告，非项目代码问题）
- 生成 `release/SerialAssistant.exe`
