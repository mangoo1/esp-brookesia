#pragma once

#include "brookesia/system_core/app/iapp.hpp"
#include "brookesia/system_core/app/context.hpp"
#include <string>
#include <vector>

namespace esp_brookesia::system::tile {

struct InfoTile {
    std::string id;
    std::string title;
    std::string color;
    std::string initial_value;
};

struct AppTile {
    std::string id;
    std::string title;
    std::string color;
};

class TileShellApp : public core::IApp {
public:
    core::AppManifest get_manifest() const override;
    core::AppGuiDescriptor get_gui_descriptor() const override;

    std::expected<void, std::string> on_start(core::AppContext &context) override;
    std::expected<void, std::string> on_stop(core::AppContext &context) override;
    std::expected<void, std::string> on_timer(core::AppContext &context, core::TimerId timer_id, std::string_view name) override;

private:
    core::AppContext *context_ = nullptr;
    core::TimerId timer_id_ = core::INVALID_TIMER_ID;

    std::vector<InfoTile> info_tiles_ = {
        {"info_energy", "Energy", "#364354", "0 kW"},
        {"info_weather", "Weather", "#1290d8", "20 °C"},
        {"info_news", "News", "#5a2f2d", "Latest"},
        {"info_stocks", "Stocks", "#263d31", "+1.2%"}
    };

    std::vector<AppTile> app_tiles_ = {
        {"app_camera", "Camera", "#4c494b"},
        {"app_settings", "Settings", "#38393a"},
        {"app_music", "Music", "#E8362D"},
        {"app_gallery", "Gallery", "#f4b22c"},
        {"app_files", "Files", "#4dc6fd"},
        {"app_calc", "Calculator", "#1290d8"},
        {"app_clock", "Clock", "#5fd28a"},
        {"app_calendar", "Calendar", "#fdc800"}
    };

    int fake_counter_ = 0;
};

} // namespace esp_brookesia::system::tile
