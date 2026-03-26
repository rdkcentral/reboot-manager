/**
 * Copyright 2026 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOCK_REBOOT_UTILS_H
#define MOCK_REBOOT_UTILS_H

#include <gmock/gmock.h>

extern "C" {
    #include "update-reboot-info.h"
}

class MockRebootUtils {
public:
    MOCK_METHOD(int, acquire_lock, (const char *lockDir), ());
    MOCK_METHOD(int, release_lock, (const char *lockDir), ());
    MOCK_METHOD(int, parse_device_properties, (EnvContext *ctx), ());
    MOCK_METHOD(int, parse_legacy_log, (const char *logPath, RebootInfo *info), ());
    MOCK_METHOD(int, should_update_reboot_info, (const EnvContext *ctx), ());
    MOCK_METHOD(int, read_brcm_previous_reboot_reason, (HardwareReason *hw), ());
    MOCK_METHOD(int, detect_kernel_panic, (const EnvContext *ctx, PanicInfo *panicInfo), ());
    MOCK_METHOD(int, check_firmware_failure, (const EnvContext *ctx, FirmwareFailure *fwFailure), ());
    MOCK_METHOD(int, classify_reboot_reason, (RebootInfo *info, const EnvContext *ctx, const HardwareReason *hwReason, const PanicInfo *panicInfo, const FirmwareFailure *fwFailure), ());
    MOCK_METHOD(int, write_reboot_info, (const char *path, const RebootInfo *info), ());
    MOCK_METHOD(int, write_hardpower, (const char *path, const char *timestamp), ());
    MOCK_METHOD(int, copy_keypress_info, (const char *srcPath, const char *destPath), ());
};

extern MockRebootUtils* g_mockRebootUtils;

#endif // MOCK_REBOOT_UTILS_H
