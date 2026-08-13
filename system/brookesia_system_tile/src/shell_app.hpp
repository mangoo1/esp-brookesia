#pragma once

#include "brookesia/system_core/app/iapp.hpp"
#include "brookesia/system_core/app/context.hpp"
#include "ess_data.hpp"
#include <string>
#include <vector>
#include <memory>

namespace esp_brookesia::system::tile {

struct StylePreset {
    std::string name;
    std::string root_bg_color;
    std::string root_gradient_color;
    std::string root_gradient_dir;
    std::string root_gradient_opacity;
    std::string header_text_color;
    
    std::string tile_bg_color;
    std::string tile_radius;
    std::string tile_border_color;
    std::string tile_border_width;
    std::string tile_shadow_color;
    std::string tile_shadow_width;
    
    std::string title_color;
    std::string value_color;
    std::string sub_color;
    std::string freshness_color;
};

class TileShellApp : public core::IApp {
public:
    core::AppManifest get_manifest() const override;
    core::AppGuiDescriptor get_gui_descriptor() const override;

    std::expected<void, std::string> on_start(core::AppContext &context) override;
    std::expected<void, std::string> on_stop(core::AppContext &context) override;
    std::expected<void, std::string> on_timer(core::AppContext &context, core::TimerId timer_id, std::string_view name) override;

private:
    void apply_style(const StylePreset& style);
    void update_tiles(const EssData& data);

    core::AppContext *context_ = nullptr;
    core::TimerId timer_id_ = core::INVALID_TIMER_ID;

    std::unique_ptr<IEssDataSource> ess_data_source_;
    int current_style_idx_ = 0;
    
    std::vector<StylePreset> styles_;
    
    std::vector<std::string> tile_ids_ = {
        "tile_battery",
        "tile_solar",
        "tile_home",
        "tile_grid",
        "tile_price",
        "tile_demand"
    };
};

} // namespace esp_brookesia::system::tile
