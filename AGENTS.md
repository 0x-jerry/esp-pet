# ESP-PET 开发指南

> 基于 ESP32-C6 的 AI 虚拟宠物，配备 ST7789 240×240 SPI 屏幕、两个物理按键，支持 Xbox 无线手柄 BLE 连接。

---

## 1. 构建与烧录

所有 ESP-IDF 操作优先通过 VS Code 命令面板（`Ctrl+Shift+P`）运行。**如需在终端执行，必须使用 `eim run "idf.py ..."`**，禁止直接调用 `idf.py`。

| 操作 | ESP-IDF 命令 |
|------|-------------|
| 设置目标芯片 | `ESP-IDF: Set ESP32-C6 target` |
| 完整构建 | `ESP-IDF: Build your project` |
| 烧录固件 | `ESP-IDF: Flash your project` |
| 构建 + 烧录 + 监视 | `ESP-IDF: Build, Flash and start a monitor` |
| 监视串口 | `ESP-IDF: Monitor device` |
| 清除构建 | `ESP-IDF: Full Clean` |
| 配置 Kconfig | `ESP-IDF: SDK Configuration Editor (menuconfig)` |
| 保存默认配置 | `ESP-IDF: Save SDK Configuration defaults` |
| 查看内存使用 | `ESP-IDF: Size analysis` / `ESP-IDF: Size analysis (components)` |

### 分区表

`partitions.csv`（4MB Flash）：

| 分区 | 类型 | 偏移 | 大小 |
|------|------|------|------|
| nvs | data | 0x9000 | 24KB |
| phy_init | data | 0xf000 | 4KB |
| factory | app | 0x10000 | 3MB |

## 2. 开发工作流

### 新增功能的一般步骤

1. **更新 `plan.md`** — 记录设计思路和决策
2. **创建或修改模块**
   - 头文件使用 `#pragma once`，公共 API 加 Doxygen 注释
   - 日志：每个文件定义自己的 `TAG`，使用 `ESP_LOGI`/`ESP_LOGE`
   - 内存：大块内存优先 `MALLOC_CAP_SPIRAM` → `MALLOC_CAP_DMA`
   - 错误：关键调用用 `ESP_ERROR_CHECK()`，非关键用返回值判断
3. **更新 `main/CMakeLists.txt`** — 在 `idf_component_register(SRCS ...)` 添加新源文件，在 `INCLUDE_DIRS` 添加新目录
4. **添加组件依赖** — 使用 `eim run "idf.py add-dependency \"main/<组件名>\""` 添加依赖（如 `eim run "idf.py add-dependency \"main/esp_wifi\""`），**禁止手动编辑 `REQUIRES`**
5. **修改 Kconfig 配置** — 编辑 `sdkconfig.defaults` 添加或修改配置项，**禁止直接编辑 `sdkconfig`**
6. **构建测试** — 运行 `ESP-IDF: Build` → 修复编译错误
7. **烧录验证** — 运行 `Build, Flash and start a monitor` → 观察串口日志和屏幕输出

### 检查清单

- [ ] 头文件使用 `#pragma once`
- [ ] 公共 API 有 Doxygen 风格注释
- [ ] `main/CMakeLists.txt` 已更新源文件和包含目录
- [ ] `sdkconfig.defaults` 已更新（如有新 Kconfig 选项），**禁止直接编辑 `sdkconfig`**
- [ ] `partitions.csv` 已更新（如需调整分区）
- [ ] 构建通过（`ESP-IDF: Build`）

## 3. 调试技巧

### 串口监视器

通过 `ESP-IDF: Monitor device` 打开。

| 快捷键 | 功能 |
|--------|------|
| `Ctrl + ]` | 退出监视器 |
| `Ctrl + T` → `Ctrl + R` | 重置设备 |

### 日志过滤

在终端中执行（无 ESP-IDF 命令替代）：

```powershell
eim run "idf.py monitor --print-filter=\"xbox_ble: I,main: I\""
# 格式：<tag>:<level>  level: N/E/W/I/V/D
```
