/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/service_display/macro_configs.h"
#undef BROOKESIA_LOG_TAG
#define BROOKESIA_LOG_TAG BROOKESIA_SERVICE_DISPLAY_LOG_TAG
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/log.hpp"
#include <chrono>

namespace esp_brookesia::service {

inline uint64_t get_current_time_ms()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

}
