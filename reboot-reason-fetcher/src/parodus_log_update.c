/*
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:

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

#include "update-reboot-info.h"
#include "rdk_logger.h"
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#define KERNEL_REASON_LOG "/opt/logs/receiver.log"
#define COPY_BUFFER_SIZE 4096

static void get_timestamp_string(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%y%m%d-%H:%M:%S", tm_info);
}

int append_kernel_reason(const EnvContext *ctx, const RebootInfo *info)
{
    FILE *fp = NULL;
    char timestamp[MAX_TIMESTAMP_LENGTH];
    char kernel_reset_reason[MAX_REASON_LENGTH];
    if (!ctx || !info) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for append_kernel_reason\n");
        return ERROR_GENERAL;
    }
    if (strcmp(ctx->soc, "RTK") != 0 && strcmp(ctx->soc, "REALTEK") != 0) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Not RTK/REALTEK platform, skipping kernel reason append \n");
        return SUCCESS;
    }
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Appending kernel reason for RTK/REALTEK platform \n");
    strncpy(kernel_reset_reason, info->reason, sizeof(kernel_reset_reason) - 1);
    kernel_reset_reason[sizeof(kernel_reset_reason) - 1] = '\0';
    for (char *p = kernel_reset_reason; *p; p++) {
        *p = tolower(*p);
    }
    get_timestamp_string(timestamp, sizeof(timestamp));
    fp = fopen(KERNEL_REASON_LOG, "a");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open kernel reason log %s: %s\n", KERNEL_REASON_LOG, strerror(errno));
        return ERROR_GENERAL;
    }
    fprintf(fp, "%s: Kernel reboot reason: %s\n", timestamp, kernel_reset_reason);
    fflush(fp);
    fclose(fp);
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Kernel reason appended to %s: %s\n", KERNEL_REASON_LOG, kernel_reset_reason);
    return SUCCESS;
}

int update_parodus_log(const RebootInfo *info)
{
    FILE *fp = NULL;
    char timestamp[MAX_TIMESTAMP_LENGTH];
    char line[MAX_BUFFER_SIZE];
    bool logVal = false;
    if (!info) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for update_parodus_log \n");
        return ERROR_GENERAL;
    }
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Updating Parodus log \n");
    fp = fopen(PARODUS_LOG, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "PreviousRebootInfo")) {
                logVal = true;
                RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","PreviousRebootInfo already exists in Parodus log \n");
                break;
            }
        }
        fclose(fp);
    }
    if (logVal) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Parodus log already contains reboot info, skipping update \n");
        return SUCCESS;
    }
    get_timestamp_string(timestamp, sizeof(timestamp));
    fp = fopen(PARODUS_LOG, "a");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open Parodus log %s: %s\n", PARODUS_LOG, strerror(errno));
        return ERROR_GENERAL;
    }
    fprintf(fp, "%s: %s: Updating previous reboot info to Parodus\n", timestamp, "update_previous_reboot_info");
    fprintf(fp, "%s: %s: PreviousRebootInfo:%s,%s,%s,%s\n",
            timestamp,
            "update_previous_reboot_info",
            info->timestamp,
            info->reason,
            info->customReason,
            info->source);
    fflush(fp);
    fclose(fp);
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Parodus log updated with reboot info:\n");
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO",
            "{\n"
            " \"timestamp\":\"%s\",\n"
            " \"source\":\"%s\",\n"
            " \"reason\":\"%s\",\n"
            " \"customReason\":\"%s\",\n"
            " \"otherReason\":\"%s\"\n"
            "}\n",
            info->timestamp,
            info->source,
            info->reason,
            info->customReason,
            info->otherReason);
    return SUCCESS;
}

int handle_parodus_reboot_file(const RebootInfo *info, const char *destPath)
{
    if (!info || !destPath) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for handle_parodus_reboot_file\n");
        return ERROR_GENERAL;
    }

    if (access(PARODUS_REBOOT_INFO_FILE, R_OK) == 0) {
        FILE *in = fopen(PARODUS_REBOOT_INFO_FILE, "r");
        if (!in) {
            RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open %s: %s\n", PARODUS_REBOOT_INFO_FILE, strerror(errno));
        } else {
            char buf[MAX_BUFFER_SIZE];
            size_t n = fread(buf, 1, sizeof(buf) - 1, in);
            buf[n] = '\0';
            fclose(in);

            FILE *out = fopen(destPath, "w");
            if (!out) {
                RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open %s: %s\n", destPath, strerror(errno));
                return ERROR_GENERAL;
            }
            fputs(buf, out);
            if (buf[0] != '\0' && buf[strlen(buf) - 1] != '\n') {
                fputc('\n', out);
            }
            fflush(out);
            fclose(out);

            char ts[MAX_TIMESTAMP_LENGTH];
            get_timestamp_string(ts, sizeof(ts));
            FILE *logfp = fopen(PARODUS_LOG, "a");
            if (logfp) {
                fprintf(logfp, "%s: %s: Updating previous reboot info to Parodus\n", ts, "update_previous_reboot_info");
                fprintf(logfp, "%s: %s: %s\n", ts, "update_previous_reboot_info", buf);
                fflush(logfp);
                fclose(logfp);
            } else {
                RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open Parodus log %s: %s\n", PARODUS_LOG, strerror(errno));
            }

            (void)unlink(PARODUS_REBOOT_INFO_FILE);
            return SUCCESS;
        }
    }
    FILE *out = fopen(destPath, "w");
    if (!out) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open %s: %s\n", destPath, strerror(errno));
        return ERROR_GENERAL;
    }
    fprintf(out, "PreviousRebootInfo:%s,%s,%s,%s\n",
            info->timestamp,
            info->reason,
            info->customReason,
            info->source);
    fflush(out);
    fclose(out);
    (void)unlink(PARODUS_REBOOT_INFO_FILE);

    return SUCCESS;
}

int copy_keypress_info(const char *srcPath, const char *destPath)
{
    int src_fd = -1, dest_fd = -1;
    char buffer[COPY_BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    int ret = SUCCESS;
    if (!srcPath || !destPath) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for copy_keypress_info \n");
        return ERROR_GENERAL;
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Copying keypress info from %s to %s\n", srcPath, destPath);
    if (access(srcPath, F_OK) != 0) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Source keypress file does not exist: %s (not an error)\n", srcPath);
        return SUCCESS;
    }
    src_fd = open(srcPath, O_RDONLY);
    if (src_fd < 0) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open source file %s: %s\n", srcPath, strerror(errno));
        return SUCCESS;
    }
    dest_fd = open(destPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd < 0) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open destination file %s: %s\n", destPath, strerror(errno));
        close(src_fd);
        return ERROR_GENERAL;
    }
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to write to %s: %s\n", destPath, strerror(errno));
            ret = ERROR_GENERAL;
            break;
        }
    }
    if (bytes_read < 0) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to read from %s: %s\n", srcPath, strerror(errno));
        ret = ERROR_GENERAL;
    }
    close(src_fd);
    if (dest_fd >= 0) {
        fsync(dest_fd);
        close(dest_fd);
    }
    if (ret == SUCCESS) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Keypress info copied successfully\n");
    }
    return ret;
}
