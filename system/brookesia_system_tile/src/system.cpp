#include "brookesia/system_tile/system.hpp"
#include "shell_app.hpp"
#include "brookesia/system_core/app/types.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper/network/wifi.hpp"
#include "brookesia/service_helper/network/sntp.hpp"

namespace esp_brookesia::system::tile {

std::expected<void, std::string> TileSystem::init()
{
    return init(Config{});
}

std::expected<void, std::string> TileSystem::init(Config config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI("TileSystem init starting");

    config.core_config.system_type = "tile";
    
    // We want the most minimal boot, skipping any external apps that could crash
    config.core_config.install_registered_apps = false;
    config.core_config.install_package_apps = false;
    
    // No startup overlay
    config.core_config.startup_overlay.enabled = false;

    if (!config.core_config.gui_backend) {
        return std::unexpected("Tile system requires a GUI backend");
    }

    return core::System::init(std::move(config.core_config));
}

core::SystemInfo TileSystem::on_get_system_info() const
{
    return {
        .name = "TileSystem",
        .version = "1.0.0",
    };
}

std::expected<void, std::string> TileSystem::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI("TileSystem on_init");

    set_system_type("tile");

    if (shell_app_id_ == core::INVALID_APP_ID) {
        shell_app_ = std::make_shared<TileShellApp>();
        auto result = install_app(shell_app_);
        if (!result) {
            shell_app_.reset();
            return std::unexpected("Failed to install shell app: " + result.error());
        }
        shell_app_id_ = *result;
    }

    return {};
}

std::expected<void, std::string> TileSystem::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI("TileSystem on_start");

    /* Services start lazily when something binds them. Nothing else in this
     * system binds networking or time, so without this the Wi-Fi link never
     * comes up and every data fetch fails. */
    bind_background_service(service::helper::Wifi::get_name().data(), wifi_binding_);
    bind_background_service(service::helper::SNTP::get_name().data(), sntp_binding_);

    if (shell_app_id_ != core::INVALID_APP_ID) {
        auto result = start_app(shell_app_id_);
        if (!result) {
            return std::unexpected("Failed to start shell app: " + result.error());
        }
        BROOKESIA_LOGI("TileSystem started shell app successfully");
    }

    return {};
}

void TileSystem::bind_background_service(const char *name, service::ServiceBinding &slot)
{
    if (slot.is_valid()) {
        return;
    }
    auto binding = service::ServiceManager::get_instance().bind(name);
    if (!binding.is_valid()) {
        BROOKESIA_LOGW("Failed to bind service '%s'; features depending on it stay disabled", name);
        return;
    }
    slot = std::move(binding);
    BROOKESIA_LOGI("Bound service '%s'", name);
}

} // namespace esp_brookesia::system::tile
