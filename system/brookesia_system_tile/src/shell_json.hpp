#pragma once

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
            "type": "viewTemplate",
            "id": "ess_tile_template",
            "node": {
                "type": "container",
                "bindings": {
                    "style.bgColor": "bgColor",
                    "style.borderColor": "borderColor",
                    "style.borderWidth": "borderWidth",
                    "style.shadowColor": "shadowColor",
                    "style.shadowWidth": "shadowWidth",
                    "style.radius": "radius"
                },
                "style": {
                    "bgColor": "#333333",
                    "radius": "24dp",
                    "padding": "28dp",
                    "borderColor": "#000000",
                    "borderWidth": "0dp",
                    "shadowColor": "#000000",
                    "shadowWidth": "0dp"
                },
                "layout": {
                    "type": "flex",
                    "flexFlow": "column",
                    "mainAlign": "spaceBetween",
                    "crossAlign": "start"
                },
                "placement": {
                    "mode": "flow",
                    "width": "match",
                    "height": "match"
                },
                "children": [
                    {
                        "type": "label",
                        "id": "title_label",
                        "bindings": {
                            "labelProps.text": "title",
                            "style.textColor": "titleColor"
                        },
                        "labelProps": {
                            "text": "Title"
                        },
                        "style": {
                            "textColor": "#ffffff",
                            "fontSize": "28sp"
                        },
                        "placement": {
                            "mode": "flow"
                        }
                    },
                    {
                        "type": "label",
                        "id": "value_label",
                        "bindings": {
                            "labelProps.text": "value",
                            "style.textColor": "valueColor"
                        },
                        "labelProps": {
                            "text": "--"
                        },
                        "style": {
                            "textColor": "#ffffff",
                            "fontSize": "56sp"
                        },
                        "placement": {
                            "mode": "flow"
                        }
                    },
                    {
                        "type": "label",
                        "id": "sub_label",
                        "bindings": {
                            "labelProps.text": "subValue",
                            "style.textColor": "subValueColor"
                        },
                        "labelProps": {
                            "text": ""
                        },
                        "style": {
                            "textColor": "#aaaaaa",
                            "fontSize": "22sp"
                        },
                        "placement": {
                            "mode": "flow"
                        }
                    }
                ]
            }
        },
        {
            "type": "viewScreen",
            "id": "tile_home",
            "mountMode": "dynamic",
            "children": [
                {
                    "type": "container",
                    "id": "root_container",
                    "bindings": {
                        "style.bgColor": "rootBgColor",
                        "style.bgGradientColor": "rootBgGradientColor",
                        "style.bgGradientDirection": "rootBgGradientDir",
                        "style.bgGradientOpacity": "rootBgGradientOpacity"
                    },
                    "style": {
                        "bgColor": "#000000",
                        "padding": "56dp"
                    },
                    "layout": {
                        "type": "flex",
                        "flexFlow": "column",
                        "mainAlign": "start",
                        "crossAlign": "stretch",
                        "gap": "32dp"
                    },
                    "placement": {
                        "mode": "flow",
                        "width": "match",
                        "height": "match"
                    },
                    "children": [
                        {
                            "type": "container",
                            "id": "header_zone",
                            "layout": {
                                "type": "flex",
                                "flexFlow": "row",
                                "mainAlign": "spaceBetween",
                                "crossAlign": "center"
                            },
                            "placement": {
                                "mode": "flow",
                                "width": "match",
                                "height": "wrap"
                            },
                            "children": [
                                {
                                    "type": "label",
                                    "id": "mode_reason_label",
                                    "bindings": {
                                        "labelProps.text": "modeReason",
                                        "style.textColor": "headerTextColor"
                                    },
                                    "labelProps": {
                                        "text": "Mode: Normal"
                                    },
                                    "style": {
                                        "textColor": "#ffffff",
                                        "fontSize": "24sp"
                                    },
                                    "placement": {
                                        "mode": "flow"
                                    }
                                },
                                {
                                    "type": "label",
                                    "id": "freshness_label",
                                    "bindings": {
                                        "labelProps.text": "freshness",
                                        "style.textColor": "freshnessColor"
                                    },
                                    "labelProps": {
                                        "text": "Updated just now"
                                    },
                                    "style": {
                                        "textColor": "#888888",
                                        "fontSize": "16sp"
                                    },
                                    "placement": {
                                        "mode": "flow"
                                    }
                                }
                            ]
                        },
                        {
                            "type": "container",
                            "id": "tiles_zone",
                            "layout": {
                                "type": "grid",
                                "gridTemplateColumns": [
                                    "match",
                                    "match"
                                ],
                                "gridTemplateRows": [
                                    "320dp",
                                    "320dp"
                                ],
                                "gap": "32dp"
                            },
                            "placement": {
                                "mode": "flow",
                                "width": "match",
                                "height": "match"
                            },
                            "children": []
                        }
                    ]
                }
            ]
        }
    ]
})";
