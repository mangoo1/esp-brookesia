#pragma once

#include "brookesia/app_ess/app.hpp"

namespace esp_brookesia::app::ess {

static constexpr const char *APP_ID = "brookesia.ess";
static constexpr const char *APP_NAME = "ESS";
static constexpr const char *APP_NAME_ZH_CN = "储能系统";
static constexpr const char *APP_ICON_ID = "app_icon_ess";
static constexpr const char *APP_ICON_PATH = "res/images/index.json";
static constexpr const char *GUI_ROOT = "res/root.json";

static constexpr const char *LOCALE_EN = "en";
static constexpr const char *LOCALE_ZH_CN = "zh_CN";

static constexpr const char *CONTENT_FLOW_ID = "ess_main";
static constexpr const char *PAGE_HOME = "ess_home";
static constexpr const char *PAGE_DETAIL = "ess_detail";

static constexpr const char *ACTION_OPEN_HOME = "ess.open.home";
static constexpr const char *ACTION_OPEN_DETAIL_BATT_PCT = "ess.open.batt_pct";
static constexpr const char *ACTION_OPEN_DETAIL_BATT_PWR = "ess.open.batt_pwr";
static constexpr const char *ACTION_OPEN_DETAIL_HOME = "ess.open.home_use";
static constexpr const char *ACTION_OPEN_DETAIL_GRID = "ess.open.grid";
static constexpr const char *ACTION_BACK = "ess.back";

} // namespace esp_brookesia::app::ess
