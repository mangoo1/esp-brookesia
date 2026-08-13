#pragma once

#include <string>

namespace esp_brookesia::system::tile {

struct EssData {
    std::string ts;
    float soc;
    float batt_power;
    float home_load;
    float pv_power;
    float grid_power;
    float buy_price;
    float feedin_price;
    int demand_window;
    int mode;
    std::string mode_reason;
    std::string alert;
    
    long long last_updated;
    bool is_valid;
};

class IEssDataSource {
public:
    virtual ~IEssDataSource() = default;
    virtual EssData get_latest() = 0;
};

class StubEssDataSource : public IEssDataSource {
public:
    StubEssDataSource();
    EssData get_latest() override;
    
private:
    EssData current_data_;
    long long start_time_;
};

} // namespace esp_brookesia::system::tile
