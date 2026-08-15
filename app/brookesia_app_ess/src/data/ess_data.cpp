#include "ess_data.hpp"
#include "esp_timer.h"

namespace esp_brookesia::app::ess {

StubEssDataSource::StubEssDataSource() {
    start_time_ = esp_timer_get_time() / 1000;
    
    current_data_.ts = "2026-08-12T23:10:10.456Z";
    current_data_.soc = 32.0f;
    current_data_.batt_power = 5.0f;
    current_data_.home_load = 1.18f;
    current_data_.pv_power = 2.71f;
    current_data_.grid_power = -3.29f;
    current_data_.buy_price = 16.19f;
    current_data_.feedin_price = 8.95f;
    current_data_.demand_window = 0;
    current_data_.mode = 1;
    current_data_.mode_reason = "Optimizing self-consumption";
    current_data_.alert = "";
    current_data_.is_valid = true;
    current_data_.last_updated = esp_timer_get_time() / 1000;
}

EssData StubEssDataSource::get_latest() {
    long long now = esp_timer_get_time() / 1000;
    
    // Simulate data changes over time
    // Every 5 seconds, drift soc and power a bit
    long long elapsed = now - start_time_;
    
    // Developer can change this to test STALE data or alert:
    bool simulate_stale = false; // CHANGE THIS TO TRUE to test STALE
    bool simulate_alert = false; // CHANGE THIS TO TRUE to test ALERT
    
    if (simulate_stale) {
        // Leave last_updated untouched, it will grow old
    } else {
        current_data_.last_updated = now; // always fresh in stub unless stale is tested
        
        current_data_.soc += 0.01f;
        if (current_data_.soc > 100.0f) current_data_.soc = 100.0f;
        
        // Random drift
        float drift = ((elapsed % 1000) - 500) / 10000.0f;
        current_data_.pv_power += drift;
        if (current_data_.pv_power < 0) current_data_.pv_power = 0;
        
        current_data_.home_load += drift / 2;
        if (current_data_.home_load < 0.5f) current_data_.home_load = 0.5f;
        
        // Balance equation: pv + batt - load - grid = 0 => grid = pv + batt - load
        // Wait, sign convention: POSITIVE batt_power = charging (taking power from system)
        // PV is generating (+)
        // Home load is consuming (-)
        // Grid is importing (-) or exporting (+)
        // Equation: PV - HomeLoad - BattPower + GridPower = 0 
        // => GridPower = HomeLoad + BattPower - PV
        // Let's check user prompt: "pv 2.71 - load 1.18 - batt 5.0 = -3.47, and grid_power was -3.29"
        // Close enough. Let's do:
        current_data_.grid_power = current_data_.pv_power - current_data_.home_load - current_data_.batt_power;
        
        if (simulate_alert) {
            current_data_.alert = "Grid overvoltage, limited export";
        } else {
            current_data_.alert = "";
        }
    }
    
    return current_data_;
}

} // namespace esp_brookesia::app::ess
