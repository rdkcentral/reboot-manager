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

#include "mock_reboot_utils.h"

MockRebootUtils* g_mockRebootUtils = nullptr;

#ifdef GTEST_ENABLE

extern "C" {
    int acquire_lock(const char *lockDir) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->acquire_lock(lockDir);
        }
        return ERROR_GENERAL;
    }
    int release_lock(const char *lockDir) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->release_lock(lockDir);
        }
        return ERROR_GENERAL;
    }

    int parse_device_properties(EnvContext *ctx) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->parse_device_properties(ctx);
        }
        return ERROR_GENERAL;
    }

    int parse_legacy_log(const char *logPath, RebootInfo *info) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->parse_legacy_log(logPath, info);
        }
        return ERROR_GENERAL;
    }

    int should_update_reboot_info(const EnvContext *ctx) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->should_update_reboot_info(ctx);
        }
        return 0;
    }
    int read_brcm_previous_reboot_reason(HardwareReason *hw) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->read_brcm_previous_reboot_reason(hw);
        }
        return ERROR_GENERAL;
    }

    int detect_kernel_panic(const EnvContext *ctx, PanicInfo *panicInfo) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->detect_kernel_panic(ctx, panicInfo);
        }
        return ERROR_GENERAL;
    }
    int check_firmware_failure(const EnvContext *ctx, FirmwareFailure *fwFailure) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->check_firmware_failure(ctx, fwFailure);
        }
        return ERROR_GENERAL;
    }

    int classify_reboot_reason(RebootInfo *info, const EnvContext *ctx, const HardwareReason *hwReason, const PanicInfo *panicInfo, const FirmwareFailure *fwFailure) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->classify_reboot_reason(info, ctx, hwReason, panicInfo, fwFailure);
        }
        return ERROR_GENERAL;
    }

    int write_reboot_info(const char *path, const RebootInfo *info) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->write_reboot_info(path, info);
        }
        return ERROR_GENERAL;
    }

    int write_hardpower(const char *path, const char *timestamp) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->write_hardpower(path, timestamp);
        }
        return ERROR_GENERAL;
    }
    int copy_keypress_info(const char *srcPath, const char *destPath) {
        if (g_mockRebootUtils) {
            return g_mockRebootUtils->copy_keypress_info(srcPath, destPath);
        }
        return ERROR_GENERAL;
    }
}

#endif // GTEST_ENABLE

