#pragma once

#include <memory>
#include <string>
#include <vector>
#include <expected>

#include "brookesia/system_core.hpp"
#include "data/ess_data.hpp"

namespace esp_brookesia::app::ess {

class EssApp : public system::core::IApp {
public:
    EssApp();
    ~EssApp() override;

    system::core::AppManifest get_manifest() const override;
    system::core::AppGuiDescriptor get_gui_descriptor() const override;

protected:
    std::expected<void, std::string> on_install(system::core::AppContext &context) override;
    void on_uninstall(system::core::AppContext &context) override;
    std::expected<void, std::string> on_start(system::core::AppContext &context) override;
    std::expected<void, std::string> on_resume(system::core::AppContext &context) override;
    std::expected<void, std::string> on_pause(system::core::AppContext &context) override;
    std::expected<void, std::string> on_stop(system::core::AppContext &context) override;
    std::expected<void, std::string> on_action(system::core::AppContext &context, std::string_view action) override;
    std::expected<void, std::string> on_timer(
        system::core::AppContext &context,
        system::core::TimerId timer_id,
        std::string_view name
    ) override;

private:
    std::expected<void, std::string> subscribe_actions(system::core::AppContext &context);
    void handle_data_update(const EssData& data);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::app::ess
