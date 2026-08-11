/*
 * SPDX-FileCopyrightText: 2026 Deven Chen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/app_camera.hpp"

#include <algorithm>
#include <vector>

#include "boost/json.hpp"

#include "brookesia/service_helper/media/display.hpp"
#include "brookesia/service_helper/media/video.hpp"
#include "brookesia/service_helper/system/device.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::app::camera {
namespace {

static constexpr const char *APP_ID = "brookesia.general.camera";
static constexpr const char *APP_NAME = "Camera";
static constexpr const char *APP_NAME_ZH_CN = "相机";
static constexpr const char *APP_NAME_I18N_KEY = "app_name";
static constexpr const char *APP_ICON_ID = "launcher_icon";
static constexpr const char *APP_ICON_PATH = "res/images/index.json";
static constexpr const char *LOCALE_EN = "en";
static constexpr const char *LOCALE_ZH_CN = "zh_CN";
static constexpr const char *GUI_ROOT = "res/root.json";
static constexpr const char *SCREEN_ID = "camera";
static constexpr const char *HINT_PATH = "/camera/page/hint";

static constexpr const char *DISPLAY_SOURCE_NAME = "Camera";
static constexpr const char *DISPLAY_SOURCE_ROLE = "preview";
static constexpr uint32_t PREVIEW_DRAW_TIMEOUT_MS = 100;
static constexpr uint8_t PREVIEW_FPS = 30;
static constexpr const char *PREVIEW_TIMER_NAME = "camera.preview.start";
static constexpr int PREVIEW_START_DELAY_MS = 300;

/* Both MIPI modes the OV02C10 exposes are 16:9 (1288x728 and 1920x1080), but
 * this board mounts the module turned 90 degrees and the capture pipeline
 * rotates to compensate, so what reaches the display is 9:16. The ratio below
 * therefore describes the rotated frame, not the raw sensor. Only the display
 * side is measured at runtime, because that is what changes when the same app
 * runs on another panel. */
static constexpr uint32_t SENSOR_ASPECT_W = 9;
static constexpr uint32_t SENSOR_ASPECT_H = 16;

std::string make_app_version()
{
    return std::to_string(BROOKESIA_APP_CAMERA_VER_MAJOR) + "." +
           std::to_string(BROOKESIA_APP_CAMERA_VER_MINOR) + "." +
           std::to_string(BROOKESIA_APP_CAMERA_VER_PATCH);
}

using DeviceHelper = service::helper::Device;
using DisplayHelper = service::helper::Display;
using VideoHelper = service::helper::Video;
using VideoEncoderHelper = service::helper::VideoEncoder<0>;

} // namespace

#include "app/lifecycle.ipp"
#include "app/provider.ipp"

} // namespace esp_brookesia::app::camera
