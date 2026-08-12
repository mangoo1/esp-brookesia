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
            "id": "info_tile_template",
            "node": {
                "type": "container",
                "bindings": {
                    "style.bgColor": "bgColor"
                },
                "style": {
                    "bgColor": "#333333",
                    "radius": "24dp",
                    "padding": "24dp"
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
                            "labelProps.text": "title"
                        },
                        "labelProps": {
                            "text": "Title"
                        },
                        "style": {
                            "textColor": "#ffffff",
                            "fontSize": "20sp"
                        },
                        "placement": {
                            "mode": "flow"
                        }
                    },
                    {
                        "type": "label",
                        "id": "value_label",
                        "bindings": {
                            "labelProps.text": "value"
                        },
                        "labelProps": {
                            "text": "--"
                        },
                        "style": {
                            "textColor": "#ffffff",
                            "fontSize": "36sp"
                        },
                        "placement": {
                            "mode": "flow"
                        }
                    }
                ]
            }
        },
        {
            "type": "viewTemplate",
            "id": "app_tile_template",
            "node": {
                "type": "container",
                "bindings": {
                    "style.bgColor": "bgColor"
                },
                "style": {
                    "bgColor": "#555555",
                    "radius": "16dp",
                    "padding": "16dp"
                },
                "layout": {
                    "type": "flex",
                    "flexFlow": "column",
                    "mainAlign": "center",
                    "crossAlign": "center"
                },
                "placement": {
                    "mode": "flow",
                    "width": "160dp",
                    "height": "160dp"
                },
                "children": [
                    {
                        "type": "label",
                        "id": "app_title_label",
                        "bindings": {
                            "labelProps.text": "title"
                        },
                        "labelProps": {
                            "text": "App"
                        },
                        "style": {
                            "textColor": "#ffffff",
                            "fontSize": "16sp"
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
                    "style": {
                        "bgColor": "#000000",
                        "padding": "32dp"
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
                            "id": "info_zone",
                            "layout": {
                                "type": "grid",
                                "gridTemplateColumns": [
                                    "match",
                                    "match"
                                ],
                                "gridTemplateRows": [
                                    "match",
                                    "match"
                                ],
                                "gap": "24dp"
                            },
                            "placement": {
                                "mode": "flow",
                                "width": "match",
                                "height": "480dp"
                            },
                            "children": []
                        },
                        {
                            "type": "container",
                            "id": "apps_zone",
                            "commonProps": {
                                "scrollable": true
                            },
                            "layout": {
                                "type": "flex",
                                "flexFlow": "rowWrap",
                                "gap": "24dp",
                                "mainAlign": "start",
                                "crossAlign": "start"
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
