# 第三方组件补丁说明（JC8012P4A1C）

本工程通过 `override_path` 使用了 **5 个打过补丁或换过版本的第三方组件**，它们放在
`/home/deven/esp/components/` 下，**不在本仓库内**。

> ⚠️ 直接 `git clone` 本仓库**无法编译**。必须先按本文档准备好这 5 个组件。

声明位置：[`examples/system/super/main/idf_component.yml`](../../../../../examples/system/super/main/idf_component.yml)

| 组件 | 版本 | 性质 | 上游出路 |
| --- | --- | --- | --- |
| `espressif/esp-sr` | 2.4.7 | 仅换版本，无代码改动 | 等 `gmf_ai_audio` 放宽版本钉 |
| `espressif/esp_board_manager` | 0.5.15 | 代码补丁 | 值得提 PR |
| `espressif/esp_cam_sensor` | 2.2.0 | 移植上游未合并的驱动 | 上游 PR #46 合并后移除 |
| `jason-mao/av_processor` | 0.6.6 | 新增能力 | 可提 PR |
| `espressif/esp_video` | 2.2.0 | 修正写死的常量 | **应提 issue**，属上游缺陷 |

---

## 1. esp-sr 2.4.7 — 换版本

**问题**：`espressif/gmf_ai_audio` 把 `esp-sr` **精确钉死在 2.4.4**（不是版本范围）。而
2.4.4 的 `CMakeLists.txt` 对「ESP32-P4 硅版本 < v3.0 + ESP-IDF 6.x」这个组合直接
`FATAL_ERROR` 中止编译。本板芯片为 **rev v1.3**，正好命中。

**为什么是 2.4.7**：commit `ae2b0bb`（2026-07-20）移除了该 `FATAL_ERROR`，并新增了
`esp32p4_less_v3_idf6` 预编译库。判据变为：

```cmake
if(CONFIG_ESP32P4_SELECTS_REV_LESS_V3)
    if(IDF_VERSION_MAJOR EQUAL 5)
        set(TARGET_LIB_PATH "esp32p4_less_v3")
    elseif(IDF_VERSION_MAJOR GREATER_EQUAL 6)
        set(TARGET_LIB_PATH "esp32p4_less_v3_idf6")   ← 本板走这一支
    endif()
endif()
```

**改动**：无代码改动，仅覆盖版本。

**准备方式**：
```bash
git clone https://github.com/espressif/esp-sr.git /home/deven/esp/components/esp-sr
```

**验证**：编译日志出现 `TARGET_LIB_PATH is set to: esp32p4_less_v3_idf6`。

**移除条件**：`gmf_ai_audio` 改为接受 `^2.4` 之类的范围后，可直接删除本 override。

**体积**：292 MB（含全部目标平台的预编译库）。

---

## 2. esp_board_manager — SD 卡与 Wi-Fi 共用 SDMMC 控制器

**问题**：ESP32-P4 只有**一个** SDMMC 控制器（`SDMMC_LL_HOST_CTLR_NUMS == 1`），两个
slot 共享它。本板 Wi-Fi 走 ESP32-C6 over SDIO，`esp_hosted` 在 slot 1 上先调用
`sdmmc_host_init()` 并独占该控制器。随后 SD 卡（slot 0）挂载时失败：

```
E SD_HOST: sd_host_create_sdmmc_controller(84): no available sd host controller
```

ESP-IDF **6.0.2** 的 `sdmmc_host_init()` 无「已存在则复用」保护，无条件新建控制器
（master 分支后来才修）。上游记录：[esp-idf#17889](https://github.com/espressif/esp-idf/issues/17889)。

**改动**：`devices/dev_fs_fat/dev_fs_fat_sub_sdmmc.c`

挂载前把 `host.init` / `host.deinit` 换成空实现，复用 `esp_hosted` 已建好的控制器。
这正是 `esp_hosted` 自带示例 `host_sdcard_with_hosted` 采用的官方绕法。

```c
#if CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE
    handle->host.init   = &sdmmc_host_init_borrowed;   // 直接 return ESP_OK
    handle->host.deinit = &sdmmc_host_deinit_borrowed;
#endif
```

**验证**：启动日志出现
`DEV_FS_FAT_SUB_SDMMC: Reusing the SDMMC controller owned by esp_hosted`
且 `DEV_FS_FAT: Filesystem mounted, base path: /sdcard`，同时 Wi-Fi 四个接口正常注册。

**上游价值**：高。任何「P4 + C6 over SDIO + TF 卡」的板子都会遇到，而
`esp_board_manager` 目前**没有**注入这两个函数的钩子。

---

## 3. esp_cam_sensor — 移植 OV02C10 驱动

**问题**：本板摄像头模组是 **OV02C10**（7 位 I2C 地址 `0x36`，芯片 ID `0x5602`，读
寄存器 `0x300A`/`0x300B` 实测确认）。厂商 BSP 文档写的是 SC2336（地址 `0x30`）——
**文档是错的**，`0x30` 上无任何器件应答。

`esp_cam_sensor` 2.2.0 与 2.3.0 均**不含** ov02c10 驱动。上游有
[PR #46](https://github.com/espressif/esp-video-components/pull/46)，2025-10 开启，**至今未合并**。

**为什么不直接升 2.3.0**：`esp_video` 锁定 `esp_cam_sensor: 2.2.*`，跨版本有 API 风险。
因此保持 2.2.0 本体不动，只移植 ov02c10 这一个驱动。

**改动**：
- 新增 `sensors/ov02c10/`（5 个文件，取自 PR #46）
- `CMakeLists.txt` 增加源码/头文件挂载 + `-u ov02c10_detect` 强制链接
- `Kconfig` 增加 `rsource "sensors/ov02c10/Kconfig.ov02c10"`

**板级配置**：
```
CONFIG_CAMERA_OV02C10=y
CONFIG_CAMERA_OV02C10_AUTO_DETECT_MIPI_INTERFACE_SENSOR=y
CONFIG_CAMERA_OV02C10_MIPI_RAW10_1920x1080_30FPS=y
```

**必须选单 lane 版本**：驱动同时提供 `..._1920x1080_30FPS`（单 lane）和
`..._1920x1080_2LAN_30FPS`（双 lane）。本板 CSI **只接了一条数据 lane**，
选双 lane 版本时传感器能正常探测、管线也能启动，但约 1.5 秒后取帧中止：

```
E VID_SRC:      Acquire on in port, ret:-1
E ESP_GMF_TASK: Job failed[...vid_src_proc], st:ESP_GMF_EVENT_STATE_RUNNING
```

现象具有迷惑性——探测成功容易让人以为 lane 配置没问题，实际失败发生在数据通路而非控制通路（I2C）。

**验证**：`ov02c10: Detected Camera sensor PID=0x5602`，且预览稳定运行无 `VID_SRC` 报错。

**移除条件**：PR #46 合并并发布后，改用官方版本即可。

---

## 4. av_processor — 视频管线旋转

**问题**：摄像头模组相对面板**物理安装转了 90 度**，硬件设计固定无法调整。
面板侧无法补偿（JD9365 不支持 `swap_xy`，日志一直报
`swap_xy is not supported by this panel`），传感器侧只有 flip/mirror，无法产生 90 度旋转。

能力其实存在于最底层 —— `gmf_video` 的 `esp_gmf_video_ppa_set_rotation()`，由 P4 的
**PPA 硬件**执行，零帧率代价。但中间两层（`esp_capture`、`av_processor`）都未暴露，
`video_capture_handle_t` 是不透明的 `void *`。

**改动**：`src/video_processor.c`

在 `esp_capture_start()` 成功后（此时管线已建、元素已存在），用 `esp_capture_advance.h`
提供的**公开扩展点**取得缩放元素并设置角度：

```c
esp_capture_sink_get_element_by_tag(capture->sink[i],
        ESP_CAPTURE_STREAM_TYPE_VIDEO, "vid_ppa", &ppa_element);
esp_gmf_video_ppa_set_rotation(ppa_element, CONFIG_AV_PROCESSOR_VIDEO_ROTATION_DEGREE);
```

该接口的头文件注释明确写着用途是「拿到元素做额外设置」，并非 hack。

新增 Kconfig `AV_PROCESSOR_VIDEO_ROTATION_DEGREE`（默认 `0` 即不旋转，对其他板子无副作用）。

**板级配置**：`CONFIG_AV_PROCESSOR_VIDEO_ROTATION_DEGREE=90`

**验证**：`VIDEO_PROCESSOR: Sink 0 video rotated 90 degrees`。

**连带影响**：应用侧取景框宽高比须按**旋转后**的画面计算。见
[`app/brookesia_app_camera/src/camera_app.cpp`](../../../../../app/brookesia_app_camera/src/camera_app.cpp)
中 `SENSOR_ASPECT_W/H`，已由 16:9 改为 9:16 并注明原因。

---

## 5. esp_video — ISP 时钟写死 80MHz

**问题**：`esp_video` 把 ISP 时钟硬编码为 80 MHz：

```c
src/device/esp_video_isp_device.c:44
#define ISP_CLK_FREQ_HZ  (80 * 1000 * 1000)
```

而 ESP-IDF 的 `isp_core.h` 对该字段的注释是
*"suggest twice higher than cam sensor speed"*。实测：

| 源模式 | 像素率 | 80 MHz 下的表现 |
| --- | --- | --- |
| 1288×728@30 | ~28 MP/s | 0 次溢出 |
| 1920×1080@30 | ~62 MP/s | **142 次 `ISP: fifo overflow`**，持续丢帧 |

按「两倍」建议，1080p 需要约 124 MHz。P4 的 ISP 时钟源为 `PLL_F240M`
（`ISP_CLK_SRC_DEFAULT`），最高可取 240 MHz。

**改动**：`src/device/esp_video_isp_device.c` + `Kconfig`

把常量改为 Kconfig 可配，**默认仍为 80 保持上游行为**：

```c
#if defined(CONFIG_ESP_VIDEO_ISP_CLK_FREQ_MHZ)
#define ISP_CLK_FREQ_HZ  (CONFIG_ESP_VIDEO_ISP_CLK_FREQ_MHZ * 1000 * 1000)
#else
#define ISP_CLK_FREQ_HZ  (80 * 1000 * 1000)
#endif
```

**板级配置**：`CONFIG_ESP_VIDEO_ISP_CLK_FREQ_MHZ=240`（240/240 分频比为 1，最干净）

**验证**：1080p 源下 `fifo overflow` 计数从 142 降到 **0**。

**上游价值**：高。这不是本板特有问题 —— 任何跑 1080p 的 P4 板子都会撞上，属于上游
默认值不合理，**建议提 issue**。

---

## 环境准备

```bash
mkdir -p /home/deven/esp/components

# 1. esp-sr：换版本，无补丁
git clone https://github.com/espressif/esp-sr.git /home/deven/esp/components/esp-sr

# 2-5. 其余四个：先取对应版本，再按上文施加补丁
#    最省事的做法是先在一个干净工程里 idf.py build 拉取 managed_components，
#    再复制到 /home/deven/esp/components/ 并删除 .component_hash，然后打补丁。
```

删除 `.component_hash` 是必须的，否则组件管理器会认为本地副本被篡改而报错。

## 磁盘占用

| 组件 | 大小 |
| --- | --- |
| esp-sr | 292 MB |
| av_processor | 19 MB |
| esp_board_manager | 8.3 MB |
| esp_cam_sensor | 5.1 MB |
| esp_video | 5.0 MB |
| **合计** | **约 330 MB** |

## 升级组件时的注意事项

这 5 个 override 会**屏蔽上游更新**。升级前逐项确认：

1. **esp-sr** — 检查 `gmf_ai_audio` 是否已放宽版本约束；若已放宽则删除 override
2. **esp_cam_sensor** — 检查 PR #46 是否合并；若已合并则改用官方版本
3. **esp_board_manager** — 检查 `dev_fs_fat_sub_sdmmc.c` 是否已有 host.init 注入钩子
4. **av_processor** — 检查是否已暴露旋转配置
5. **esp_video** — 检查 `ISP_CLK_FREQ_HZ` 是否已可配

每次升级后都必须重新施加尚未被上游吸收的补丁，否则会静默回退到故障状态 ——
其中第 2、5 两项的故障表现（摄像头不出图、画面丢帧）都不会直接指向根因。
