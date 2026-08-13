#include "turso_ess_data_source.hpp"
#include "brookesia/service_helper/network/http.hpp"
#include "esp_timer.h"
#include "brookesia/lib_utils.hpp"
#include <fstream>
#include <sstream>
#include <boost/json.hpp>
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace esp_brookesia::system::tile {

static long long parse_iso8601(const std::string& ts) {
    int y, M, d, h, m;
    float s;
    if (sscanf(ts.c_str(), "%d-%d-%dT%d:%d:%fZ", &y, &M, &d, &h, &m, &s) == 6) {
        if (M < 3) { y--; M += 12; }
        long long days = 365LL * y + y / 4 - y / 100 + y / 400 + (153LL * M - 457) / 5 + d - 306;
        long long epoch = (days - 719162) * 86400LL + h * 3600LL + m * 60LL + static_cast<int>(s);
        return epoch;
    }
    return 0;
}

TursoEssDataSource::TursoEssDataSource(core::AppContext& context) : context_(context) {
    /* SD card first so the user can change the token without reflashing; the
     * LittleFS copy is the build-time fallback. */
    for (const auto *path : {"/sdcard/ess_config.json", "/littlefs/ess_config.json"}) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            continue;
        }
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        boost::system::error_code ec;
        auto val = boost::json::parse(buffer.str(), ec);
        if (ec || !val.is_object()) {
            BROOKESIA_LOGW("Ignoring malformed ESS config at %s", path);
            continue;
        }
        auto obj = val.as_object();
        if (obj.contains("url") && obj.at("url").is_string() &&
            obj.contains("token") && obj.at("token").is_string()) {
            url_ = std::string(obj.at("url").as_string().c_str());
            token_ = std::string(obj.at("token").as_string().c_str());
            active_ = true;
            BROOKESIA_LOGI("Loaded ESS config from %s", path);
            break;
        }
        BROOKESIA_LOGW("ESS config at %s lacks url/token", path);
    }

    if (!active_) {
        BROOKESIA_LOGW(
            "TursoEssDataSource inactive: no valid ess_config.json on /sdcard or /littlefs"
        );
        return;
    }
    
    BROOKESIA_LOGI("TursoEssDataSource initialized");
    
    // Set some default invalid data until first fetch
    current_data_.is_valid = false;
    current_data_.last_updated = esp_timer_get_time() / 1000;
    
    auto task_runner = [](void* arg) {
        auto* self = static_cast<TursoEssDataSource*>(arg);
        self->fetch_loop();
        vTaskDelete(nullptr);
    };
    
    /* Services only start when something binds them; nothing else binds HTTP. */
    http_binding_ = service::ServiceManager::get_instance().bind(
        service::helper::Http::get_name().data()
    );
    if (!http_binding_.is_valid()) {
        active_ = false;
        BROOKESIA_LOGE("Failed to bind HTTP service; ESS data will not update");
        return;
    }

    if (xTaskCreate(task_runner, "turso_fetch", 8192, this, 5, &task_handle_) != pdPASS) {
        task_handle_ = nullptr;
        active_ = false;
        BROOKESIA_LOGE("Failed to create Turso fetch task; ESS data will not update");
        return;
    }
}

TursoEssDataSource::~TursoEssDataSource() {
    if (active_ && task_handle_ != nullptr) {
        stop_thread_ = true;
        // Task will delete itself, we could wait but not strictly necessary for this system
    }
}

EssData TursoEssDataSource::get_latest() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_data_;
}

void TursoEssDataSource::fetch_loop() {
    BROOKESIA_LOGI("Turso fetch task running");

    /* Fetch once up front: the panel would otherwise show nothing until the
     * next five-minute boundary. Retry, because at this point Wi-Fi may still be
     * connecting and the HTTP service may not have started yet. */
    for (int attempt = 0; attempt < 20 && !stop_thread_; ++attempt) {
        if (do_fetch()) {
            time_t now = time(nullptr);
            if (now > 1000000000) {
                last_window_ = now / 300;
            }
            last_fetch_esp_time_ = esp_timer_get_time() / 1000;
            break;
        }
        for (int i = 0; i < 30 && !stop_thread_; ++i) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    while (!stop_thread_) {
        time_t now = time(nullptr);
        if (now > 1000000000) { // RTC is synced
            if (now >= next_retry_time_) {
                long window = now / 300;
                if (window > last_window_) {
                    long offset = now % 300;
                    if (offset >= 10) {
                        // Wait if successful, else retry in 30s
                        if (do_fetch()) {
                            last_window_ = window;
                            last_fetch_esp_time_ = esp_timer_get_time() / 1000;
                        } else {
                            next_retry_time_ = now + 30;
                        }
                    }
                }
            }
        } else {
            // No RTC, fallback to esp_timer polling every 5 mins
            long long now_ms = esp_timer_get_time() / 1000;
            if (now_ms - last_fetch_esp_time_ > 300000) {
                if (do_fetch()) {
                    last_fetch_esp_time_ = now_ms;
                } else {
                    last_fetch_esp_time_ = now_ms - 270000; // retry in 30s (300000 - 270000 = 30000)
                }
            }
        }
        
        // Wait ~1s before next check
        for (int i = 0; i < 10 && !stop_thread_; ++i) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

bool TursoEssDataSource::do_fetch() {
    using HttpHelper = esp_brookesia::service::helper::Http;
    
    boost::json::object body_json;
    boost::json::array requests;
    boost::json::object execute_req;
    execute_req["type"] = "execute";
    boost::json::object stmt;
    stmt["sql"] = "SELECT ts,soc,batt_power,home_load,pv_power,grid_power,buy_price,feedin_price,demand_window,mode,mode_reason,alert FROM energy_log ORDER BY ts DESC LIMIT 1";
    execute_req["stmt"] = stmt;
    requests.push_back(execute_req);
    boost::json::object close_req;
    close_req["type"] = "close";
    requests.push_back(close_req);
    body_json["requests"] = requests;
    
    HttpHelper::HttpRequest req;
    req.url = url_;
    req.method = HttpHelper::HttpMethod::Post;
    req.headers["Authorization"] = "Bearer " + token_;
    req.headers["Content-Type"] = "application/json";
    req.body = boost::json::serialize(body_json);
    req.timeout_ms = 15000;
    req.tls_verify = HttpHelper::TlsVerifyMode::Default;
    req.use_crt_bundle = true;
    req.max_response_size = 65536;
    req.retry_count = 0;
    
    auto result = HttpHelper::call_function_sync<boost::json::object>(
        HttpHelper::FunctionId::Request,
        BROOKESIA_DESCRIBE_TO_JSON(req).as_object()
    );
    
    if (!result) {
        BROOKESIA_LOGE("Turso HTTP submit failed: %s", result.error().c_str());
        return false;
    }
    
    HttpHelper::HttpResponse resp;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(*result, resp)) {
        BROOKESIA_LOGE("Turso HTTP response parse failed");
        return false;
    }
    
    if (resp.error != HttpHelper::ErrorCode::Ok || resp.status_code < 200 || resp.status_code >= 300) {
        BROOKESIA_LOGE("Turso HTTP request error: code=%d", resp.status_code);
        return false;
    }
    
    boost::system::error_code ec;
    auto val = boost::json::parse(resp.body, ec);
    if (ec || !val.is_object()) {
        BROOKESIA_LOGE("Turso response invalid JSON");
        return false;
    }
    
    auto root = val.as_object();
    if (!root.contains("results") || !root.at("results").is_array()) return false;
    auto results = root.at("results").as_array();
    if (results.empty() || !results[0].is_object()) return false;
    
    auto res0 = results[0].as_object();
    if (!res0.contains("type") || res0.at("type").as_string() != "ok") {
        BROOKESIA_LOGE("Turso query error response");
        return false;
    }
    
    if (!res0.contains("response") || !res0.at("response").is_object()) return false;
    auto response = res0.at("response").as_object();
    if (!response.contains("result") || !response.at("result").is_object()) return false;
    auto result_obj = response.at("result").as_object();
    if (!result_obj.contains("rows") || !result_obj.at("rows").is_array()) return false;
    auto rows = result_obj.at("rows").as_array();
    if (rows.empty() || !rows[0].is_array()) return true; 
    
    auto row = rows[0].as_array();
    if (row.size() < 12) return false;
    
    auto get_float = [](const boost::json::value& v) -> float {
        if (!v.is_object()) return 0.0f;
        auto const& obj = v.as_object();
        auto type_ptr = obj.if_contains("type");
        if (!type_ptr || !type_ptr->is_string()) return 0.0f;
        if (type_ptr->as_string() == "null") return 0.0f;
        auto val_ptr = obj.if_contains("value");
        if (!val_ptr) return 0.0f;
        if (val_ptr->is_double()) return static_cast<float>(val_ptr->as_double());
        if (val_ptr->is_int64()) return static_cast<float>(val_ptr->as_int64());
        if (val_ptr->is_number()) return static_cast<float>(val_ptr->as_double());
        if (val_ptr->is_string()) {
            const char* s = val_ptr->as_string().c_str();
            char* end;
            float f = std::strtof(s, &end);
            return (end == s) ? 0.0f : f;
        }
        return 0.0f;
    };
    
    auto get_int = [](const boost::json::value& v) -> int {
        if (!v.is_object()) return 0;
        auto const& obj = v.as_object();
        auto type_ptr = obj.if_contains("type");
        if (!type_ptr || !type_ptr->is_string()) return 0;
        if (type_ptr->as_string() == "null") return 0;
        auto val_ptr = obj.if_contains("value");
        if (!val_ptr) return 0;
        if (val_ptr->is_int64()) return static_cast<int>(val_ptr->as_int64());
        if (val_ptr->is_double()) return static_cast<int>(val_ptr->as_double());
        if (val_ptr->is_string()) {
            const char* s = val_ptr->as_string().c_str();
            char* end;
            long i = std::strtol(s, &end, 10);
            return (end == s) ? 0 : static_cast<int>(i);
        }
        return 0;
    };
    
    auto get_string = [](const boost::json::value& v) -> std::string {
        if (!v.is_object()) return "";
        auto const& obj = v.as_object();
        auto type_ptr = obj.if_contains("type");
        if (!type_ptr || !type_ptr->is_string()) return "";
        if (type_ptr->as_string() == "null") return "";
        auto val_ptr = obj.if_contains("value");
        if (!val_ptr) return "";
        if (val_ptr->is_string()) return std::string(val_ptr->as_string().c_str());
        return "";
    };
    
    EssData data;
    data.ts = get_string(row[0]);
    data.soc = get_float(row[1]);
    data.batt_power = get_float(row[2]);
    data.home_load = get_float(row[3]);
    data.pv_power = get_float(row[4]);
    data.grid_power = get_float(row[5]);
    data.buy_price = get_float(row[6]);
    data.feedin_price = get_float(row[7]);
    data.demand_window = get_int(row[8]);
    data.mode = get_int(row[9]);
    data.mode_reason = get_string(row[10]);
    data.alert = get_string(row[11]);
    data.is_valid = true;
    
    long long ts_sec = parse_iso8601(data.ts);
    long long current_rtc = time(nullptr);
    if (ts_sec > 0 && current_rtc > 1000000000) {
        long long age_sec = current_rtc - ts_sec;
        if (age_sec < 0) age_sec = 0;
        data.last_updated = (esp_timer_get_time() / 1000) - (age_sec * 1000);
    } else {
        data.last_updated = esp_timer_get_time() / 1000;
    }
    
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        current_data_ = data;
    }
    
    BROOKESIA_LOGI("Turso fetch successful. Data age: %lld s", (long long)(current_rtc - ts_sec));
    return true;
}

} // namespace esp_brookesia::system::tile
