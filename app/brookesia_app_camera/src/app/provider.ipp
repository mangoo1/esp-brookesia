/*
 * SPDX-FileCopyrightText: 2026 Deven Chen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

system::core::AppManifest CameraAppProvider::get_manifest() const
{
    return CameraApp().get_manifest();
}

std::shared_ptr<system::core::IApp> CameraAppProvider::create_app()
{
    return std::make_shared<CameraApp>();
}

BROOKESIA_SYSTEM_CORE_APP_PROVIDER_REGISTER_WITH_SYMBOL(
    CameraAppProvider,
    APP_ID,
    app_camera_provider_symbol
);
