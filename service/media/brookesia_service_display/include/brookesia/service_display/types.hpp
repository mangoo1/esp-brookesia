/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"
#include "brookesia/service_helper/media/display.hpp"

namespace esp_brookesia::service::display {

using PixelFormat = helper::Display::PixelFormat;
using OutputSlot = helper::Display::OutputSlot;
using TouchOperationMode = helper::Display::TouchOperationMode;
using OutputTouchCapability = helper::Display::OutputTouchCapability;
using OutputBacklightCapability = helper::Display::OutputBacklightCapability;
using OutputInfo = helper::Display::OutputInfo;
using SourceInfo = helper::Display::SourceInfo;
using TouchInfo = helper::Display::TouchInfo;
using SourceState = helper::Display::SourceState;
using TouchGestureEventType = helper::Display::TouchGestureEventType;
using TouchGestureDirection = helper::Display::TouchGestureDirection;
using TouchGestureArea = helper::Display::TouchGestureArea;
using TouchGestureThreshold = helper::Display::TouchGestureThreshold;
using TouchGestureConfig = helper::Display::TouchGestureConfig;
using TouchGestureInfo = helper::Display::TouchGestureInfo;
using TouchPoint = hal::display::TouchIface::Point;

struct FrameInfo {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    PixelFormat pixel_format = PixelFormat::RGB565;
};

enum class PresentResult {
    Presented,
    DroppedNotActive,
    DroppedInvalidFrame,
    DroppedQueueFull,
    Error,
};

enum class PresentSubmitState {
    Queued,
    DroppedNotActive,
    DroppedInvalidFrame,
    Error,
};

struct AsyncSubmitResult {
    uint32_t frame_id = 0;
    PresentSubmitState state = PresentSubmitState::Error;
};

struct TouchSnapshot {
    std::vector<TouchPoint> points;
    uint32_t sequence = 0;
    uint64_t updated_at_ms = 0;
    bool valid = false;
};

struct BacklightScheduleWindow {
    uint8_t weekday_mask = 0; // bit 0=Mon, ..., bit 6=Sun
    uint8_t start_hour = 0;
    uint8_t start_minute = 0;
    uint8_t end_hour = 0;
    uint8_t end_minute = 0;

    bool operator==(const BacklightScheduleWindow &other) const {
        return weekday_mask == other.weekday_mask &&
               start_hour == other.start_hour &&
               start_minute == other.start_minute &&
               end_hour == other.end_hour &&
               end_minute == other.end_minute;
    }
};

struct BacklightScheduleConfig {
    bool enable = false;
    std::vector<BacklightScheduleWindow> windows;

    bool operator==(const BacklightScheduleConfig &other) const {
        if (enable != other.enable) return false;
        if (windows.size() != other.windows.size()) return false;
        for (size_t i = 0; i < windows.size(); ++i) {
            if (!(windows[i] == other.windows[i])) return false;
        }
        return true;
    }
};

BROOKESIA_DESCRIBE_ENUM(PresentResult, Presented, DroppedNotActive, DroppedInvalidFrame, DroppedQueueFull, Error);
BROOKESIA_DESCRIBE_ENUM(PresentSubmitState, Queued, DroppedNotActive, DroppedInvalidFrame, Error);
BROOKESIA_DESCRIBE_STRUCT(FrameInfo, (), (x, y, width, height, pixel_format));
BROOKESIA_DESCRIBE_STRUCT(AsyncSubmitResult, (), (frame_id, state));
BROOKESIA_DESCRIBE_STRUCT(TouchSnapshot, (), (points, sequence, updated_at_ms, valid));
BROOKESIA_DESCRIBE_STRUCT(BacklightScheduleWindow, (), (weekday_mask, start_hour, start_minute, end_hour, end_minute));
BROOKESIA_DESCRIBE_STRUCT(BacklightScheduleConfig, (), (enable, windows));

} // namespace esp_brookesia::service::display
