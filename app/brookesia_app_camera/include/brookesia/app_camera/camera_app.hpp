/*
 * SPDX-FileCopyrightText: 2026 Deven Chen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "brookesia/service_manager.hpp"
#include "brookesia/system_core.hpp"

namespace esp_brookesia::app::camera {

/**
 * @brief Camera application that renders a live MIPI-CSI preview.
 *
 * The preview is drawn by the video encoder straight to the display output
 * rather than through the GUI, so the JSON UI only supplies the backdrop and
 * status text while this class owns the preview geometry and lifetime.
 */
class CameraApp final: public system::core::IApp {
public:
    CameraApp();
    ~CameraApp() override;

    system::core::AppManifest get_manifest() const override;
    system::core::AppGuiDescriptor get_gui_descriptor() const override;
    std::expected<void, std::string> on_install(system::core::AppContext &context) override;
    void on_uninstall(system::core::AppContext &context) override;
    std::expected<void, std::string> on_start(system::core::AppContext &context) override;
    std::expected<void, std::string> on_resume(system::core::AppContext &context) override;
    std::expected<void, std::string> on_pause(system::core::AppContext &context) override;
    std::expected<void, std::string> on_stop(system::core::AppContext &context) override;
    std::expected<void, std::string> on_action(
        system::core::AppContext &context,
        std::string_view action
    ) override;
    std::expected<void, std::string> on_timer(
        system::core::AppContext &context,
        system::core::TimerId timer_id,
        std::string_view name
    ) override;

private:
    /**
     * @brief Preview rectangle that keeps the sensor aspect ratio on this panel.
     *
     * Computed from the display output at runtime: the preview spans the full
     * output width and its height follows the sensor aspect, centred vertically.
     */
    struct Viewfinder {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    std::expected<Viewfinder, std::string> compute_viewfinder(system::core::AppContext &context) const;
    std::expected<void, std::string> start_preview(system::core::AppContext &context);
    void stop_preview();

    system::core::AppContext *context_ = nullptr;
    /* The encoder binding has to outlive start_preview(): releasing it would
     * drop the service reference while the preview is still running. */
    service::ServiceBinding encoder_binding_;
    bool preview_active_ = false;
};

class CameraAppProvider final: public system::core::IAppProvider {
public:
    system::core::AppManifest get_manifest() const override;
    std::shared_ptr<system::core::IApp> create_app() override;
};

} // namespace esp_brookesia::app::camera
