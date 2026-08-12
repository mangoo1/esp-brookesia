#pragma once

#include "brookesia/system_core/app/iapp.hpp"
#include "brookesia/system_core/app/context.hpp"

namespace esp_brookesia::system::tile {

class TileShellApp : public core::IApp {
public:
    core::AppManifest get_manifest() const override;
    core::AppGuiDescriptor get_gui_descriptor() const override;

    std::expected<void, std::string> on_start(core::AppContext &context) override;
    std::expected<void, std::string> on_stop(core::AppContext &context) override;

private:
    core::AppContext *context_ = nullptr;
};

} // namespace esp_brookesia::system::tile
