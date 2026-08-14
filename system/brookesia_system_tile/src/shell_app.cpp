#include "shell_app.hpp"
#include "turso_ess_data_source.hpp"
#include "brookesia/system_core/app/context.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/system_core/app/gui_runtime.hpp"
#include "brookesia/system_core/app/timer_runtime.hpp"
#include "shell_json.hpp"
#include "esp_timer.h"
#include <charconv>
#include <cmath>

namespace esp_brookesia::system::tile {

core::AppManifest TileShellApp::get_manifest() const {
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

core::AppGuiDescriptor TileShellApp::get_gui_descriptor() const {
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

std::expected<void, std::string> TileShellApp::on_start(core::AppContext &context) {
    BROOKESIA_LOGI("TileShellApp starting...");
    context_ = &context;
    
    auto turso_src = std::make_unique<TursoEssDataSource>(context);
    if (turso_src->is_active()) {
        ess_data_source_ = std::move(turso_src);
        BROOKESIA_LOGI("Using TursoEssDataSource");
    } else {
        ess_data_source_ = std::make_unique<StubEssDataSource>();
        BROOKESIA_LOGI("Using StubEssDataSource fallback");
    }

    styles_ = {
        // Preset 1: Vibrant Space (Purple to dark blue gradient, solid dark tiles)
        {
            "Vibrant Space",
            "#0b0c10", "#2c1b3d", "vertical", "255", "#ffffff",
            "#1f2026", "24dp", "#000000", "0dp", "#000000", "0dp",
            "#a8b0c3", "#ffffff", "#888df2", "#a8b0c3"
        },
        // Preset 2: Frosted Glass (Gradient background, translucent tiles with thin bright borders)
        {
            "Frosted Glass",
            "#111827", "#1e3a8a", "vertical", "200", "#e5e7eb",
            "#243044", "32dp", "#8fa3c4", "1dp", "#000000", "16dp",
            "#9ca3af", "#f3f4f6", "#60a5fa", "#9ca3af"
        },
        // Preset 3: High Contrast Neobrutilism (Black background, bright solid tiles, thick borders)
        {
            "High Contrast",
            "#000000", "#000000", "vertical", "0", "#ffffff",
            "#ffff00", "0dp", "#ffffff", "4dp", "#ffffff", "8dp",
            "#000000", "#000000", "#333333", "#ffffff"
        },
        // Preset 4: Soft Minimalist (Light theme, soft shadows, rounded)
        {
            "Soft Minimalist",
            "#f3f4f6", "#f3f4f6", "vertical", "0", "#111827",
            "#ffffff", "16dp", "#e5e7eb", "1dp", "#000000", "24dp",
            "#6b7280", "#111827", "#3b82f6", "#6b7280"
        }
    };

    std::vector<gui::BindingValueUpdate> binding_updates;

    for (const auto& tile_id : tile_ids_) {
        auto create_result = context.gui().create_view(
            "ess_tile_template",
            "tile_home/root_container/tiles_zone",
            tile_id
        );
        if (!create_result) {
            BROOKESIA_LOGE("Failed to create tile %s: %s", tile_id.c_str(), create_result.error().c_str());
        }
    }

    apply_style(styles_[current_style_idx_]);

    auto timer_result = context.timer().start_periodic("tile_refresh", 1000);
    if (timer_result) {
        timer_id_ = *timer_result;
        BROOKESIA_LOGI("Timer started successfully with ID: %llu", (unsigned long long)timer_id_);
    }

    BROOKESIA_LOGI("TileShellApp started successfully");
    return {};
}

std::expected<void, std::string> TileShellApp::on_stop(core::AppContext &context) {
    if (timer_id_ != core::INVALID_TIMER_ID) {
        context.timer().stop(timer_id_);
        timer_id_ = core::INVALID_TIMER_ID;
    }
    context_ = nullptr;
    return {};
}

void TileShellApp::apply_style(const StylePreset& style) {
    BROOKESIA_LOGI("Applying style preset: %s", style.name.c_str());
    std::vector<gui::BindingValueUpdate> bu;
    
    bu.push_back({"tile_home/root_container", "rootBgColor", style.root_bg_color});
    bu.push_back({"tile_home/root_container", "rootBgGradientColor", style.root_gradient_color});
    bu.push_back({"tile_home/root_container", "rootBgGradientDir", style.root_gradient_dir});
    bu.push_back({"tile_home/root_container", "rootBgGradientOpacity", style.root_gradient_opacity});
    
    bu.push_back({"tile_home/root_container/header_zone/mode_reason_label", "headerTextColor", style.header_text_color});
    bu.push_back({"tile_home/root_container/header_zone/freshness_label", "freshnessColor", style.freshness_color});

    for (const auto& tile_id : tile_ids_) {
        std::string p = "tile_home/root_container/tiles_zone/" + tile_id;
        bu.push_back({p, "bgColor", style.tile_bg_color});
        bu.push_back({p, "radius", style.tile_radius});
        bu.push_back({p, "borderColor", style.tile_border_color});
        bu.push_back({p, "borderWidth", style.tile_border_width});
        bu.push_back({p, "shadowColor", style.tile_shadow_color});
        bu.push_back({p, "shadowWidth", style.tile_shadow_width});
        
        bu.push_back({p + "/title_label", "titleColor", style.title_color});
        bu.push_back({p + "/value_label", "valueColor", style.value_color});
        bu.push_back({p + "/sub_label", "subValueColor", style.sub_color});
    }

    context_->gui().set_binding_values(bu);
}

void TileShellApp::update_tiles(const EssData& data) {
    std::vector<gui::BindingValueUpdate> bu;
    
    long long now = esp_timer_get_time() / 1000;
    long long age_ms = now - data.last_updated;
    long long age_sec = age_ms / 1000;
    
    std::string freshness_text;
    std::string freshness_color = styles_[current_style_idx_].freshness_color;
    
    if (age_sec > 300) {
        freshness_text = "STALE DATA (" + std::to_string(age_sec / 60) + "m old)";
        freshness_color = "#ff4444"; // Visually obvious stale warning
    } else {
        freshness_text = "Updated " + std::to_string(age_sec) + "s ago";
    }
    
    bu.push_back({"tile_home/root_container/header_zone/freshness_label", "freshness", freshness_text});
    bu.push_back({"tile_home/root_container/header_zone/freshness_label", "freshnessColor", freshness_color});
    
    std::string mode_text = data.mode_reason;
    if (data.alert != "") {
        mode_text = "ALERT: " + data.alert;
    }
    bu.push_back({"tile_home/root_container/header_zone/mode_reason_label", "modeReason", mode_text});
    if (data.alert != "") {
        bu.push_back({"tile_home/root_container/header_zone/mode_reason_label", "headerTextColor", "#ff4444"});
    } else {
        bu.push_back({"tile_home/root_container/header_zone/mode_reason_label", "headerTextColor", styles_[current_style_idx_].header_text_color});
    }

    auto f2s = [](float val) {
        char buf[32];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val, std::chars_format::fixed, 2);
        return std::string(buf, ptr);
    };

    auto set_tile = [&](const std::string& id, const std::string& title, const std::string& val, const std::string& sub) {
        std::string p = "tile_home/root_container/tiles_zone/" + id;
        bu.push_back({p + "/title_label", "title", title});
        bu.push_back({p + "/value_label", "value", val});
        bu.push_back({p + "/sub_label", "subValue", sub});
    };
    
    /* Direction is shown as a word rather than a sign: a minus in front of a
     * kW figure is easy to misread from across the room. */
    set_tile("tile_soc", "Battery", f2s(data.soc) + " %", "");

    std::string batt_dir = (data.batt_power > 0.01f) ? "Charging"
                         : ((data.batt_power < -0.01f) ? "Discharging" : "Idle");
    set_tile("tile_batt", "Battery Power", f2s(std::abs(data.batt_power)) + " kW", batt_dir);

    set_tile("tile_home", "Home Use", f2s(data.home_load) + " kW", "");

    std::string grid_dir = (data.grid_power < -0.01f) ? "Buying"
                         : ((data.grid_power > 0.01f) ? "Selling" : "Idle");
    set_tile("tile_grid", "Grid Power", f2s(std::abs(data.grid_power)) + " kW", grid_dir);

    context_->gui().set_binding_values(bu);
}

std::expected<void, std::string> TileShellApp::on_timer(core::AppContext &context, core::TimerId timer_id, std::string_view name) {
    if (timer_id != timer_id_) return {};

    EssData data = ess_data_source_->get_latest();
    update_tiles(data);

    return {};
}

} // namespace esp_brookesia::system::tile
