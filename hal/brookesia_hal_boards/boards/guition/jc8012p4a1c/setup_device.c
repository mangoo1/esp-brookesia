/*
 * SPDX-FileCopyrightText: 2026 Deven Chen
 * SPDX-License-Identifier: Apache-2.0
 *
 * Board setup hooks for Guition JC8012P4A1C.
 *
 * esp_board_manager calls these two weak factory functions from
 * devices/dev_display_lcd/dev_display_lcd_sub_dsi.c and
 * devices/dev_lcd_touch/dev_lcd_touch_sub_i2c.c. There is no chip whitelist,
 * so a board is free to install any panel or touch driver here.
 */

#include <string.h>
#include "esp_log.h"
#include "dev_display_lcd.h"

#if __has_include(<esp_lcd_jd9365.h>)
#define HAS_JD9365  1
#include "esp_lcd_jd9365.h"
#include "lcd_init_cmds.h"
#endif  /* __has_include(<esp_lcd_jd9365.h>) */

#if __has_include(<esp_lcd_gsl3680.h>)
#define HAS_GSL3680  1
#include "esp_lcd_gsl3680.h"
#endif  /* __has_include(<esp_lcd_gsl3680.h>) */

static const char *TAG = "JC8012P4A1C_SETUP_DEVICE";

#if defined(HAS_JD9365)
__attribute__((weak)) esp_err_t lcd_dsi_panel_factory_entry_t(esp_lcd_dsi_bus_handle_t dsi_handle, dev_display_lcd_config_t *lcd_cfg, dev_display_lcd_handles_t *lcd_handles)
{
    /* The vendor init sequence is mandatory. The driver's built-in defaults
     * omit {0x80, 0x01}, the DSI lane-count select. Without it the DBI channel
     * still answers ID reads with 93 65 04, but the DPI video path produces
     * nothing and the panel stays black. */
    jd9365_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .mipi_config = {
            .dsi_bus = dsi_handle,
            .dpi_config = &lcd_cfg->sub_cfg.dsi.dpi_config,
            .lane_num = 2,
        },
    };
    const esp_lcd_panel_dev_config_t lcd_dev_config = {
        .reset_gpio_num = lcd_cfg->sub_cfg.dsi.reset_gpio_num,
        .rgb_ele_order = lcd_cfg->rgb_ele_order,
        .data_endian = lcd_cfg->data_endian,
        .bits_per_pixel = lcd_cfg->bits_per_pixel,
        .flags = {
            .reset_active_high = lcd_cfg->sub_cfg.dsi.reset_active_high,
        },
        .vendor_config = &vendor_config,
    };

    ESP_LOGI(TAG, "Install JD9365 LCD panel driver");
    esp_err_t ret = esp_lcd_new_panel_jd9365(lcd_handles->io_handle, &lcd_dev_config, &lcd_handles->panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create JD9365 panel driver: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    return ESP_OK;
}
#endif  /* defined(HAS_JD9365) */

#if defined(HAS_GSL3680)
__attribute__((weak)) esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *touch_dev_config, esp_lcd_touch_handle_t *ret_touch)
{

    ESP_LOGI(TAG, "Install GSL3680 touch driver");
    esp_err_t ret = esp_lcd_touch_new_i2c_gsl3680(io, touch_dev_config, ret_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create gsl3680 touch driver: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
#endif  /* defined(HAS_GSL3680) */
