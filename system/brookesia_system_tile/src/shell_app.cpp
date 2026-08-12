#include "shell_app.hpp"
#include "brookesia/system_core/app/context.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/runtime_manager.hpp"

namespace esp_brookesia::system::tile {

static const char* TILE_SHELL_JSON = R"({
    "version": "0.1.0",
    "assets": [
        {
            "type": "screenFlow",
            "id": "home_flow",
            "screens": [
                "tile_home"
            ],
            "initial": "tile_home"
        },
        {
            "type": "viewScreen",
            "id": "tile_home",
            "mountMode": "dynamic",
            "children": [
                {
                    "type": "container",
                    "id": "tile_grid",
                    "layout": {
                        "type": "grid",
                        "gridTemplateColumns": [
                            "match",
                            "match"
                        ],
                        "gridTemplateRows": [
                            "match",
                            "match",
                            "match"
                        ],
                        "gap": "16dp"
                    },
                    "placement": {
                        "mode": "flow",
                        "width": "match",
                        "height": "match"
                    },
                    "children": [
                        {
                            "type": "container",
                            "id": "tile_0",
                            "layout": {
                                "type": "flex",
                                "flexFlow": "column",
                                "mainAlign": "center",
                                "crossAlign": "center"
                            },
                            "placement": {
                                "mode": "flow",
                                "gridColumn": 0,
                                "gridRow": 0,
                                "alignSelf": "stretch"
                            },
                            "children": [
                                {
                                    "type": "label",
                                    "id": "tile_0_label",
                                    "labelProps": {
                                        "text": "ESS"
                                    },
                                    "placement": {
                                        "mode": "flow"
                                    }
                                }
                            ]
                        },
                        {
                            "type": "container",
                            "id": "tile_1",
                            "layout": {
                                "type": "flex",
                                "flexFlow": "column",
                                "mainAlign": "center",
                                "crossAlign": "center"
                            },
                            "placement": {
                                "mode": "flow",
                                "gridColumn": 1,
                                "gridRow": 0,
                                "alignSelf": "stretch"
                            },
                            "children": [
                                {
                                    "type": "label",
                                    "id": "tile_1_label",
                                    "labelProps": {
                                        "text": "Weather"
                                    },
                                    "placement": {
                                        "mode": "flow"
                                    }
                                }
                            ]
                        },
                        {
                            "type": "container",
                            "id": "tile_2",
                            "layout": {
                                "type": "flex",
                                "flexFlow": "column",
                                "mainAlign": "center",
                                "crossAlign": "center"
                            },
                            "placement": {
                                "mode": "flow",
                                "gridColumn": 0,
                                "gridRow": 1,
                                "alignSelf": "stretch"
                            },
                            "children": [
                                {
                                    "type": "label",
                                    "id": "tile_2_label",
                                    "labelProps": {
                                        "text": "News"
                                    },
                                    "placement": {
                                        "mode": "flow"
                                    }
                                }
                            ]
                        },
                        {
                            "type": "container",
                            "id": "tile_3",
                            "layout": {
                                "type": "flex",
                                "flexFlow": "column",
                                "mainAlign": "center",
                                "crossAlign": "center"
                            },
                            "placement": {
                                "mode": "flow",
                                "gridColumn": 1,
                                "gridRow": 1,
                                "alignSelf": "stretch"
                            },
                            "children": [
                                {
                                    "type": "label",
                                    "id": "tile_3_label",
                                    "labelProps": {
                                        "text": "Stocks"
                                    },
                                    "placement": {
                                        "mode": "flow"
                                    }
                                }
                            ]
                        },
                        {
                            "type": "container",
                            "id": "tile_4",
                            "layout": {
                                "type": "flex",
                                "flexFlow": "column",
                                "mainAlign": "center",
                                "crossAlign": "center"
                            },
                            "placement": {
                                "mode": "flow",
                                "gridColumn": 0,
                                "gridRow": 2,
                                "alignSelf": "stretch"
                            },
                            "children": [
                                {
                                    "type": "label",
                                    "id": "tile_4_label",
                                    "labelProps": {
                                        "text": "Camera"
                                    },
                                    "placement": {
                                        "mode": "flow"
                                    }
                                }
                            ]
                        },
                        {
                            "type": "container",
                            "id": "tile_5",
                            "layout": {
                                "type": "flex",
                                "flexFlow": "column",
                                "mainAlign": "center",
                                "crossAlign": "center"
                            },
                            "placement": {
                                "mode": "flow",
                                "gridColumn": 1,
                                "gridRow": 2,
                                "alignSelf": "stretch"
                            },
                            "children": [
                                {
                                    "type": "label",
                                    "id": "tile_5_label",
                                    "labelProps": {
                                        "text": "Settings"
                                    },
                                    "placement": {
                                        "mode": "flow"
                                    }
                                }
                            ]
                        }
                    ]
                }
            ]
        }
    ]
})";

core::AppManifest TileShellApp::get_manifest() const
{
    return {
        .id = "tile_shell",
        .name = "Tile Shell",
        .localized_names = {{"en", "Tile Shell"}},
        .version = "1.0.0",
        .kind = core::AppKind::Native,
        .visible = false,
        .preload_dom = false,
        .icon_id = "",
        .supported_systems = {},
        .icon_path = "",
        .runtime_type = runtime::BackendType::Unknown,
        .app_path = "",
        .entry = "",
        .resource_dir = "",
        .arguments = {},
    };
}

core::AppGuiDescriptor TileShellApp::get_gui_descriptor() const
{
    return {
        .root_kind = core::GuiRootKind::JsonString,
        .root = TILE_SHELL_JSON,
        .resources = {},
        .screen_flows = {
            core::GuiScreenFlowEntry{
                .screen_flow = "home_flow",
                .layer = core::GuiAppLayer::AppDefault,
            }
        },
    };
}

std::expected<void, std::string> TileShellApp::on_start(core::AppContext &context)
{
    BROOKESIA_LOGI("TileShellApp starting...");
    context_ = &context;
    BROOKESIA_LOGI("TileShellApp started successfully");
    return {};
}

std::expected<void, std::string> TileShellApp::on_stop(core::AppContext &context)
{
    (void)context;
    context_ = nullptr;
    return {};
}

} // namespace esp_brookesia::system::tile
