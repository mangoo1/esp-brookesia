/*
 * SPDX-FileCopyrightText: 2026 Deven Chen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

CameraApp::CameraApp() = default;

CameraApp::~CameraApp() = default;

system::core::AppManifest CameraApp::get_manifest() const
{
    return {
        .id = APP_ID,
        .name = APP_NAME,
        .localized_names = {
            {LOCALE_EN, APP_NAME},
            {LOCALE_ZH_CN, APP_NAME_ZH_CN},
        },
        .version = make_app_version(),
        .kind = system::core::AppKind::Native,
        .visible = true,
        .preload_dom = BROOKESIA_APP_CAMERA_ENABLE_PRELOAD_DOM,
        .icon_id = APP_ICON_ID,
        .supported_systems = {},
        .icon_path = APP_ICON_PATH,
        .runtime_type = runtime::BackendType::Unknown,
        .app_path = {},
        .entry = {},
        .resource_dir = BROOKESIA_APP_CAMERA_RESOURCE_DIR,
        .arguments = {},
    };
}

system::core::AppGuiDescriptor CameraApp::get_gui_descriptor() const
{
    return {
        .root_kind = system::core::GuiRootKind::File,
        .root = GUI_ROOT,
        .resources = {},
        .screen_flows = {
            system::core::GuiScreenFlowEntry{
                .screen_flow = "camera_main",
                .layer = system::core::GuiAppLayer::AppDefault,
            },
        },
    };
}

std::expected<void, std::string> CameraApp::on_install(system::core::AppContext &context)
{
    (void)context;
    return {};
}

void CameraApp::on_uninstall(system::core::AppContext &context)
{
    (void)context;
}

std::expected<CameraApp::Viewfinder, std::string> CameraApp::compute_viewfinder(
    system::core::AppContext &context
) const
{
    (void)context;

    auto display_binding = service::ServiceManager::get_instance().bind(DisplayHelper::get_name().data());
    if (!display_binding.is_valid()) {
        return std::unexpected("Failed to bind display service");
    }
    auto outputs_result = DisplayHelper::call_function_sync<boost::json::array>(
                              DisplayHelper::FunctionId::GetOutputs
                          );
    if (!outputs_result) {
        return std::unexpected("Failed to query Display outputs: " + outputs_result.error());
    }

    std::vector<DisplayHelper::OutputInfo> outputs;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(outputs_result.value(), outputs)) {
        return std::unexpected("Failed to parse Display outputs");
    }
    if (outputs.empty()) {
        return std::unexpected("No Display output is available");
    }

    const auto &output = outputs.front();
    if ((output.width == 0) || (output.height == 0)) {
        return std::unexpected("Display output reports a zero dimension");
    }

    Viewfinder viewfinder;
    viewfinder.width = output.width;
    viewfinder.height = output.width * SENSOR_ASPECT_H / SENSOR_ASPECT_W;
    if (viewfinder.height > output.height) {
        // Landscape panel: the sensor is taller than the output allows, so fit
        // to height instead and centre horizontally.
        viewfinder.height = output.height;
        viewfinder.width = output.height * SENSOR_ASPECT_W / SENSOR_ASPECT_H;
    }
    viewfinder.x = (output.width - viewfinder.width) / 2;
    viewfinder.y = (output.height - viewfinder.height) / 2;

    return viewfinder;
}

std::expected<void, std::string> CameraApp::start_preview(system::core::AppContext &context)
{
    if (preview_active_) {
        return {};
    }

    if (!VideoEncoderHelper::is_available()) {
        return std::unexpected("Video encoder service is not available");
    }

    if (!encoder_binding_.is_valid()) {
        auto binding = service::ServiceManager::get_instance().bind(VideoEncoderHelper::get_name().data());
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind video encoder service");
        }
        encoder_binding_ = std::move(binding);
    }

    auto device_binding = service::ServiceManager::get_instance().bind(DeviceHelper::get_name().data());
    if (!device_binding.is_valid()) {
        return std::unexpected("Failed to bind device service");
    }
    auto device_result = DeviceHelper::call_function_sync<boost::json::array>(
                             DeviceHelper::FunctionId::GetCameraDeviceInfos
                         );
    if (!device_result) {
        return std::unexpected("Failed to query camera devices: " + device_result.error());
    }

    DeviceHelper::CameraDeviceInfos device_infos;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(device_result.value(), device_infos)) {
        return std::unexpected("Failed to parse camera device infos");
    }

    auto device_it = std::find_if(device_infos.begin(), device_infos.end(), [](const auto & info) {
        return !info.device_path.empty();
    });
    if (device_it == device_infos.end()) {
        return std::unexpected("No camera device path is available");
    }

    auto viewfinder_result = compute_viewfinder(context);
    if (!viewfinder_result) {
        return std::unexpected(viewfinder_result.error());
    }
    const auto viewfinder = viewfinder_result.value();

    VideoHelper::EncoderConfig encoder_config{
        .sinks = std::vector<VideoHelper::EncoderSinkInfo>({
            {
                .format = VideoHelper::EncoderSinkFormat::Max,
                .width = static_cast<uint16_t>(viewfinder.width),
                .height = static_cast<uint16_t>(viewfinder.height),
                .fps = PREVIEW_FPS,
            },
        }),
        /* Frames only reach the display through on_encoder_frame(), which the
         * service invokes automatically in stream mode. With stream mode off it
         * fires solely from an explicit FetchFrame call, so a preview-only app
         * would never present anything. */
        .enable_stream_mode = true,
        .source = VideoHelper::EncoderSourceConfig{
            .device_path = device_it->device_path,
        },
        .display = VideoHelper::EncoderDisplayConfig{
            .output_name = {},
            .source_name = DISPLAY_SOURCE_NAME,
            .source_role = DISPLAY_SOURCE_ROLE,
            .x = viewfinder.x,
            .y = viewfinder.y,
            .draw_timeout_ms = PREVIEW_DRAW_TIMEOUT_MS,
            .publish_sink_event = false,
            .activate_source = true,
            .sink_index = 0,
        },
    };

    auto open_result = VideoEncoderHelper::call_function_sync(
                           VideoEncoderHelper::FunctionId::Open,
                           BROOKESIA_DESCRIBE_TO_JSON(encoder_config).as_object()
                       );
    if (!open_result) {
        return std::unexpected("Failed to open video encoder: " + open_result.error());
    }

    auto start_result = VideoEncoderHelper::call_function_sync(VideoEncoderHelper::FunctionId::Start);
    if (!start_result) {
        (void)VideoEncoderHelper::call_function_sync(VideoEncoderHelper::FunctionId::Close);
        return std::unexpected("Failed to start video encoder: " + start_result.error());
    }

    preview_active_ = true;
    BROOKESIA_LOGI(
        "Camera preview started: %1%x%2% at (%3%,%4%) from %5%",
        viewfinder.width, viewfinder.height, viewfinder.x, viewfinder.y, device_it->device_path
    );

    return {};
}

void CameraApp::stop_preview()
{
    if (!preview_active_) {
        return;
    }
    preview_active_ = false;

    auto close_result = VideoEncoderHelper::call_function_sync(VideoEncoderHelper::FunctionId::Close);
    if (!close_result) {
        BROOKESIA_LOGW("Failed to close video encoder: %1%", close_result.error());
    }

    encoder_binding_ = {};
}

std::expected<void, std::string> CameraApp::on_start(system::core::AppContext &context)
{
    /* The preview cannot start inline here. on_start() runs before the app
     * reaches the foreground, and the screen-flow transition that follows hands
     * the display output back to the GUI source, silently dropping a preview
     * started this early. on_resume() is not invoked on a first launch either,
     * so a short delay is the reliable hook: by the time it fires the app owns
     * the output. */
    context_ = &context;

    auto timer_result = context.timer().start_delayed(PREVIEW_TIMER_NAME, PREVIEW_START_DELAY_MS);
    if (!timer_result) {
        BROOKESIA_LOGE("Failed to schedule preview start: %1%", timer_result.error());
        (void)context.gui().set_text(HINT_PATH, timer_result.error());
    }

    return {};
}

std::expected<void, std::string> CameraApp::on_timer(
    system::core::AppContext &context,
    system::core::TimerId timer_id,
    std::string_view name
)
{
    (void)timer_id;
    if (name != PREVIEW_TIMER_NAME) {
        return {};
    }

    if (auto result = start_preview(context); !result) {
        BROOKESIA_LOGE("%1%", result.error());
        (void)context.gui().set_text(HINT_PATH, result.error());
        return {};
    }

    (void)context.gui().set_text(HINT_PATH, "");
    return {};
}

std::expected<void, std::string> CameraApp::on_resume(system::core::AppContext &context)
{
    context_ = &context;

    if (auto result = start_preview(context); !result) {
        BROOKESIA_LOGE("%1%", result.error());
        (void)context.gui().set_text(HINT_PATH, result.error());
        return {};
    }

    (void)context.gui().set_text(HINT_PATH, "");
    return {};
}

std::expected<void, std::string> CameraApp::on_pause(system::core::AppContext &context)
{
    (void)context;
    stop_preview();
    return {};
}

std::expected<void, std::string> CameraApp::on_stop(system::core::AppContext &context)
{
    (void)context;
    stop_preview();
    context_ = nullptr;
    return {};
}

std::expected<void, std::string> CameraApp::on_action(
    system::core::AppContext &context,
    std::string_view action
)
{
    (void)context;
    (void)action;
    return {};
}
