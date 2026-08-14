# JC8012P4A1C 板级移植 + ESS 储能监控 App 实施计划

> 目标仓库：`/home/deven/workspace/esp-brookesia`（ESP-Brookesia v0.8 / master 分支）
> 目标硬件：Guition JC8012P4A1C（ESP32-P4，10.1" 800×1280 竖屏）
> 用途：自用，不提交上游，因此不受 Apache-2.0 许可证约束

---

## 0. 背景与总体判断

本计划分两大部分，彼此独立、可并行推进：

| 部分 | 内容 | 前置依赖 |
| --- | --- | --- |
| **A. 板级移植** | 让 esp-brookesia 在 JC8012P4A1C 上跑起来 | 无 |
| **B. ESS App** | 写一个储能监控应用 | 可先在 PC 仿真上开发，不必等 A 完成 |

**关键结论：B 不需要等 A。** `brookesia_hal_linux` 提供 SDL2 桌面仿真后端，UI 和数据层可以完全在 PC 上开发调试，等 A 完成后再合到真机。这条路能把两部分的风险解耦，强烈建议采用。

### 已确认的硬件规格

数据来源：第三方 BSP 仓库 [profi-max/JC8012P4A1_BSP_ESP32P4](https://github.com/profi-max/JC8012P4A1_BSP_ESP32P4)（含引脚表、原理图、可运行示例），以及本人此前在 xiaozhi-esp32 上的实机验证。

| 类别 | 参数 | 来源 |
| --- | --- | --- |
| SoC | ESP32-P4，Flash 16MB，PSRAM 32MB (HEX, 200MHz) | BSP + 官方规格书 |
| 屏幕 | JD9365，800×1280，MIPI-DSI 2 lane @1500Mbps | 实机验证 |
| DPI 时序 | 60MHz，hsync 20/20/40，vsync 4/8/20 | **实机验证（与驱动宏默认值不同）** |
| 屏幕复位 | GPIO 27 | 实机验证 |
| 背光 | GPIO 23，**LEDC 无法驱动，只能开关控制** | 实机验证 |
| 触摸 | GSL3680，RST=GPIO22，INT=GPIO21 | 实机验证 |
| 音频 | ES8311 + NS4150，PA_CTRL=**GPIO 20** | 实机验证 |
| I2S | MCLK13 / BCLK12 / WS10 / DOUT9 / DIN11 | 实机验证 |
| I2C（共用总线） | SDA=GPIO7，SCL=GPIO8 | 实机验证 |
| MIPI LDO | 通道 3，2500mV | 实机验证 |
| 摄像头 | **SC2336**，CSI 2 lane，SCCB 走 GPIO7/8 @100kHz，LDO 通道 3 / 2500mV | BSP 确认 |
| SD 卡 | SDMMC slot 0，4-bit，CLK43 / CMD44 / D0-D3 = 39/40/41/42 | BSP 确认 |
| Wi-Fi | ESP32-C6 走 ESP-Hosted SDIO 4-bit：CLK18 / CMD19 / D0-D3 = 14-17，复位 GPIO54，唤醒 GPIO6 | BSP 确认 |

### 待实测项（不阻塞开工，但必须在验收前解决）

| 编号 | 项目 | 现状 | 处理方式 |
| --- | --- | --- | --- |
| U1 | 摄像头 reset GPIO | BSP 示例中为 `-1` | 先填 `-1`，若初始化失败再查原理图 |
| U2 | 摄像头 power-down GPIO | BSP 示例中为 `-1` | 同上 |
| U3 | SD 卡供电 LDO 通道与电压 | 无文档 | 参考板用 VO4/`ldo_chan_id: 4`，先试该值 |
| U4 | SD 卡 card-detect 引脚 | 无文档 | 先不配，用轮询挂载 |
| U5 | 触摸坐标方向（mirror/swap） | 从未实机划过屏幕 | 点亮后手动标定 |

---

## 第一部分：板级移植

### 层 1：yaml 板级描述

**产出目录**：`hal/brookesia_hal_boards/boards/guition/jc8012p4a1c/`

参考对象：`hal/brookesia_hal_boards/boards/espressif/esp32_p4x_function_ev/`（同为 ESP32-P4 + MIPI-DSI + ES8311 + SC2336，I2C/I2S/LDO 引脚完全一致，可大量复用）

#### 1.1 `board_info.yaml`（5 行）

```yaml
board: jc8012p4a1c
chip: esp32p4
version: 1.0.0
description: "Guition JC8012P4A1C 10.1inch ESP32-P4 HMI Board"
manufacturer: "GUITION"
```

#### 1.2 `board_peripherals.yaml`

相对参考板的**全部改动**：

| 外设 | 参考板值 | 本板值 | 备注 |
| --- | --- | --- | --- |
| `i2c_master` | sda 7 / scl 8 | **不变** | |
| `i2s_audio_out` / `_in` | 13/12/10/9/11 | **不变** | |
| `gpio_pa_control` | pin 53 | **pin 20** | 填错会导致「服务端显示已发送音频但喇叭无声」 |
| `ldo_mipi` | chan 3 / 2500mV | **不变** | 摄像头也复用此 LDO |
| `dsi_display` | lane_bit_rate 1000 | **1500** | |
| 背光 | `ledc_backlight` (type: ledc, gpio 26) | **保留 LEDC**，仅改 pin 与 `freq_hz: 1000`（原计划改 GPIO 已推翻，见层 3） | |
| SD 卡 | 无独立 peripheral | 新增（若 `dev_fs_fat` 需要显式引脚，见下） | |

> **注**：此处原有「新增 `gpio_backlight` 条目」的 yaml 示例已删除。实测证明 LEDC 可用，
> 表象上的「驱动不了」实为 PWM 频率过高（4 kHz → MP3202 关断延迟不足），改 `freq_hz: 1000` 即解决。

#### 1.3 `board_devices.yaml`

| 设备 | 处理 |
| --- | --- |
| `audio_dac` / `audio_adc`（ES8311） | 原样复制，仅确认 I2C 地址 0x30 |
| `display_lcd` | `chip: ek79007` → **`jd9365`**；`dependencies` 换成 `espressif/esp_lcd_jd9365: "^1.0.2"`；`video_timing` 全部换成本板实测值；`h_size: 800` / `v_size: 1280` |
| `lcd_touch` | `chip: gt911` → **`gsl3680`**；`dependencies` 换成 `mangoo1/esp_lcd_touch_gsl3680`；`x_max: 800` / `y_max: 1280`；`touch_config` 增加 `rst_gpio_num: 22` / `int_gpio_num: 21`；`mirror_x`/`mirror_y` 先全设 `false`，实测后调（U5） |
| `lcd_brightness` | **保持 `type: ledc_ctrl`**，仅调整引脚与 `freq_hz: 1000`（原计划改 `gpio_ctrl` 已推翻，见层 3） |
| `camera` | **保留**。`csi_config.reset_io: -1`、`pwdn_io: -1`（U1/U2），`dont_init_ldo: true`，peripherals 引用 `i2c_master`(100kHz) 与 `ldo_mipi` |
| `fs_sdcard` | **保留**。slot 0，4-bit，`SDMMC_FREQ_HIGHSPEED`。**与参考板的关键差异**：参考板 slot 0 引脚全填 0（由驱动内部固定），本板 BSP 明确给出 GPIO 号，需实测确认填 0 还是填实际号（43/44/39/40/41/42）。`ldo_chan_id: 4`（U3） |

**风险点**：`dev_display_lcd` 的 yaml schema 是否支持传入自定义 vendor init 命令序列。若不支持（大概率不支持），init 序列必须在层 2 的 `setup_device.c` 里以 C 数组形式提供 —— 这也是本计划的既定做法，因此该风险已被规避。

#### 1.4 `sdkconfig.defaults.board`

基于参考板文件修改：

```
# 分辨率
CONFIG_BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_H_RES=800
CONFIG_BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_V_RES=1280

# 背光：沿用 LEDC（见下方「已推翻」说明），无需任何改动

# 摄像头：见下方「已推翻」说明，实际是 OV02C10 而非 SC2336
# PSRAM / Flash / L2 Cache：原样保留（16MB Flash + HEX PSRAM 与参考板一致）
```

#### 1.5 层 1 验收 QA

前置：`. /home/deven/esp/esp-idf-v6.0.2/export.sh`，工作目录 `examples/system/super`。

| # | 操作 | 期望结果 | 失败处理 |
| --- | --- | --- | --- |
| Q1.1 | `idf.py set-target esp32p4` | 退出码 0 | 检查 IDF 版本是否在 `>=6.0, <=6.2` 区间 |
| Q1.2 | `idf.py gen-bmgr-config -b jc8012p4a1c` | 退出码 0；输出中出现 `jc8012p4a1c`；生成的 `build/` 下 bmgr 代码含 `jd9365`、`gsl3680`、`sc2336`、`gpio_ctrl` 字样 | 报「board not found」→ 目录名与 `board_info.yaml` 的 `board:` 字段不一致；报 yaml 解析错误 → 逐段注释法定位 |
| Q1.3 | 检查生成产物：<br>`grep -c "jd9365\|gsl3680" build/bmgr_*/*.c` | 命中数 > 0 | 说明 `chip:` 字段名与驱动组件不匹配 |
| Q1.4 | `idf.py build` | 退出码 0，输出 `Project build complete` | 组件版本冲突 → 查 `dependencies.lock` |
| Q1.5 | 逐个确认关键值已生效：<br>`grep -rn "1500\|800\|1280" build/bmgr_*/` | DSI 速率 1500、h_size 800、v_size 1280 均出现 | yaml 字段名拼错会被静默忽略，必须逐个 grep 确认 |

> 层 1 单独无法验证背光与面板是否真能工作，Q1.1–Q1.5 仅证明「配置被正确解析并生成了代码」。功能性验证在 V3 之后。

**工作量：2–3 小时。风险：低。**

---

### 层 2：`setup_device.c`

`esp_board_manager` 的面板与触摸初始化均通过 `extern` 弱符号工厂函数完成，**没有芯片白名单**，板子可自由覆盖。

> **这两个文件不在本仓库内。** 它们属于外部组件 `espressif/esp_board_manager`（由 `hal/brookesia_hal_boards/idf_component.yml` 声明依赖，版本 `"0.5.*"`，`require: public`）。
>
> 获取方式二选一：
> - 执行过一次 `idf.py build` 后，落在 `<project>/managed_components/espressif__esp_board_manager/devices/`
> - 或直接查看上游源码：`git clone https://github.com/espressif/esp-board-manager`（本计划核对时使用的提交为 `05815cd`，2026-07-28）

| 工厂函数 | 组件内路径 | 行号 |
| --- | --- | --- |
| `lcd_dsi_panel_factory_entry_t()` | `esp_board_manager/devices/dev_display_lcd/dev_display_lcd_sub_dsi.c` | 24（声明），74（调用） |
| `lcd_touch_factory_entry_t()` | `esp_board_manager/devices/dev_lcd_touch/dev_lcd_touch_sub_i2c.c` | 24（声明），101（调用） |

同一组件的 `devices/dev_gpio_ctrl/` 提供 GPIO 控制设备类型（层 3 原计划用它，现已推翻不再需要），`devices/dev_lcd_touch/dev_lcd_touch.py:139-140` 证实 yaml schema 支持 `rst_gpio_num` / `int_gpio_num`。

#### 2.1 面板工厂函数

以参考板 `setup_device.c` 为骨架，替换为 JD9365：

```c
#if __has_include(<esp_lcd_jd9365.h>)
#define HAS_JD9365 1
#include "esp_lcd_jd9365.h"
#include "lcd_init_cmds.h"   // 从 xiaozhi-esp32 移植过来的厂商初始化序列
#endif

__attribute__((weak)) esp_err_t lcd_dsi_panel_factory_entry_t(
    esp_lcd_dsi_bus_handle_t dsi_handle,
    dev_display_lcd_config_t *lcd_cfg,
    dev_display_lcd_handles_t *lcd_handles)
{
    jd9365_vendor_config_t vendor_config = {
        .init_cmds      = jd9365_lcd_init_cmds,                 // 关键
        .init_cmds_size = sizeof(jd9365_lcd_init_cmds) / sizeof(jd9365_lcd_init_cmd_t),
        .mipi_config = {
            .dsi_bus    = dsi_handle,
            .dpi_config = &lcd_cfg->sub_cfg.dsi.dpi_config,
            .lane_num   = 2,
        },
    };
    /* ... esp_lcd_panel_dev_config_t 与参考板同构 ... */
    return esp_lcd_new_panel_jd9365(lcd_handles->io_handle, &lcd_dev_config,
                                    &lcd_handles->panel_handle);
}
```

**`lcd_init_cmds.h` 直接从 `xiaozhi-esp32/main/boards/guition/jc8012p4a1c/lcd_init_cmds.h` 复制（246 行）。**

⚠️ **此文件不可用驱动内置默认序列替代。** 实机验证结论：`esp_lcd_jd9365` 内置默认序列缺少 `{0x80, 0x01}`（DSI lane count select）。缺失时 DBI 通道仍能正常读回 panel ID `93 65 04`，但 DPI 视频通路无输出，屏幕全黑 —— 是一个极具迷惑性的故障现象，务必保留该文件原样。

#### 2.2 触摸工厂函数

```c
#if __has_include(<esp_lcd_touch_gsl3680.h>)
__attribute__((weak)) esp_err_t lcd_touch_factory_entry_t(
    esp_lcd_panel_io_handle_t io,
    const esp_lcd_touch_config_t *touch_dev_config,
    esp_lcd_touch_handle_t *ret_touch)
{
    return esp_lcd_touch_new_i2c_gsl3680(io, touch_dev_config, ret_touch);
}
#endif
```

依赖组件 `mangoo1/esp_lcd_touch_gsl3680`（GPL-2.0-or-later）。自用不涉及分发，无许可证问题；若日后考虑上游需重新评估。

该组件相对厂商原始代码含两处已修复缺陷，移植时确保使用已修版本：
1. `chazhi` 原声明为 `uint16_t`，导致 `chazhi < -900` 的缩小手势分支永远不可达
2. `distance` 原为 `uint16_t`，但存放的是平方像素距离，本屏最大可达 2278400，溢出

#### 2.3 层 2 验收 QA

| # | 操作 | 期望结果 | 失败处理 |
| --- | --- | --- | --- |
| Q2.1 | `idf.py build` | 退出码 0。链接期**不得**出现 `undefined reference to lcd_dsi_panel_factory_entry_t` | 出现该错误说明 `__has_include` 判断失败，即 `esp_lcd_jd9365` 未进入依赖树，检查 `board_devices.yaml` 的 `dependencies:` |
| Q2.2 | 确认弱符号被本板实现覆盖：<br>`idf.py size-components \| grep setup_device`，或 `nm build/*.elf \| grep lcd_dsi_panel_factory` | 符号地址落在本板 `setup_device.c` 对应的 section 内 | 若落在 board_manager 默认实现，说明 `__attribute__((weak))` 用法或链接顺序有问题 |
| Q2.3 | 核对 init 序列完整性：<br>`grep -c "0x80, 0x01" main/../boards/guition/jc8012p4a1c/lcd_init_cmds.h` | 返回 ≥ 1 | **返回 0 则必然黑屏**，且串口仍能读到 panel ID `93 65 04`，属于最易误判的故障 |
| Q2.4 | `idf.py -p /dev/ttyACM0 flash monitor` | 串口出现 `Install JD9365 LCD panel driver` 与触摸初始化日志，无 `E (` 级别报错 | 触摸 I2C 无应答 → 确认 GSL3680 需先上电复位（RST=GPIO22），且固件上传耗时较长，超时值需放宽 |

**工作量：1–2 小时。风险：低（参数均已实机验证）。**

---

### 层 3：hal_adaptor GPIO 背光实现 —— **已推翻，无需实施**

原计划认为本板背光引脚 LEDC 驱动不了，需新增 GPIO 开关式背光实现（预估 4–8 小时，改框架层）。

**实际结论：判断错误，LEDC 完全可用，框架层零改动。**

真正的原因是 **PWM 频率**：背光芯片 MP3202 在 4 kHz 下关断延迟不够，表现为无论占空比多少都始终全亮，看起来像「LEDC 驱动不了」。把 `freq_hz` 从 4000 改成 **1000** 后，调光完全正常。

```yaml
# board_peripherals.yaml —— 唯一需要的改动
freq_hz: 1000
```

**代价**：曾据此实现了 `gpio_backlight_impl.{cpp,hpp}` 并接入 `device.cpp` / `Kconfig` / `macro_configs.h`，共 4 改 2 增。确认 LEDC 可用后已**全部还原删除**，`brookesia_hal_adaptor` 现仅剩 `lcd_panel_impl.cpp` 一处必要改动（IDF 6.0.2 回调字段版本判据）。

**教训**：外设「驱动不了」的表象，先怀疑时序/频率参数，再怀疑硬件能力。本例中若一开始扫一遍 `freq_hz`，可省掉整个层 3。

---

## 附：音频与无线能力结论（2026-08 调查，留待后续）

### 蓝牙音箱 —— 硬件不支持，无解

- P4 **无任何射频**，无线全靠 esp_hosted + **ESP32-C6** 协处理器（SDIO slot 1）
- C6 是 **BLE only**，无经典蓝牙 BR/EDR。证据三重：
  - `soc_caps.h`：C6 有 `SOC_BLE_SUPPORTED`，**无** `SOC_BT_CLASSIC_SUPPORTED`
  - `components/bt/controller/esp32c6/` 中 `br_edr`/`classic` 零命中，全是 `BT_LE_*`
  - 运行时从机能力协商：`capabilities: 0xd` → `WLAN | HCI over SDIO | **BLE only**`
- 蓝牙音箱走 **A2DP，只跑在 BR/EDR 上** → **本板永远接不了蓝牙音箱**
- **陷阱**：`BT_CLASSIC_ENABLED` 的 Kconfig 依赖含 `|| BT_CONTROLLER_DISABLED`，而 esp_hosted 正属此列。因此该选项**可以勾选、可以编译通过**，但运行时 C6 不认 BR/EDR 的 HCI 命令必然失败。切勿被「选项存在」误导

### BLE —— 可用，且从机固件已就绪

- 从机能力协商已确认 `HCI over SDIO` + `BLE only`，**C6 固件无需重刷**
- 主机侧只需开 `ESP_HOSTED_ENABLE_BT_NIMBLE`（VHCI 透传）。**选 NimBLE 而非 Bluedroid**，内存占用小得多
- 对 ESS 的价值：**直读 BMS 实时数据**（JK / JBD / Daly / Victron 等普遍支持 BLE），与 Turso 云端方案互补——BLE 提供断网可用的秒级本地数据，Turso 提供历史趋势
- 需自行实现 **central 角色**；esp_hosted 自带例程（`host_nimble_bleprph_host_only_vhci`）是 peripheral
- **待实测风险**：BLE 的 HCI 与 Wi-Fi、SD 卡共用同一条 SDIO 链路，叠加后是否互相拖累需实测

### 外置音频 —— USB 音频为首选方案

板载 ES8311 + 功放（`gpio_pa_control`, gain 6）音质不佳，外置方案按推荐度：

| 方案 | 音质 | 前提 | 状态 |
| --- | --- | --- | --- |
| **USB 音频（UAC）** | 最好，无损数字 | 板子需引出 USB Host 口 | 组件现成，**待确认硬件** |
| Wi-Fi 推流（DLNA/AirPlay） | 好 | 音箱支持网络协议 | 备选 |
| 外接 I2S DAC | 很好 | 需引出空闲 I2S 引脚 | 备选，模块廉价 |
| 蓝牙音箱 | — | **硬件不支持** | 无解 |

> **2026-08 结案**：用户确认本板**没有引出 USB Host 口**，且决定**不再追求蓝牙音箱**。
> 外置音频方案整体搁置，音频维持板载 ES8311 + 功放现状。若日后仍需改善音质，
> 剩余可行路径是 Wi-Fi 推流（DLNA/AirPlay）或外接 I2S DAC，均无需蓝牙。

USB 方案要点：
- `espressif/usb_host_uac` **v1.5.0 支持目标明确含 `esp32p4`**
- P4 硅片：`SOC_USB_OTG_PERIPH_NUM = 2`，含高速 UTMI PHY
- **未决前提**：板上 USB-C 接的是 **CH340 串口芯片**（调控制台时已确认），并非 P4 原生 USB。需确认板子是否另有 USB Host 口或引出 `USB_DP`/`USB_DM` 排针
- 相比蓝牙 A2DP 的有损压缩（SBC/AAC），USB 是数字直出，音质上限更高

---

### 板级验证顺序

**必须严格按序进行，每步通过后才进入下一步。** 跳步会导致故障现象互相掩盖。

| 步骤 | 验证内容 | 通过标准 | 失败时首查 |
| --- | --- | --- | --- |
| V1 | 编译通过 | `Project build complete`，无 lsp 报错 | yaml 语法、组件依赖版本 |
| V2 | 串口启动日志 | 出现 board manager 设备初始化成功日志 | I2C 总线、LDO |
| V3 | **点屏** | 背光亮 + 画面输出 | 背光不亮或不可调 → 查 `freq_hz`（见层 3）；黑屏但背光亮 → init 序列 / DPI 时序 |
| V4 | 触摸 | 手指位置与光标一致 | 标定 mirror_x/mirror_y/swap_xy（U5） |
| V5 | 音频 | 喇叭出声 | PA_CTRL 是否为 GPIO 20 |
| V6 | Wi-Fi | 能扫到并连上 AP | ESP-Hosted C6 固件版本 / SDIO 引脚 |
| V7 | SD 卡 | 能挂载并列目录 | slot 0 引脚填法、LDO 通道（U3） |
| V8 | 摄像头 | 能出图 | reset/pwdn 引脚（U1/U2）、SCCB 地址 |
| V9 | System Super 壳 | launcher + 状态栏正常显示 | LittleFS 分区是否烧录 |
| V10 | 800×1280 布局 | 内置三个 app 可用且不错位 | 补 `constants/800x1280.json` |

---

## 附：屏幕休眠与唤醒源（2026-08 调查，待实施）

### 需求

息屏等待时间在 Settings 可调；**只关背光**；唤醒源做成**回调注册表**以便后续扩展。

### 框架现状：无任何内置机制，需自行实现

搜索确认框架**没有**息屏 / 待机 / 空闲变暗 / 屏保功能。但所需零件齐备：

| 能力 | API | 位置 |
| --- | --- | --- |
| 关背光 | `set_brightness(0)` / `set_light_on_off(false)` | `hal_interface/.../display/backlight.hpp:57,80` |
| 服务级调用（持久化 + 发事件） | `SetBacklightOnOff` / `SetBacklightBrightness` | `service_display/src/display_backlight.cpp:112,32` |
| 检测触摸活动 | `connect_touch_updated(output, cb)` | `service_display.hpp:192` |
| 亮度语义 | 百分比 `0–100`，LEDC 映射见 | `ledc_backlight_impl.cpp:24-26,186-212` |

**关键约束**：原始触摸点**只在进程内**经 `connect_touch_updated` 可得；服务框架对外仅发布高层
`TouchGesture` 事件。因此休眠管理器**必须位于 Display 服务或 system_super 内，不能做成独立 app**，
否则轻点一下可能唤不醒（手势未必触发）。

**待决**：息屏后第一次触摸是否「吃掉」（不穿透为点击）。`connect_touch_updated` 是被动观察者、
拦不住事件，要屏蔽需在 LVGL indev 层加一道。建议吃掉，避免误触。

### Wi-Fi 省电：**已默认开启，无需任何工作**

实测（临时代码已删除）：

```
PSTEST 默认值: err=ESP_OK mode=WIFI_PS_MIN_MODEM (每个DTIM醒)
PSTEST 设为 WIFI_PS_MIN_MODEM -> set=ESP_OK readback=一致
PSTEST 设为 WIFI_PS_MAX_MODEM -> set=ESP_OK readback=一致
全程 DNS 解析正常，无掉线
```

- **默认即 `WIFI_PS_MIN_MODEM`**，DTIM 周期性打盹本来就在跑
- `esp_wifi_set_ps` 经 esp_hosted RPC 透传正常，C6 侧无问题
- **不要改用 `MAX_MODEM`**：按 `listen_interval` 跳过多个 DTIM 会增大下行延迟，与 ESS 告警推送需求冲突
- **不要断开重连做「定期连一下」**：告警会在断开窗口丢失，且重连（扫描/认证/DHCP）能耗常高于维持连接

结论：**省电空间只剩背光与摄像头**。息屏时应**直接停止摄像头预览**（画面无人看，可省 sensor + MIPI + ISP 整条链路），唤醒时重启，而非定期唤醒。

### 唤醒源设计

回调注册表，触摸为内置的第一个使用者：

```
register_wake_source(name) -> wake() 句柄
```

后续新增唤醒源（ESS 告警、人脸、mmWave）只需注册，不改架构。

### 人脸检测唤醒 —— 可行但有硬伤，暂缓

- P4 有 AI 加速，ESP-DL 有现成人脸**检测**模型（只需判断「有没有脸」，无需识别身份，模型小）
- 功耗账算得过来：10 吋背光约 1–3 W，摄像头低分辨率跑检测约几百 mW，净省
- **硬伤：暗光完全失效**。OV02C10 是普通彩色传感器，板上无红外补光。夜间关灯后人站在面前也检测不到，
  而「夜里路过扫一眼」恰是该功能最该生效的场景
- 次要顾虑：摄像头需持续开启监视房间

### 已实现（2026-08）

- **空闲息屏**：Display 服务内状态机，默认 120s，Settings 可调 7 档（15s/30s/1m/2m/5m/10m/永不），持久化
- **唤醒源注册表**：`function_register_wake_source(name)` 返回 wake 句柄，触摸为首个使用者
- **首触吞掉**：`get_touch_snapshot()` 中遮蔽，`gui_lvgl` 零改动（LVGL 主动来拉快照）
- **定时关屏**：周计划（weekday bitmask + 起止时分），支持跨午夜，`localtime_r` 本地时区
- **SNTP 守卫**：开机时间未同步前不执行计划表，避免按 1970 的垃圾时钟黑屏。实测生效

### 踩坑记录：JSON UI 模板 override 路径

**症状**：`System init failed: Template override target not found: title` → Settings app 安装失败 → **整个系统 init 失败**。

**原因**：`switch_row` 模板的 `title` 元素**嵌套在 `text` 容器内**，必须写 `"text/title"`；
而 `slider_row` 的 `title` 在顶层，裸写 `"title"` 有效。照抄另一个模板的写法必错。

**教训（与 i18n 格式属同一类错误）**：
1. **不同模板结构不同，override 路径不可互抄**。写之前先读 `package/res/templates/<模板>.json`，
   或找一处**已知可用**的同模板用法（如 `sound.json` 的 `switch_row`）照着写
2. **编译通过 ≠ 可用**。JSON UI 是**运行时**解析的资源，编译器完全看不见，必须烧录验证
3. 可用脚本穷举校验所有 `templateRef` 的 override 目标是否存在于对应模板中，成本极低

### 踩坑记录 3（根因）：新增 action 必须注册进订阅列表，否则永远到不了 on_action

**症状**：Settings 新增选项**点得动、有按压变色，但毫无效果**，设置值不变、不持久化。

**关键认知**：action **不会自动**转发给 `IApp::on_action`。系统侧 `make_app_action_forwarder`
必须经 `gui_subscribe_actions()` 显式订阅后才会转发。而 LVGL 的**按压变色是本地渲染**，
与是否有人处理该事件无关 —— 所以「有视觉反馈」完全不能证明事件被消费了。

**原因**：`settings_app.cpp` 的 `make_default_action_subscriptions()` 决定订阅哪些 action：

```cpp
append_action_subscriptions(actions, NAVIGATION_ACTIONS);
append_action_subscriptions(actions, THEME_ACTIONS);
append_action_subscriptions(actions, TIME_ZONE_ACTIONS);   // 时区可用正因在此
append_action_subscriptions(actions, WIFI_ACTIONS);
```

新增的 23 个息屏/定时 action 不在任何数组中 → 从未订阅 → 事件止步于 GUI 层。

**修复**：新增 `DISPLAY_SLEEP_ACTIONS` 数组并 append。

**为何排查耗时**：缺的不是任何一处代码，而是**一处注册**。事件结构、action 字符串
（JSON 与 C++ 26 对 26 完全一致）、分发链可达性、守卫条件、服务端函数注册与 handler 实现、
绑定路径、模板元素、容器写法、`events` 覆盖语义——逐项静态检查**全部通过**，
因为它们确实都是对的。

**教训**：在 Brookesia 里新增任何 UI action，**三件事缺一不可**：
1. JSON 中声明 `events`
2. C++ 中处理（`on_action` 分支或 `subscribe_actions` handler）
3. **注册进 `make_default_action_subscriptions()`** ← 最易遗漏，且失败时完全静默

### 踩坑记录 2：Settings 新增页面项必须补「进入页面刷新」

**症状**：Settings > 显示 里新增的选项**渲染正常但看起来选不了**，没有「当前」选中标记。

**原因**：`on_action` 的导航块中，每个页面进入时都会刷新自身状态，但新增功能时**漏了 `PAGE_DISPLAY` 分支**：

```cpp
if (current_page_ == PAGE_DEVICE)     -> refresh_my_device_state
if (current_page_ == PAGE_LANGUAGE)   -> refresh_language_state
if (current_page_ == PAGE_TIME_ZONE)  -> refresh_time_zone_state
if (current_page_ == PAGE_DEBUG)      -> refresh_debug_state
// PAGE_DISPLAY 缺失 -> refresh_display_state 只在服务绑定与各 setter 内部被调用
```

结果：打开页面时选中标记从未绘制，整列空白，表现为「有选项但选不了」。

**排查代价**：先后排除了事件结构、`is_navigation_action` 误吞、分发链可达性、守卫条件、
绑定路径、模板元素、容器写法、`disabled` 绑定共 8 个假设，最后才定位到刷新时机。

**顺带确认的框架语义**（`gui_interface/src/parser.cpp:3597-3605`）：
templateRef 的 `overrides` 中，**仅当模板原字段与覆盖值都是 object 时才合并**，
否则整体 `insert_or_assign` 替换。`events` 是数组 → **整体替换**，
所以不会与模板默认事件叠加造成双重派发。

**教训**：在 Settings 新增任何「需要回显当前值」的页面项时，
**先确认该页面在导航块里有对应的 refresh 分支**，没有就补上。

### 工具限制：串口开口会复位板子

尝试做「累计计数 + 周期心跳」的异步诊断（让用户随时操作、之后随时抓包读取）**不可行**：
每次打开 `/dev/ttyUSB0` 都会触发 CH340 自动复位电路，板子重启、计数器清零。
`stty -hupcl` 与改用 `cat` 均无效（后者还会把板子按在复位态）。

**结论**：需要人工操作配合的验证，只能「抓包窗口内实时操作」，或改用**屏幕本身可观测**的判据
（例如：把息屏设为 1 分钟后观察屏幕是否提前变黑），后者不依赖串口。

### mmWave 存在感应 —— 首选方案，**待购硬件**

| | 摄像头人脸检测 | mmWave（如 LD2410，UART，约 ¥15） |
| --- | --- | --- |
| 暗光 | **失效** | 正常 |
| 功耗 | 几百 mW | 几十 mW |
| 隐私 | 有顾虑 | 无成像 |
| 静止的人 | 能测 | 能测（优于 PIR） |
| 判断「在看屏幕」 | **能** | 仅能测在场 |

除「是否真在看屏幕」外全面占优，而对亮屏而言「有人靠近」通常已足够。

**当前无此配件，留待后续**。因唤醒源是回调注册表，届时只需注册一个新源，无架构改动。

---

## 第二部分：ESS 储能监控 App

### 2.0 架构决策：Pi 4 作为归一化代理

**决策：ESP32 不直连厂商云或 InfluxDB，一律通过 Pi 4 上的一个轻量 HTTP 服务取数。**

理由：

1. **限流**。Sigenergy Cloud 每端点 5 分钟仅允许 1 次请求，SolaX 限 10 次/分钟。若 ESP32 直连，UI 刷新率被云端死死卡住。Pi 侧可以按云端限流拉取并本地缓存，ESP32 则以任意频率读缓存。
2. **鉴权复杂度**。多数厂商云需要 HMAC-SHA256 签名、token 定期刷新、完整 TLS 证书链校验。在 ESP32 上实现这套东西代码量大且脆弱，凭据还得存进 NVS。
3. **可用性**。云端故障或断网时，Pi 可继续提供最后一次成功的数据 + 明确的 stale 标记，面板不会变砖。
4. **解耦**。厂商是 FoxESS 还是 SolaX 还是 InfluxDB，对 ESP32 完全透明。将来换设备只改 Pi，固件不动。

**⚠️ 待用户确认（阻塞 Pi 侧实现，不阻塞 ESP32 侧）**：数据实际来源是 InfluxDB 还是某家厂商云（用户提及 "turbux"，未能匹配，疑为 InfluxDB 的口误）。

#### 归一化 JSON Schema（ESP32 侧唯一契约）

Pi 暴露 `GET http://<pi>:<port>/api/ess/summary`，返回：

```json
{
  "ts": 1754100000,
  "stale": false,
  "battery": { "soc": 62.5, "soh": 98.0, "power_w": 1850, "temp_c": 24.5 },
  "solar":   { "power_w": 4200, "today_kwh": 18.4 },
  "grid":    { "power_w": -1200, "import_today_kwh": 2.1, "export_today_kwh": 9.8 },
  "load":    { "power_w": 1150 },
  "mode":    "self_consumption",
  "status":  "normal"
}
```

功率符号约定（必须在 Pi 侧统一，ESP32 不做猜测）：
- `battery.power_w`：**正 = 充电，负 = 放电**
- `grid.power_w`：**正 = 从电网买电，负 = 上网卖电**

历史曲线走单独端点 `GET /api/ess/history?range=day`，返回等间隔采样数组，避免 summary 端点体积膨胀。

### 2.1 组件文件清单

**产出目录**：`app/brookesia_app_ess/`

骨架完全照搬 `app/brookesia_app_settings/`。核心机制（已验证）：

```cpp
// src/app/provider.ipp
BROOKESIA_SYSTEM_CORE_APP_PROVIDER_REGISTER_WITH_SYMBOL(
    EssAppProvider, APP_ID, app_ess_provider_symbol);
```

```cmake
# cmake/esp_platform.cmake
target_link_libraries(${COMPONENT_LIB} PUBLIC "-u app_ess_provider_symbol")
brookesia_stage_runtime_app_package(
    PACKAGE_ID "brookesia.general.ess"
    SOURCE_DIR "${COMPONENT_DIR}/package"
    STAGE_ROOT ${stage_root})
```

运行时 `system/brookesia_system_core/src/app/manager.cpp` 通过 `AppProviderRegistry::get_all_instances()` 自动枚举并安装，**`examples/system/super/main/main.cpp` 一个字都不用改**。

完整文件清单：

```
app/brookesia_app_ess/
├── CMakeLists.txt                     # 照抄 settings，仅改组件名
├── Kconfig                            # BROOKESIA_APP_ESS_ENABLE_PRELOAD_DOM 等
├── idf_component.yml                  # 依赖 system_core / gui_interface / service_http
├── cmake/
│   ├── esp_platform.cmake             # 强制链接 provider 符号 + 资源 staging
│   └── pc_platform.cmake              # PC 仿真路径
├── include/brookesia/
│   ├── app_ess.hpp
│   └── app_ess/
│       ├── ess_app.hpp                # class EssApp final : public system::core::IApp
│       └── macro_configs.h            # CONFIG_* → BROOKESIA_APP_ESS_* 映射
├── src/
│   ├── ess_app.cpp                    # action 字符串常量定义
│   ├── app/
│   │   ├── lifecycle.ipp              # get_manifest / get_gui_descriptor / on_install / on_start
│   │   └── provider.ipp               # provider 注册宏
│   ├── data/
│   │   ├── model.ipp                  # EssSnapshot 结构体 + JSON 解析
│   │   └── poller.ipp                 # HTTP 轮询 + 定时器 + 失败退避
│   ├── screen/
│   │   ├── overview.ipp               # 能量流主页刷新逻辑
│   │   ├── history.ipp                # 曲线页
│   │   └── detail.ipp                 # 详细数据页
│   └── i18n/locale.ipp
└── package/res/
    ├── root.json                      # assets 清单 + 分辨率 variants
    ├── constants/{default,800x1280,portrait}.json
    ├── images/index.json + icon/*.png
    ├── i18n/{en,zh_CN}.json
    ├── templates/{metric_card,flow_node,stat_row}.json
    ├── screens/{overview,history,detail,header}.json
    └── flows/content.json
```

### 2.2 UI 结构（针对 800×1280 竖屏设计）

竖屏是本项目的优势 —— 能量流图天然适合纵向排布。

**页面一：能量流总览（overview）** —— 主页

```
┌──────────────────────────┐
│  ☀ 太阳能    4.2 kW      │  ← 顶部：发电
│         │                │
│         ▼                │  ← 流向箭头（用 canvas 或旋转 image 做动画）
│  ┌──────────────┐        │
│  │  🔋 62.5%    │        │  ← 中部：电池，大号 SoC + 环形进度
│  │  充电 1.85kW │        │
│  └──────────────┘        │
│      │        │          │
│      ▼        ▼          │
│  🏠 负载   ⚡ 电网       │  ← 底部：消耗与并网
│  1.15 kW   卖电 1.2kW    │
├──────────────────────────┤
│  今日发电 18.4 kWh       │
│  今日买电  2.1 kWh       │  ← 统计卡片区（可滚动）
│  今日卖电  9.8 kWh       │
└──────────────────────────┘
```

**页面二：历史曲线（history）** —— 日/周/月切换
**页面三：详细数据（detail）** —— SoH、温度、循环次数、运行模式、原始字段

页面间跳转用 `flows/content.json` 的 screenFlow，与 Settings 应用同构。

### 2.3 数据层与 UI 的对接方式

**拉取**：`on_start` 中启动周期定时器（`on_timer` 回调），调用 `service::helper::Http` 发起请求。轮询间隔做成 Kconfig 可配，默认 5 秒。请求失败采用指数退避，连续失败时 UI 显示 stale 标记而非清零 —— **绝不能因为一次网络抖动就把数值显示成 0，那会造成严重误读**。

**推送到 UI**：统一走批量绑定更新，避免逐个字段触发重排：

```cpp
std::vector<gui::BindingValueUpdate> updates;
updates.push_back({ .absolute_path = "/page/battery_card/soc_label",
                    .key = "labelProps.text", .value = fmt_percent(snap.battery.soc) });
// ... 其余字段
context.gui().set_binding_values(updates);
```

**用户交互**：JSON 里挂 `"events": [{"type":"clicked","action":"ess.open.history"}]`，C++ 侧在 `subscribe_actions()` 中注册同名字符串的处理器。

### 2.4 分步实施顺序与验收 QA

PC 仿真环境准备（一次性）：

```bash
# 安装依赖，--full 含 SDL2 显示后端
hal/brookesia_hal_linux/scripts/install_linux_deps.sh --full
# 构建 PC 仿真版 super 示例（Display 后端选 sdl2，窗口尺寸设为 800x1280 模拟本板）
cmake -S examples/system/super -B build-pc -DBROOKESIA_HAL_LINUX_DISPLAY_BACKEND=sdl2
cmake --build build-pc -j
```

桩服务（E4 起使用），在 Pi 或本机跑：

```bash
mkdir -p /tmp/ess-stub && cd /tmp/ess-stub
# summary.json 内容见 2.0 节的归一化 schema
python3 -m http.server 8088
# ESP32/PC 侧访问 http://<host>:8088/summary.json
```

| 步骤 | 内容 | 具体 QA 场景 | 通过标准 |
| --- | --- | --- | --- |
| **E1** | 组件骨架 + provider 注册，UI 只有一个 "Hello ESS" 标签 | 1. 在 `examples/system/super/main/idf_component.yml` 加入 `brookesia_app_ess` 依赖<br>2. 运行 `./build-pc/super`<br>3. 在 SDL 窗口的 launcher 中查找图标<br>4. 鼠标点击图标<br>5. 从屏幕底部向上拖拽 | 图标出现且名称为「储能」；点击后进入并显示 "Hello ESS"；上滑手势可返回 launcher |
| **E2** | `EssSnapshot` 结构体 + JSON 解析 | 用三份输入喂解析函数：<br>① 2.0 节的完整合法 JSON<br>② 缺少 `battery.soh` 字段的 JSON<br>③ `"soc": "abc"` 类型错误的 JSON | ① 全字段正确赋值；② 缺失字段回落到 `std::optional` 空值且不崩溃；③ 返回解析错误而非未定义值。三条均以断言形式固化，`ctest` 通过 |
| **E3** | overview 页面静态布局（假数据写死 SoC=62.5 等 2.0 节示例值） | 1. `./build-pc/super` 打开 ESS<br>2. 截图窗口<br>3. 逐项比对 | 电池 SoC 显示 `62.5%`；光伏 `4.2 kW`；负载 `1.15 kW`；电网显示为**卖电 1.2 kW**（因 `grid.power_w` 为负）；电池显示为**充电**；四项数值无截断、无重叠、无溢出屏幕边界；800×1280 窗口内不出现横向滚动条 |
| **E4** | 接 HTTP 轮询，指向桩服务 | 1. 启动桩服务，`summary.json` 用 2.0 节示例（`soc: 62.5`、`battery.power_w: 1850`）<br>2. 打开 ESS，观察 5 秒<br>3. 用编辑器把 `soc` 改成 `20.0`、`battery.power_w` 改成 `-3000`，保存<br>4. 等待 ≤ 5 秒<br>5. 停掉桩服务（Ctrl-C），等待 30 秒<br>6. 重新启动桩服务 | 步骤 2 显示 `62.5%` 且电池状态为**充电 1.85 kW**（正值=充电）；步骤 4 内 UI 自动变为 `20.0%` 且电池状态变为**放电 3.0 kW**（负值=放电）；步骤 5 数值**保持最后一次的值不变**并出现 stale 标记，**绝不显示为 0**；步骤 6 后 stale 标记消失、数值恢复更新 |
| **E5** | Pi 侧接入真实数据源 | 1. 在 Pi 上 `curl -s http://localhost:<port>/api/ess/summary \| jq .`<br>2. 与厂商 App / InfluxDB 面板上的同一时刻读数对照<br>3. 观察连续 10 分钟 | curl 返回的 JSON 通过 `jq` 校验合法；SoC 与厂商 App 差值 ≤ 1%；功率符号方向与 2.0 节约定一致（可通过「白天光伏充电时 `battery.power_w` 为正」验证）；10 分钟内 `ts` 字段持续更新，无长时间冻结 |
| **E6** | history / detail 页面 + 页面跳转 | 1. 主页点击「历史」入口 → 进入 history<br>2. 切换 日/周/月 三个 tab<br>3. 点击返回 → 回到主页<br>4. 主页点击「详情」→ 进入 detail<br>5. 返回<br>6. 重复上述循环 10 次 | 每次跳转均正确切换页面，无白屏、无残留上一页元素；三个 tab 均能画出曲线且横轴刻度随 range 变化；10 次循环后内存无持续增长（PC 侧用 `valgrind --tool=massif` 或观察 RSS） |
| **E7** | 动画、i18n、异常态 UI | 1. 观察能量流箭头 30 秒<br>2. Settings → 语言切到 English，返回 ESS<br>3. 切回中文<br>4. 让桩服务返回 `"stale": true`<br>5. 让桩服务返回 HTTP 500 | 箭头动画流畅无卡顿，方向与功率符号一致（放电时箭头由电池指向负载）；切英文后所有标签变为英文且**无文字截断**；切回中文同样正常；stale 状态下出现视觉提示（如灰化或角标）；HTTP 500 时不崩溃，显示错误态 |
| **E8** | 合入真机 | 1. 在真机执行 `idf.py build flash monitor`<br>2. 重复 E3–E7 的全部 QA 场景<br>3. 额外：拔掉 Wi-Fi AP 电源 60 秒后恢复 | 真机行为与 PC 仿真一致；800×1280 实屏上布局与截图一致；断网 60 秒期间显示 stale 且不崩溃，AP 恢复后自动重连并恢复更新 |

**E1–E4 全部可在 PC 仿真完成，不依赖板级移植。**

---

## 第三部分：Tile 式产品外壳（2026-08 立项，不急着做）

### 产品形态

墙面板的**默认画面**是一块 Tile（磁贴）网格，而非传统 app 图标栅格：

- 背景图从 **SD 卡某个文件夹**轮播
- 每块 Tile 直接显示活数据：**ESS 储能、天气、新闻、股票**
- **点开 Tile** 进入该主题的详细页面

用户洞察：「本质上就是一个一个 app」——因此这不是「做一个 Dashboard app」，
而是**换掉 launcher**。这是与早前结论（写普通 app）不同的方向，且更贴合产品形态。

### 架构决策：自建 System，**不 fork** `system_super`

调查结论（证据见下）：`brookesia_system_core` 本就是为派生而设计，
`system_super` 只是它的一个具体实现，二者是并列关系而非「必须继承 super」。

| 证据 | 位置 |
| --- | --- |
| `core::System` 提供大量 protected 虚钩子供覆写 | `system_core/include/.../system/system.hpp:375-396`（`on_init`/`on_start`/`on_app_installed`/`on_app_started` 等） |
| `system_super` 本身就是 `core::System` 的子类 | `system_super/include/.../system.hpp:40`（`class System: public core::System`） |
| super 在 `on_init` 里创建并安装自己的 ShellApp | `system_super/src/system_lifecycle.cpp:263-271` |
| Shell 本身只是一个普通 native app | `system_super/src/private/shell_app.hpp:30`（`class ShellApp final: public core::IApp`） |

**做法**：新建组件 `MySystem : public core::System`，在 `on_init` 里安装**我们自己的 ShellApp**
（同样实现 `core::IApp`），完全不碰 `system_super`。

**为何不 fork super**：本项目已维护 5 个第三方组件补丁，
fork super 会把 launcher、状态栏、手势、通知全部绑死到我们的维护责任里。
自建 System 则把产品代码与上游彻底隔离。

**保留退路**：`system_super` 仍可作为参考实现随时对照，且它证明了这条路可行。

### 关键可行性：Tile 的动态内容 —— **机制已存在**

这是决定方案成立与否的前提，已验证通过。

`launcher_app_button` 模板的绑定结构（`system_super/resource/shell/templates/launcher_app_button.json`）：

```
button                 bindings: placement.x <- x, placement.y <- y
  icon_box
    icon_image         bindings: imageProps.src  <- src,  commonProps.hidden <- hidden
    icon   (label)     bindings: labelProps.text <- text, commonProps.hidden <- hidden
  title_box
    app_name (label)   bindings: labelProps.text <- name
```

两点关键：

1. **图标区内部已有一个 label**（`icon`，现用作无图标时的文字兜底）
   → 「在磁贴里渲染文字（如 23°C）」这件事模板层面已经支持
2. ShellApp 已在**运行时**通过 `add_binding_update` / `set_binding_values` 更新 `src` 与 `text`
   （`system_super/src/shell_app.cpp:815-832`）→ 图标本就是动态的，不是静态资源

**结论**：做 Tile 缺的**不是渲染能力**，而是「谁来周期性推送数据」这一层。
自定义 ShellApp 里加一个定时器 + 数据源订阅即可，无需改动 GUI 框架。

**要做的扩展**：在自己的 tile 模板里增加更多元素（主数值、副标题、单位、趋势箭头等），
每个都挂 binding，由 ShellApp 定时刷新。

### 数据来源（待定，倾向 B）

用户提出三条路：

| 方案 | 说明 | 评价 |
| --- | --- | --- |
| A. 直连 Turso | 面板直接查数据库 | 需在设备侧处理 TLS + 凭据 + SQL，且业务逻辑分散两处 |
| **B. 调 ESS Dashboard 现有 API** | 复用已有聚合逻辑 | **倾向此方案**：不重复实现聚合与归一化，面板只做展示 |
| C. Pi 4 代理（原 2.0 节决策） | 归一化代理 | 与 B 不冲突，可作为统一入口 |

**关于「需要实时数据吗」**：不需要真正的实时。墙面板是「扫一眼」的使用场景，
ESS 数据 **10–30 秒刷新足够**；秒级以下毫无价值，反而增加功耗与请求量。
真正需要低延迟的只有**告警**，而告警更适合走推送/唤醒源，而非高频轮询。

（唤醒源注册表已就绪，见「屏幕休眠与唤醒源」一节，告警可直接注册一个 wake source 点亮屏幕。）

### 背景图轮播

- 来源：SD 卡某文件夹（SD 卡已挂载可用，15.6 GB）
- **注意**：图片需**预先缩放到 800×1280**，不要让 P4 实时解大图
- 与息屏功能协同：息屏时应停止轮播定时器

### 分阶段（不急，排在 ESS 数据打通之后）

| 阶段 | 内容 |
| --- | --- |
| T1 | 自建 `MySystem` + 最小 ShellApp，能起来并显示一屏静态 Tile |
| T2 | Tile 模板扩展（数值/副标题/单位），ShellApp 定时刷新绑定 |
| T3 | 接入第一个真实数据源（ESS），验证刷新与功耗 |
| T4 | 背景图轮播（SD 卡） |
| T5 | 点开 Tile → 详细页面 |
| T6 | 天气、新闻、股票依次接入 |

### 顺序决策（2026-08 修正）：**先做 launcher，后接 ESS 数据**

初版计划写的是「ESS 数据先行」，该结论**在架构改为自建 System 后不再成立**，理由：

1. **先退最大未知**。自建 System 是本项目**从未走过**的路，仓库内亦无先例或文档
   （`docs/` 中无自建 System 指引，仅 README 指向线上文档）。若 `MySystem` 或自定义
   ShellApp 走不通，产品形态需重新设计——此时 ESS 数据接得再好也是白做。
2. **避免 UI 返工**。第二部分 2.2 节的 ESS UI 是按**独立 app 形态**设计的；
   若最终形态是 Tile launcher，该设计至少要改一半。先定容器可避免这部分浪费。
3. **T1/T2 对 ESS 零依赖**，可连续推进；等 T3 接真实数据时，容器已验证。

**T1 必须刻意做小**——它是**架构验证（spike）而非功能开发**：
- `MySystem : public core::System` 能启动
- 装一个最小 ShellApp，显示**一屏写死的静态 Tile**
- **不接**任何数据源、**不做**背景轮播、**不做**点击详情

单一目标：证明这条路通。通了再加，不通趁早改道。

**新前置依赖**：T3 依赖 ESS 数据通路；T1/T2 无前置依赖，可立即开始。

---

## 第四部分：T1 架构调研结果（2026-08，可开工）

三个并行 explore agent 的调研结论。关键结论均已由我用直接工具复核过行号
（其中两个 agent 后期跑偏或陷入自我总结循环，未产出终稿，缺口由我自行补齐）。

### 结论一：自建 System 的门槛比预想低得多

**`core::System` 没有任何纯虚函数**，所有 `on_*` 钩子都有默认实现
（声明 `system_core/include/.../system/system.hpp:376-409`，默认实现 `src/system/core.cpp:35-144`）。
因此「最小可编译子类」几乎为空。

真正的风险不在编译，而在 **`init()` 的中止点**——任何一处失败都会让启动直接返回错误。
以下为必须满足的检查清单（`system_core/src/system/lifecycle.cpp`）：

| 中止点 | 行号 | 应对 |
| --- | --- | --- |
| 任务调度器启动失败 | 43-49 | 保证环境正常 |
| `configure_task_groups()` 失败 | 53-55 | 同上 |
| 添加 SystemCore/Gui/Timer 服务失败 | 139-147 | 同上 |
| ServiceManager 启动失败 | 148-150 | 或设 `start_service_manager=false` |
| 服务绑定失败 | 154-157 | 同上 |
| **存储布局初始化失败** | 159-162 | Storage 服务须可用 |
| `on_prepare_startup_overlay()` 返回错误 | 165-168 | 覆写时务必返回成功 |
| `show_startup_overlay()` 失败 | 170-173 | 或不启用 overlay |
| **`on_init()` 返回错误** | 180-183 | 覆写时务必返回成功 |
| `install_registered_apps()` 失败 | 184-189 | **T1 建议设 false** |
| `install_unpacked_apps()` 失败 | 190-195 | **T1 建议设 false** |
| `on_start()` 返回错误 | 210-214 | 覆写时务必返回成功 |

**两个默认即「不支持」的钩子**（`src/system/core.cpp:110-117, 122-138`）：
`on_show_app_keyboard()` 与 `on_show_message_dialog()` 默认返回
`std::unexpected("... is not supported by this system")`。需要键盘或对话框就必须覆写，
否则 app 调用时运行时报错。**T1 不需要，但 Tile 详情页若有输入框就要补。**

### 结论二：ShellApp 只需实现一个纯虚函数

`core::IApp`（`system_core/include/.../app/iapp.hpp`）中：

- **`get_manifest()` 是唯一的纯虚函数**（:36）
- `get_gui_descriptor()` 有默认实现（:43），默认 `root_kind = None` → **不显示任何 GUI**
- `on_install/on_start/on_pause/on_resume/on_stop/on_action/on_timer` 全部有默认实现

`AppGuiDescriptor`（`app/types.hpp:173-178`）：`root_kind` / `root` / `resources` / `screen_flows`。

### 结论三（重要简化）：T1 可以不碰 LittleFS 资源打包

`GuiRootKind` 有三个值：`None` / `File` / **`JsonString`**（`app/types.hpp:144-149`），
且 `JsonString` 分支**确已实现**（`system_core/src/system/gui.cpp:513, 554`）。

**意味着 T1 的根文档可以直接内嵌为 C++ 字符串**，无需先打通
`brookesia_system_super_stage_resources()`（`system_super/cmake/resource_stage.cmake:5`）
那套资源打包与 LittleFS 部署流程。

这把 T1 的范围又缩小了一大截：**验证架构时不必同时验证资源管线**。
等 T1 通过、要接真实图片和字体时，再切到 `GuiRootKind::File` + 资源打包。

### 结论四：自建 System 会失去什么（关键决策依据）

| 能力 | 归属 | 自建后是否丢失 | 重建代价 |
| --- | --- | --- | --- |
| **底边上滑返回手势** | **识别在框架**（`service_display.cpp` `process_touch_gesture` ~1702，`emit_touch_gesture` 2569-2575，发布 `TouchGesture` 事件）；super 仅订阅（`shell_overlay.cpp:576-583`） | **不丢失** | **很低**：订阅事件 + 调 `stop_app()` |
| **app GUI 挂载 / 前后台切换** | **核心**（`system_core/src/app/manager.cpp:719-731`，core 在 `on_start` 之前就加载并挂载 app GUI） | **不丢失** | 无 |
| 状态栏（时间/Wi-Fi） | super（`shell_overlay.cpp:676-702` Wi-Fi 绑定，704-720 SNTP 时钟） | 丢失 | 中，需自己订阅服务并更新绑定 |
| 启动遮罩 | 可选（core 默认 `on_prepare_startup_overlay` 返回 `{}`；super 的覆写同样是空实现 `system_lifecycle.cpp:243`） | 丢失但**无所谓** | 极低，或直接不做 |
| app 启动动画 | super（`shell_overlay.cpp:1485-1516`） | 丢失 | 低，纯观感 |
| 通知页 | super（`system_navigation.cpp:26-59`） | 丢失 | 中，但我们不需要 |
| 键盘 / 消息对话框 | 需自行覆写（base 默认「不支持」） | 需要才做 | 中 |

**最重要的两条都是好消息**：

1. **返回手势的识别在 Display 服务里，不在 super**。我们只需订阅 `TouchGesture` 事件并决定如何反应。
   之前担心「Tile 详情页进去出不来」的问题不成立。
2. **app 的 GUI 挂载与前后台切换由 core 负责**，不是 super。
   所以「点开 Tile → 打开详情 app → 滑回」这条链路在自建 System 下天然可用。

结论：**自建 System 的代价主要是状态栏和一些观感细节，核心交互能力全部保留。**

### T1 实施蓝图（修订后，范围更小）

```
MySystem : public core::System
  Config: gui_backend 必填；install_registered_apps=false；install_package_apps=false
  on_init():  install_app(make_shared<TileShellApp>())  -> 必须返回成功
  on_start(): 启动 shell app                              -> 必须返回成功

TileShellApp : public core::IApp
  get_manifest()        -> 唯一必须实现的纯虚函数
  get_gui_descriptor()  -> root_kind = JsonString, root = 内嵌的一屏静态 Tile JSON
  on_start()            -> 挂载首屏
```

**T1 验收标准**：烧录后屏幕显示一屏静态 Tile，无 init 失败、无 app 安装失败。
**不做**：数据源、背景轮播、点击详情、状态栏、手势。

### 待确认（T1 过程中解决）

- `install_registered_apps=false` 后，相机 app / Settings 等既有 app 是否还需要能被安装（Tile 要点开它们的话需要）
- 首屏挂载具体调用哪个 API（`mount_screen` vs `start_screen_flow`），需照 `ShellApp::on_start` 的写法
- 自建 System 与现有 `examples/system/super` 的关系：新建 example 还是切换现有 example 的 System 类型

---

## 第五部分：T1 实施结果（2026-08，**已通过**）

### 结论：自建 System 架构成立

实机日志（未改动 `system_core` / `system_super` 一行）：

```
TileSystem init starting
TileSystem on_init
SysCore: Native app installed: id(1), manifest(tile_shell)
SysCore: System core initialized: type(tile)
TileSystem on_start
TileShellApp starting... / started successfully
SysCore: App started: id(1), manifest(tile_shell), total_ms(74)
=== System Example Completed ===
```

无 GUI 解析错误、无 init 失败。**最大的架构风险已退掉。**

### 交付物

新组件 `system/brookesia_system_tile/`：

- `TileSystem : core::System` —— `system_type="tile"`，拒绝缺失的 `gui_backend`，
  **关闭 `install_registered_apps` 与 `install_package_apps`**（避免无关 app 中止启动），
  不启用 startup overlay；`on_init` 安装 shell app，`on_start` 启动它
- `TileShellApp : core::IApp` —— 实现唯一的纯虚函数 `get_manifest()`；
  `get_gui_descriptor()` 返回 `root_kind = JsonString` + 内嵌根文档

改动仅 3 处：新组件、example 的 `idf_component.yml`、example 的 `main.cpp`（切换 System 类型）。

### 关键实现要点

**内联资产可行**（此前不确定）：`assets` 数组的条目**既可是文件路径字符串，也可是资产对象**
（`gui_interface/src/parser.cpp:825-870`，错误信息原文 "entries must be either file paths or asset objects"）。
因此 `JsonString` 根文档可以内联声明 screenFlow 与 viewScreen，**T1 全程未接触 LittleFS 资源打包**。

**资产 schema**（照抄现有可用文件）：
- `viewScreen`: `{type, id, mountMode, commonProps, styleRefs, children}`
  （范本 `app_settings/package/res/screens/time_zone.json`）
- `screenFlow`: `{type, id, screens, initial, transitions}`
  （范本 `system_super/resource/shell/flows/shell_pages.json`）

### 踩坑记录 4：JSON 由代码生成，不要手写

首版内嵌 JSON 有 125 行、带完整样式，**根对象与 `assets` 数组均未闭合**，
运行时报 `Failed to parse GUI JSON: incomplete JSON`。而交付方声称「已复核 JSON 合法」——该声明不实。

**教训**：
1. 内嵌的 JSON 字面量**必须用脚本生成并 `json.loads()` 自检后再写入源码**，不要手写、更不要靠肉眼复核
2. T1 这类验证任务里，JSON 应当**尽可能小**。首版为 6 个磁贴写了完整配色与字号，
   徒增出错面而对验证目标毫无贡献。替换后的版本只用
   `layout` / `placement` / `labelProps` / `gridColumn` / `gridRow` 这些**已核实存在**的属性，
   不碰颜色字号，一次通过
3. 与前三个坑同源：**JSON UI 是运行时解析的，编译通过不代表可用**

### 下一步（T2）

在此基础上扩展 tile 模板（数值/副标题/单位），并由 ShellApp 定时刷新绑定值（先用假数据）。
T2 同样不依赖 ESS 数据通路。

---

## 第七部分：T2 实施结果（2026-08，**已通过**）

### 结论：模板化 + 周期刷新机制成立

实机日志：

```
LibUtils: Timer started successfully with ID: 17
LibUtils: TileShellApp started successfully
LibUtils: First successful refresh of tiles     <- 定时器启动后 1 秒准时触发
Main:     === System Example Completed ===
```

三环全通，无 GUI 错误：**模板实例化**、**周期定时器**、**绑定刷新**。

### 关键转变：磁贴不再写在 JSON 里

T1 把 6 个磁贴硬编码在 JSON 中，加一个磁贴就要改 JSON 并重新验证解析。
T2 改为 **JSON 只提供 `viewTemplate`，C++ 按列表在运行时实例化**：

```cpp
// system/brookesia_system_tile/src/shell_app.hpp —— 加磁贴只改这里
std::vector<InfoTile> info_tiles_ = {
    {"info_energy",  "Energy",  "#364354", "0 kW"},
    {"info_weather", "Weather", "#1290d8", "20 °C"},
    ...
};
std::vector<AppTile> app_tiles_ = { {"app_camera", "Camera", "#4c494b"}, ... };
```

用到的 API（与 super 的 `populate_launcher` 同款）：
- `context.gui().create_view(template_id, parent_path, instance_id)`（`app/gui_runtime.hpp:47-51`）
- `context.gui().set_binding_values(vector<BindingValueUpdate{absolute_path, key, value}>)`
  —— `key` 是模板里声明的**绑定名**，不是属性路径
- `context.timer().start_periodic(name, interval_ms)` + 覆写 `IApp::on_timer`（`app/timer_runtime.hpp:23-25`）

**应用入口区放在可滚动容器内**（`commonProps.scrollable`），以应对 app 数量增长。

### 踩坑记录 5：字号必须用 sp，不能用 dp

首版三处字号写成 `"32dp"` / `"64dp"` / `"24dp"`，运行时报：

```
Failed to parse viewTemplate asset 'inline asset #1': Field 'font_size' must use sp units
```

**框架的报错信息非常精确**——直接点名字段和单位要求，定位成本几乎为零。
这与前几个坑形成对比：值得记住 JSON UI 的解析错误通常是可读的，遇到问题**先认真读报错**。

修正为 `sp` 并按需求缩小：`36sp` / `20sp` / `16sp`。

**注意 dp 与 sp 的分工**：尺寸、间距、圆角用 `dp`；**字号用 `sp`**（随 `font_scale` 缩放）。

### 本轮 JSON 生成方式（吸取 T1 教训）

内嵌 JSON 由脚本生成，写入前 `json.loads()` 自检，写入后**再从 .cpp 中提取回来复验**一次。
T1 的「不完整 JSON」问题未再出现。此法应作为后续所有内嵌 JSON 的标准做法。

### 下一步（T3 及以后）

- T3：接入第一个真实数据源（ESS），验证刷新与功耗 —— **依赖 ESS 数据通路**
- T4：背景图轮播（SD 卡，图片需预缩放到 800x1280）
- T5：点开 Tile 进详情 —— **届时须重新打开 `install_registered_apps`**，
  并处理「单个 app 安装失败不应中止整个启动」
- 方向设置项（见第六部分）

---

## 第六部分：屏幕方向与 RTC（2026-08 调查，**排在 T2 之后**）

### 方向：做成设置项，重启生效

**决定**：屏幕方向做成 Settings 里的选项，**不做自动感应**（板上无传感器），**不做热切换**。

#### 硬约束一：无方向传感器

板上设备清单确认：ES8311 音频、GSL3680 触摸、JD9365 屏、摄像头、SD 卡、LEDC 背光。
**没有 IMU / 加速度计 / 陀螺仪**。方向只能由配置决定。
（墙面板固定安装，本也不需要自动旋转。）

#### 硬约束二：旋转不可运行时修改

`rotation` 是注册显示时一次性传入的配置字段（`esp_lv_adapter_display.h:121`），
**没有 runtime setter**。因此设置项只能「保存 + 重启生效」。

#### 硬约束三：横屏多花 2MB 显存

```c
/* Rotation 90 or 270 always requires 3 buffers for rotation processing. */
if (rotation == ROTATE_90 || rotation == ROTATE_270) return 3;
```
（`esp_lvgl_adapter/src/display/display_manager.c:1667-1673`）

实测当前为 `frame buffers=2, tear mode=DOUBLE_DIRECT`。
横屏被强制到 3 缓冲，单帧 800x1280x2 ≈ 2MB，**净增 2MB**。

**付得起**：PSRAM 256 Mbit = 32MB，堆中可用约 22.4MB（`Adding pool of 22912K of PSRAM`）。
但这也解释了为何不能热切换——改方向需重新分配显存并重建显示链路。

#### 好消息：旋转是 PPA 硬件加速

`lvgl_bridge_v9.c:2828-2851` 使用 PPA 的 SRM（Scale-Rotate-Mirror）：

```c
ppa_srm_rotation_angle_t ppa_rotation;
case ESP_LV_ADAPTER_ROTATE_90: ppa_rotation = PPA_SRM_ROTATION_ANGLE_270; ...
```

与相机 app 使用同一硬件单元。**不是软件旋转**，无「软件转 2MB framebuffer」的性能顾虑。

注：面板级 `swap_xy` 不可用（JD9365 不支持，每次启动都报错），但无关紧要——旋转由适配器完成。

#### 实施要点（四处）

| 位置 | 内容 |
| --- | --- |
| Storage | 持久化方向值 |
| 启动时 | 读出后传给适配器 config 的 `rotation` |
| `environment` | 横屏须把宽高对调为 1280x800，否则布局仍按竖屏计算 |
| 触摸映射 | 现为 `mirror_x: true, mirror_y: false`，旋转后**必须重新标定** |

**触摸标定只能实机手工验证**，四个角度各有一组正确参数，需逐个试。

#### 为何排在 T2 之后

该改动**同时触及显示链路与触摸标定**。T1 刚验证通过、T2 尚未开始，此时插入会导致
问题归因困难（分不清是方向改动还是磁贴逻辑）。且 T2 的布局本就要重做（模板化 + 滚动），
横屏布局一并考虑更省事。

### RTC：芯片在板上，尚未装配

I2C 总线上有 **RX8025T**（见 `board_peripherals.yaml:3` 注释），但
`board_devices.yaml` 中**未配置为设备**。用户确认芯片确实存在，但**尚未装配完成**。

**价值**：当前开机后系统时间为 1970，须等 SNTP 同步（实测约 18.6 秒）才能启用定时关屏，
为此专门写了时间守卫。**接入 RTC 后开机瞬间时间即准确**，且断网不受影响。

**状态**：待用户装配完成后再配置——在此之前无法实测，不实现。

---

## 第八部分：T3 ESS 数据接入计划（2026-08，**已实测数据源**）

### 数据源事实（已连库实测，非假设）

系统背景：**Sungrow ESS + Amber Electric（悉尼）**，调度脚本每 5 分钟由 cron 驱动。

**连接**：Turso（libSQL），HTTP API `POST https://<host>/v2/pipeline`，
`Authorization: Bearer <token>`。已用 curl 实测连通。

> **凭据绝不入库**：仓库中任何文件、注释、提交信息都不得出现 token。

**凭据存放方式（2026-08 决定）：SD 卡上的配置文件，不用 NVS，也不做 Settings 输入项。**

理由：token 是 300+ 字符的 JWT **且会变更**。在触摸屏上手输不现实，
做成 Settings 文本框既难用又易错。配置文件可在 PC 上编辑后插卡即用。

- 位置：SD 卡根目录的配置文件（如 `/sdcard/ess_config.json`），含 `url` 与 `token`
- 缺失或格式错误时**必须优雅降级**：界面正常显示并提示「未配置数据源」，
  **不得因此导致启动失败**（参见 T1 中 `install_registered_apps` 的教训）
- token 变更时用户只需改文件，无需重新烧录

#### 实测表清单（比文档记载的多）

| 表 | 用途 |
| --- | --- |
| `energy_log` | 5 分钟粒度能源实况 —— **主数据源** |
| `daily_summary` | 每日汇总（kWh、成本、收益、需量峰值、SOC 极值） |
| `solar_forecast` | 今明两日发电预估、峰值辐照、云量 |
| `daily_plan` | 当日充放电计划、充电窗口、阈值 |
| `daily_plan_report` | 计划执行报告 |
| **`display_feed`** | **通用显示内容表**：category / lang / title / body / extra / 有效期 |
| `meter_daily` | 电表日数据 |

#### `energy_log` 字段（权威，来自 `init-db.js`）

`ts`(UTC) / `nem_time` / `soc` / `batt_power` / `home_load` / `pv_power` / `grid_power` /
`buy_price` / `feedin_price` / `spot_price` / `demand_window` / `mode` / `mode_changed` /
`mode_reason` / `renewables` / `alert`

**实测样本**：
```
ts=2026-08-12T23:10:10.456Z  soc=32.0  batt_power=5.0   home_load=1.18
pv_power=2.71  grid_power=-3.29  buy_price=16.19  feedin_price=8.95
```

- 时间戳为 23:00 / 23:05 / 23:10 → **确认 5 分钟间隔**
  （`turso-connection.md` 中「每分钟」的说法**已过时**）
- 每个区间边界后约 **10 秒**写入
- 单位：功率 kW，电价 分/kWh
- **`grid_power` 为负 = 从电网买入**（能量平衡验证：2.71 − 1.18 − 5.0 ≈ −3.47）

#### `display_feed` 已在供新闻

实测已有 ABC 新闻条目（category='news'，带 `valid_until`）。
**用户想要的新闻磁贴数据源现成**，无需另接新闻 API。
该表的 category 机制意味着它可承载任意显示内容，不限于新闻。

### 关键结论：本系统内不存在「实时」

数据 5 分钟一个粒度，**面板刷新再快也拿不到更新的数**。因此：

- 轮询快于 5 分钟纯属浪费
- 界面上的「当前功率」实为**最多 5 分钟前的采样**，不是实时值
- **唯一真正需要低延迟的是告警**（`energy_log.alert` 字段已存在），
  而轮询给不了推送；若要即时告警需另加 MQTT/WebSocket 长连接，属独立决策

#### 轮询策略

- **对齐 NEM 边界**：数据在 :00/:05/:10... 后约 10 秒落库，故在边界后 ~20 秒拉取
- 失败重试用退避，不要固定间隔猛打
- **息屏时降频或暂停**（息屏管理器已就绪），醒屏立即拉一次
- 必须设 `max_response_size`，防止异常大响应打爆内存

#### 断网：不可解，但仍须如实呈现

**用户确认：逆变器控制全部依赖互联网，无本地方案，断网期间无法可解。**

这不改变新鲜度显示的必要性——目的不是修复连接，而是**让用户知道自己看到的是旧数据**。
断网时面板若继续显示五分钟前的数字且毫无提示，用户会据此做出错误判断。
无法解决连接问题，更应当如实呈现状态。

#### 必须显示数据新鲜度

5 分钟粒度 + 可能断网 = **面板可能长时间显示旧数据而用户无从察觉**。
因此数据模型须带 `last_updated`，界面须表达陈旧度（如「3 分钟前」或超时置灰）。
**这是本设计中最容易被忽略却最影响信任的一点。**

### 架构：数据源藏在接口之后

```
IEssDataSource (接口)
  ├── StubEssDataSource     <- 先做，不依赖网络，可造满电/放电/故障/断网等场景
  └── TursoEssDataSource    <- 后接，实现 HTTP + JSON 解析
```

**先做数据模型与界面，用桩喂数**。收益：
1. 界面开发完全不阻塞于网络
2. 桩可复现真实环境难触发的场景（告警、断网、极值）
3. 将来换 ESS Dashboard API 或加 Pi 代理，**只换适配器，界面零改动**

### 拟定磁贴（基于真实字段，非臆想）

**信息区**
| 磁贴 | 字段 |
| --- | --- |
| 电池 | `soc` + `batt_power`（充/放及功率） |
| 光伏 | `pv_power`，可叠加 `solar_forecast.today_kwh_est` |
| 家庭负载 | `home_load` |
| 电网 | `grid_power`（正负决定买/卖） |
| 电价 | `buy_price` / `feedin_price` |
| 运行模式 | `mode` + **`mode_reason`** |

**`mode_reason` 是墙面板的独特价值**：回答「系统现在为什么这么做」，
这是手机 App 通常不给、而现场最想知道的信息。

**`demand_window`（15:00–20:00）应作为一等状态显示** —— 该时段策略不同，
用户需要一眼知道当前是否处于需量窗口。

**每日汇总区**：`cost_aud` / `earnings_aud` / `grid_buy_kwh` / `grid_sell_kwh` /
**`demand_peak_kw`**（关系需量电费，值得单独强调）

### 数据源解锁的其他功能

| 功能 | 数据来源 | 评价 |
| --- | --- | --- |
| **告警亮屏** | `energy_log.alert` | 唤醒源注册表已就绪，墙面板相对手机的核心优势 |
| **新闻磁贴** | `display_feed` (category='news') | 数据现成 |
| **发电预测** | `solar_forecast` | 与光伏磁贴天然互补 |
| **当日计划** | `daily_plan` 充电窗口 | 可视化「今天打算怎么充放」 |
| 历史趋势 | `energy_log` 历史行 | **框架无图表元素**，只有 `line`/`arc`/`canvas`，简单趋势可自绘，专业图表工作量大 |
| 控制下发 | 需 API 支持 | **慎重**：面板误触会真实改变储能行为，须二次确认 |

### 实施顺序

1. **T3a**：数据模型 + `IEssDataSource` + 桩实现
2. **T3b**：界面（磁贴 + 详情），桩数据驱动，**产出多套视觉风格供选择**
3. **T3c**：`TursoEssDataSource`（HTTP + JSON 解析 + NVS 取凭据）
4. **T3d**：新鲜度显示、失败退避、息屏降频
5. 之后：告警亮屏、发电预测、新闻磁贴

---

## 附：主题系统调查（2026-08，**方案 A 可行，待实施**）

用户选定视觉风格 **Vibrant Space**（已固定为默认），并希望风格可选。
调查结论：**应复用框架主题系统，而非自建一套并行机制。**

### 关键事实（已查证）

| 事实 | 证据 |
| --- | --- |
| `theme_id` 是**自由字符串**，非枚举 | `gui_interface/document.hpp:31`（`std::string theme_id = "default"`） |
| Settings 里的 `"light"`/`"dark"` 只是该 app 自己的常量 | `app_settings/src/settings_app.cpp:142-143` |
| **支持热重应用** | `runtime.hpp:77` `set_theme(theme_id, reapply_loaded_documents)` |
| **框架已持久化主题选择** | `system.hpp:412` `get_stored_gui_theme_id()` |
| 主题可**内联**声明 | 资产条目可为对象（T1 已证实）；parser 有 "inline theme asset" 分支 |

**因此不存在「只支持明暗两种」的限制**，可自由增加命名主题。

### 主题文件结构

```json
{ "type": "theme", "id": "dark",
  "assets": ["color/dark.json", "font/default.json", "size/default.json", "style/default.json"],
  "variants": [ {"when": "${expr(${env.widthDp} < ${env.heightDp})}", "assets": [...]}, ... ] }
```

组成部分：
- **颜色资产**：`{"type":"constant","data":{"colors":{...}}}` → 供 `${color.*}` 引用
- **样式资产**：`{"type":"constant","data":{"styles":{"all":{...},"label":{...}}}}` → 供 `styleRefs` 引用，
  内部再引用 `${color.*}` / `${constant.*}`
- `variants` 按 `env.widthDp`/`heightDp` 做响应式（框架自带竖屏判断表达式）

**换主题 = 换调色板**，所有用 `${color.*}` 和 `styleRefs` 的节点自动跟随。

### 与现状的差距

当前 tile shell 把十六进制**硬编码在 C++ 预设里**，经绑定推送。
改造方向：磁贴改用 `styleRefs` / `${color.*}`，四套风格改写为四个主题。

**收益**：持久化与热切换由框架提供，无需自建；将来新增风格只是加一个主题资产。

### 加载方式

`SystemGuiAccess::load_theme_file(resource_dir, relative_path)`（`gui_access.cpp:648`），
super 在 `prepare_shell_themes()` 中加载 `light.json` / `dark.json`（`system_lifecycle.cpp:173-185`）。
我们的自建 System 不加载 super 资源，**需自行加载或内联声明主题**。

**状态**：用户决定先做 ESS 数据接入，主题改造延后。

---

## 第九部分：磁贴点击不通（2026-08，**未解决，已搁置**）

Launcher 的核心交互是点磁贴，但在自建 System 中**点击事件从未送达 app**：
无回调日志、无 GUI 警告、无解析错误，界面与数据一切正常。

### 已排除（均经实测或代码核对）

| 假设 | 结论 | 依据 |
| --- | --- | --- |
| 触摸链路有问题 | **排除** | 启动日志与 super 逐行一致：`Registered display touch Touch0 ... operation_mode=Interrupt`、`Bound display output Output0 to touch Touch0`、`LVGL Display source created 5 pointer input(s)`、`Display touch gesture enabled` |
| 元素类型不可点 | 排除 | `container` 改为 `button`（`menu_item` 模板根即 `button`），无变化 |
| 事件格式错误 | 排除 | 原写 `{"type":"clicked","action":"x"}` 系臆想；改为 super 真实格式 `effects` + `emitAction`，无变化 |
| 订阅方式错误 | 排除 | 由无回调的 `subscribe_action(action)` 改为带回调重载并持有 `ScopedConnection`（照 `system_super/src/shell_app.cpp:837-852`），`connected()` 为 true，无变化 |
| 订阅时机错误 | 排除 | 与 super 一致：先 `create_view` 后 `subscribe_action` |
| 祖先容器可滚动导致 PressLost | **排除** | Oracle 指出容器默认可滚动（`parser.cpp:3878`），`emitAction` 在 `require_valid_press && press_lost_since_pressed` 时静默 `continue`（`runtime.cpp:6311`），而 super 显式关滚动（`app_launcher.json:4,87`）。给四个祖先加 `scrollable:false` 后**仍不触发** |
| 按压有效性判定 | **排除** | 去掉 `requireValidPress` 并加 `pressed` 事件做对照，**`pressed` 同样不触发** → 事件根本没产生，与 clicked 语义无关 |
| 实例化不绑事件 | 排除 | `create_view` 内确有 `backend->bind_events(handle, stored_record->node.events)`（`runtime.cpp` 约 4855 行附近） |
| 模板结构不同 | 排除 | 与 super 逐项比对：顶层键 `[type,id,node]`、`node.type=button`、events 格式，**完全一致** |

### Oracle 已澄清的机制（有价值，避免重复调研）

- `AppGuiRuntime::subscribe_action(action, handler)` 直连 `System::gui_subscribe_action`
  （`system_core/src/app/app.cpp:342` → `gui.cpp:1047`），**绕过** `make_app_action_forwarder`/`on_action` 那条路
- 屏幕流在 native `on_start` 之前已自动挂载（`manager.cpp:645`），`create_view` 与订阅都在其后
- **`GuiRootKind::JsonString` 不是差异点**：经 `gui_runtime_->load_json`（`gui.cpp:513`）加载后，
  与文件型文档走同一套 document/action 信号路径

### 下一步（若日后继续）

逐项排除已到尽头，应改为**取运行时真相**而非继续推断：在 `gui_lvgl` 后端的
`bind_events` 与 LVGL 命中测试处加临时日志，一次烧录即可看出事件断在哪一层
（实例化时是否真的绑上、触摸时命中了哪个对象）。代价是需临时改动 `gui/` 目录并还原。

**当前决定：搁置。** 面板主要价值（显示真实 ESS 数据）已实现，点击属锦上添花。

---

## 3. 工作量汇总

| 阶段 | 工作量 | 风险 |
| --- | --- | --- |
| A-层1 yaml（含 camera + SD） | 2–3 h | 低 |
| A-层2 setup_device.c | 1–2 h | 低 |
| ~~A-层3 GPIO 背光~~ | ~~4–8 h~~ → **0 h** | **已推翻**，LEDC 可用，仅需 `freq_hz: 1000` |
| A-验证 V1–V8 | 0.5–1 天 | 中 |
| A-验证 V9–V10（UI 适配） | 1–3 天 | **高（不可控）** |
| B-E1~E4（PC 仿真） | 1–2 天 | 低 |
| B-E5（Pi 侧数据源） | 0.5–1 天 | 取决于数据源确认 |
| B-E6~E7 | 1–2 天 | 低 |
| B-E8 真机合入 | 0.5 天 | 低 |

**总计约 6–11 天**，其中 A、B 两部分前期可并行。

---

## 4. 风险登记

| 编号 | 风险 | 影响 | 缓解措施 |
| --- | --- | --- | --- |
| ~~R1~~ | ~~GPIO 背光需改 hal_adaptor 框架层~~ | **已消除** | 实为 PWM 频率问题，LEDC 无需改框架层 |
| R2 | 800×1280 无现成 UI 适配 | 内置 app 布局错乱 | 逐个补 `constants/800x1280.json`；参考 BSP 仓库中 v0.6 时代的 esp_brookesia_phone 示例 |
| R3 | SD 卡 slot 0 引脚填法不确定 | SD 挂载失败 | 先试全填 0，失败再填实际 GPIO |
| R4 | 数据源未确认（"turbux"） | 阻塞 Pi 侧实现 | ESP32 侧按归一化 schema 开发，与数据源解耦 |
| R5 | 摄像头 reset/pwdn 未知 | 摄像头无法初始化 | 优先级最低，可延后；先填 -1 |
| R6 | GSL3680 组件为 GPL-2.0-or-later | 无法上游 | 自用不受影响，已接受 |

---

## 5. 验收标准

**第一部分（板级移植）**
- V1–V10 全部通过
- 触摸坐标经实机标定，四角点击位置准确
- System Super launcher 显示正常，内置三个 app 可正常进出

**第二部分（ESS App）**
- launcher 中出现 ESS 图标，点击可进入
- 主页正确显示电池 SoC、光伏功率、电网功率、负载功率，符号方向正确
- 网络中断时显示 stale 标记而非归零
- 三个页面可正常跳转
- 中英文切换生效
