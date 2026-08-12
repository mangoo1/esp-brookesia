#include "shell_app.hpp"
#include "brookesia/system_core/app/context.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/runtime_manager.hpp"
#include "brookesia/system_core/app/gui_runtime.hpp"
#include "brookesia/system_core/app/timer_runtime.hpp"
#include "shell_json.hpp"
#include <string>
#include <vector>

namespace esp_brookesia::system::tile {

core::AppManifest TileShellApp::get_manifest() const
{
    return {
        .id = "tile_shell",
        .name = "Tile Shell",
        .localized_names = {{"en", "Tile Shell"}},
        .version = "1.0.0",
        .kind = core::AppKind::Native,
        .visible = false,
        .preload_dom = false,
        .icon_id = "",
        .supported_systems = {},
        .icon_path = "",
        .runtime_type = runtime::BackendType::Unknown,
        .app_path = "",
        .entry = "",
        .resource_dir = "",
        .arguments = {},
    };
}

core::AppGuiDescriptor TileShellApp::get_gui_descriptor() const
{
    return {
        .root_kind = core::GuiRootKind::JsonString,
        .root = TILE_SHELL_JSON,
        .resources = {},
        .screen_flows = {
            core::GuiScreenFlowEntry{
                .screen_flow = "home_flow",
                .layer = core::GuiAppLayer::AppDefault,
            }
        },
    };
}

std::expected<void, std::string> TileShellApp::on_start(core::AppContext &context)
{
    BROOKESIA_LOGI("TileShellApp starting...");
    context_ = &context;

    std::vector<gui::BindingValueUpdate> binding_updates;

    // Create info tiles
    for (const auto& tile : info_tiles_) {
        auto create_result = context.gui().create_view(
            "info_tile_template",
            "tile_home/root_container/info_zone",
            tile.id
        );
        if (!create_result) {
            BROOKESIA_LOGE("Failed to create info tile %s: %s", tile.id.c_str(), create_result.error().c_str());
            continue;
        }
        
        std::string instance_path = "tile_home/root_container/info_zone/" + tile.id;
        binding_updates.push_back({instance_path, "bgColor", tile.color});
        binding_updates.push_back({instance_path + "/title_label", "title", tile.title});
        binding_updates.push_back({instance_path + "/value_label", "value", tile.initial_value});
    }

    // Create app tiles
    for (const auto& tile : app_tiles_) {
        auto create_result = context.gui().create_view(
            "app_tile_template",
            "tile_home/root_container/apps_zone",
            tile.id
        );
        if (!create_result) {
            BROOKESIA_LOGE("Failed to create app tile %s: %s", tile.id.c_str(), create_result.error().c_str());
            continue;
        }

        std::string instance_path = "tile_home/root_container/apps_zone/" + tile.id;
        binding_updates.push_back({instance_path, "bgColor", tile.color});
        binding_updates.push_back({instance_path + "/app_title_label", "title", tile.title});
    }

    if (!binding_updates.empty()) {
        auto update_result = context.gui().set_binding_values(binding_updates);
        if (!update_result) {
            BROOKESIA_LOGE("Failed to set initial binding values");
        }
    }

    auto timer_result = context.timer().start_periodic("tile_refresh", 1000);
    if (timer_result) {
        timer_id_ = *timer_result;
        BROOKESIA_LOGI("Timer started successfully with ID: %llu", (unsigned long long)timer_id_);
    } else {
        BROOKESIA_LOGE("Failed to start timer: %s", timer_result.error().c_str());
    }

    BROOKESIA_LOGI("TileShellApp started successfully");
    return {};
}

std::expected<void, std::string> TileShellApp::on_stop(core::AppContext &context)
{
    if (timer_id_ != core::INVALID_TIMER_ID) {
        context.timer().stop(timer_id_);
        timer_id_ = core::INVALID_TIMER_ID;
    }
    context_ = nullptr;
    return {};
}

std::expected<void, std::string> TileShellApp::on_timer(core::AppContext &context, core::TimerId timer_id, std::string_view name)
{
    if (timer_id != timer_id_) return {};

    fake_counter_++;
    
    std::vector<gui::BindingValueUpdate> binding_updates;
    
    // Refresh info tiles with fake data
    for (auto& tile : info_tiles_) {
        std::string instance_path = "tile_home/root_container/info_zone/" + tile.id;
        std::string new_value;
        
        if (tile.id == "info_energy") {
            new_value = std::to_string(fake_counter_ * 2) + " kW";
        } else if (tile.id == "info_weather") {
            new_value = std::to_string(20 + (fake_counter_ % 5)) + " °C";
        } else if (tile.id == "info_news") {
            new_value = "News " + std::to_string(fake_counter_);
        } else if (tile.id == "info_stocks") {
            new_value = (fake_counter_ % 2 == 0 ? "+" : "-") + std::to_string(1 + (fake_counter_ % 3)) + ".0%";
        } else {
            new_value = std::to_string(fake_counter_);
        }
        
        tile.initial_value = new_value;
        binding_updates.push_back({instance_path + "/value_label", "value", new_value});
    }

    if (!binding_updates.empty()) {
        auto update_result = context.gui().set_binding_values(binding_updates);
        if (!update_result) {
            BROOKESIA_LOGE("Failed to set timer binding values");
        } else if (fake_counter_ == 1) {
            BROOKESIA_LOGI("First successful refresh of tiles");
        }
    }
    return {};
}

} // namespace esp_brookesia::system::tile
