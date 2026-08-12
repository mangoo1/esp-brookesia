#pragma once

#include "brookesia/system_core/system/system.hpp"
#include <memory>
#include <string>
#include <expected>

namespace esp_brookesia::system::tile {

class TileShellApp;

class TileSystem : public core::System {
public:
    struct Config {
        core::System::Config core_config;
    };
    std::expected<void, std::string> init();
    std::expected<void, std::string> init(Config config);

protected:
    std::expected<void, std::string> on_init() override;
    std::expected<void, std::string> on_start() override;
    core::SystemInfo on_get_system_info() const override;

private:
    std::shared_ptr<TileShellApp> shell_app_;
    core::AppId shell_app_id_ = core::INVALID_APP_ID;
};

} // namespace esp_brookesia::system::tile
