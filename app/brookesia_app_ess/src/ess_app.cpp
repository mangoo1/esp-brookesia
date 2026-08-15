#include "brookesia/app_ess/app.hpp"
#include "brookesia/system_core.hpp"
#include "data/turso_ess_data_source.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::app::ess {

struct EssApp::Impl {
    system::core::AppContext *context = nullptr;
    std::unique_ptr<IEssDataSource> data_source;
    system::core::TimerId poll_timer_id = system::core::INVALID_TIMER_ID;
};

#include "app/lifecycle.ipp"

} // namespace esp_brookesia::app::ess

#include "app/provider.ipp"
