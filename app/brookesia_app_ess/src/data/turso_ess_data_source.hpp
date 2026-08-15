#pragma once

#include "brookesia/service_manager.hpp"

#include "ess_data.hpp"
#include "brookesia/system_core/app/context.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <mutex>
#include <atomic>
#include <string>

namespace esp_brookesia::app::ess {

class TursoEssDataSource : public IEssDataSource {
public:
    TursoEssDataSource(system::core::AppContext& context);
    ~TursoEssDataSource() override;
    
    EssData get_latest() override;
    bool is_active() const { return active_; }

private:
    void fetch_loop();
    bool do_fetch();

    system::core::AppContext& context_;
    EssData current_data_;
    std::mutex data_mutex_;
    
    bool active_ = false;
    std::string url_;
    std::string token_;
    
    service::ServiceBinding http_binding_;
    std::atomic<bool> stop_thread_{false};
    TaskHandle_t task_handle_ = nullptr;
    
    long last_window_ = 0;
    time_t next_retry_time_ = 0;
    long long last_fetch_esp_time_ = 0;
};

} // namespace esp_brookesia::app::ess
