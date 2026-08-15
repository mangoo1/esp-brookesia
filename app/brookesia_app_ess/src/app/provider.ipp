/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

namespace esp_brookesia::app::ess {

class EssAppProvider : public system::core::IAppProvider {
public:
    system::core::AppManifest get_manifest() const override {
        return EssApp().get_manifest();
    }

    std::shared_ptr<system::core::IApp> create_app() override {
        return std::make_shared<EssApp>();
    }
};

BROOKESIA_SYSTEM_CORE_APP_PROVIDER_REGISTER_WITH_SYMBOL(
    EssAppProvider,
    "brookesia.ess",
    app_ess_provider_symbol
);

} // namespace esp_brookesia::app::ess

