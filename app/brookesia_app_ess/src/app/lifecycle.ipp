#include <iomanip>
#include <sstream>

EssApp::EssApp() : impl_(std::make_unique<Impl>()) {}

EssApp::~EssApp() = default;

system::core::AppManifest EssApp::get_manifest() const {
    return {
        .id = APP_ID,
        .name = APP_NAME,
        .localized_names = {
            {LOCALE_EN, APP_NAME},
            {LOCALE_ZH_CN, APP_NAME_ZH_CN},
        },
        .version = "1.0.0",
        .kind = system::core::AppKind::Native,
        .visible = true,
#ifdef CONFIG_BROOKESIA_APP_ESS_ENABLE_PRELOAD_DOM
        .preload_dom = true,
#else
        .preload_dom = false,
#endif
        .icon_id = APP_ICON_ID,
        .supported_systems = {},
        .icon_path = APP_ICON_PATH,
        .runtime_type = runtime::BackendType::Unknown,
        .app_path = {},
        .entry = {},
        .resource_dir = APP_ID,
        .arguments = {},
    };
}

system::core::AppGuiDescriptor EssApp::get_gui_descriptor() const {
    return {
        .root_kind = system::core::GuiRootKind::File,
        .root = GUI_ROOT,
        .resources = {},
        .screen_flows = {
            system::core::GuiScreenFlowEntry{
                .screen_flow = CONTENT_FLOW_ID,
                .layer = system::core::GuiAppLayer::AppDefault,
            },
        },
    };
}

std::expected<void, std::string> EssApp::on_install(system::core::AppContext &context) {
    (void)context;
    return {};
}

void EssApp::on_uninstall(system::core::AppContext &context) {
    (void)context;
}

std::expected<void, std::string> EssApp::on_start(system::core::AppContext &context) {
    BROOKESIA_LOGI("EssApp on_start");
    impl_->context = &context;

    /* Show the UI in Chinese. Must run before the first data update so the
     * direction words pick the right language. */
    if (auto lang_res = context.gui().set_language("zh_CN"); !lang_res) {
        BROOKESIA_LOGW("Failed to set language zh_CN: %s", lang_res.error().c_str());
    }
    
    // Subscribe to taps
    auto subscribe_res = subscribe_actions(context);
    if (!subscribe_res) {
        return std::unexpected("Failed to subscribe actions: " + subscribe_res.error());
    }

    // Init data source
    impl_->data_source = std::make_unique<TursoEssDataSource>(context);

    // Initial fetch
    handle_data_update(impl_->data_source->get_latest());

    // Setup poll timer every 1s
    auto timer_res = context.timer().start_periodic("ess_poll", 1000);
    if (!timer_res) {
        return std::unexpected("Failed to start poll timer");
    }
    impl_->poll_timer_id = timer_res.value();
    
    return {};
}

std::expected<void, std::string> EssApp::on_resume(system::core::AppContext &context) {
    (void)context;
    // ensure data is updated
    if (impl_->data_source) {
        handle_data_update(impl_->data_source->get_latest());
    }
    return {};
}

std::expected<void, std::string> EssApp::on_pause(system::core::AppContext &context) {
    (void)context;
    return {};
}

std::expected<void, std::string> EssApp::on_stop(system::core::AppContext &context) {
    (void)context;
    if (impl_->poll_timer_id != system::core::INVALID_TIMER_ID) {
        context.timer().stop(impl_->poll_timer_id);
        impl_->poll_timer_id = system::core::INVALID_TIMER_ID;
    }
    impl_->data_source.reset();
    impl_->context = nullptr;
    return {};
}

std::expected<void, std::string> EssApp::on_timer(
    system::core::AppContext &context,
    system::core::TimerId timer_id,
    std::string_view name
) {
    (void)timer_id;
    if (name == "ess_poll") {
        if (impl_->data_source) {
            handle_data_update(impl_->data_source->get_latest());
        }
    }
    return {};
}

std::expected<void, std::string> EssApp::on_action(
    system::core::AppContext &context,
    std::string_view action
) {
    BROOKESIA_LOGI("EssApp on_action: %.*s", (int)action.size(), action.data());
    if (action == ACTION_OPEN_HOME || action == ACTION_BACK) {
        context.gui().trigger_screen_flow(CONTENT_FLOW_ID, action);
    } else if (action == ACTION_OPEN_DETAIL_BATT_PCT ||
               action == ACTION_OPEN_DETAIL_BATT_PWR ||
               action == ACTION_OPEN_DETAIL_HOME ||
               action == ACTION_OPEN_DETAIL_GRID) {
        context.gui().trigger_screen_flow(CONTENT_FLOW_ID, action);
        
        // update detail screen title
        std::string title;
        if (action == ACTION_OPEN_DETAIL_BATT_PCT) title = "Battery Level";
        else if (action == ACTION_OPEN_DETAIL_BATT_PWR) title = "Battery Power";
        else if (action == ACTION_OPEN_DETAIL_HOME) title = "Home Load";
        else if (action == ACTION_OPEN_DETAIL_GRID) title = "Grid Power";
        
        context.gui().set_text("/ess_detail/page/header/title", title);
    }
    return {};
}

std::expected<void, std::string> EssApp::subscribe_actions(system::core::AppContext &context) {
    std::vector<std::string> action_names = {
        ACTION_OPEN_HOME,
        ACTION_OPEN_DETAIL_BATT_PCT,
        ACTION_OPEN_DETAIL_BATT_PWR,
        ACTION_OPEN_DETAIL_HOME,
        ACTION_OPEN_DETAIL_GRID,
        ACTION_BACK
    };

    return context.gui().subscribe_actions(action_names);
}

static std::string format_power(double power_kw) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << std::abs(power_kw) << " kW";
    return ss.str();
}

void EssApp::handle_data_update(const EssData& data) {
    if (!impl_->context) return;
    auto& gui = impl_->context->gui();

    /* set_text does not resolve ${...}; give it the resolved word directly,
     * picking the language from the GUI runtime. */
    const bool zh = gui.get_language() == "zh_CN";
    auto word = [zh](const char *en, const char *cn) { return std::string(zh ? cn : en); };

    // Battery %
    gui.set_text("/ess_home/page/card/batt_pct/value", std::to_string(data.soc) + " %");
    gui.set_text("/ess_home/page/card/batt_pct/dir", "");

    // Battery Power
    gui.set_text("/ess_home/page/card/batt_pwr/value", format_power(data.batt_power));
    std::string batt_dir;
    if (data.batt_power > 0.05) batt_dir = word("Charging", "充电中");
    else if (data.batt_power < -0.05) batt_dir = word("Discharging", "放电中");
    else batt_dir = word("Idle", "待机");
    gui.set_text("/ess_home/page/card/batt_pwr/dir", batt_dir);

    // Home Load
    gui.set_text("/ess_home/page/card/home_use/value", format_power(data.home_load));
    gui.set_text("/ess_home/page/card/home_use/dir", word("Consuming", "用电中"));

    // Grid Power
    gui.set_text("/ess_home/page/card/grid_pwr/value", format_power(data.grid_power));
    std::string grid_dir;
    if (data.grid_power < -0.05) grid_dir = word("Buying", "买电中");
    else if (data.grid_power > 0.05) grid_dir = word("Selling", "卖电中");
    else grid_dir = word("Idle", "待机");
    gui.set_text("/ess_home/page/card/grid_pwr/dir", grid_dir);
}
