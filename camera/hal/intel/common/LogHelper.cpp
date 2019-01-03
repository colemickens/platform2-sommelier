/*
 * Copyright (C) 2012-2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "LogHelper"
#define LOG_TAG_CCA "CCA"

#include <stdint.h>
#include <limits.h> // INT_MAX, INT_MIN
#include <stdlib.h> // atoi.h

#include "LogHelper.h"

#include "LogHelperChrome.h"

namespace cros {
namespace intel {
int32_t gLogLevel = 0;
int32_t gLogCcaLevel = CAMERA_DEBUG_CCA_LOG_ERROR;
int32_t gPerfLevel = 0;
int32_t gDumpType = 0;
// Skip frame number before dump. Default: 0, not skip
int32_t gDumpSkipNum = 0;
// dump 1 frame every gDumpInterval frames. Default: 1, there is no skip frames between frames.
int32_t gDumpInterval = 1;
// Dump frame count. Default: -1, negative value means infinity
int32_t gDumpCount = -1;
// Path for dump data. Default: CAMERA_OPERATION_FOLDER
char    gDumpPath[PATH_MAX] = CAMERA_OPERATION_FOLDER;
int32_t gEnforceDvs = 0;
int32_t gGuiLogLevel = 0;

// Dump stats grid data
int32_t gRgbsGridDump = 0;
int32_t gAfGridDump = 0;

namespace LogHelper {

__attribute__((format(printf, 1, 2)))
void cca_print_error(const char *fmt, ...)
{
    va_list arglist;
    va_start(arglist, fmt);
    if (gLogCcaLevel & CAMERA_DEBUG_CCA_LOG_ERROR) {
        LOG(ERROR) << base::StringPrintf(LOG_HEADER, "E/", CAMHAL_TAG)
                   << base::StringPrintV(fmt, arglist);
    }
    va_end(arglist);
}

__attribute__((format(printf, 1, 2)))
void cca_print_debug(const char *fmt, ...)
{
    va_list arglist;
    va_start(arglist, fmt);
    if (gLogCcaLevel & CAMERA_DEBUG_CCA_LOG_DEBUG) {
        VLOG(3) << base::StringPrintf(LOG_HEADER, "D/", CAMHAL_TAG)
                << base::StringPrintV(fmt, arglist);
    }
    va_end(arglist);
}

__attribute__((format(printf, 1, 2)))
void cca_print_info(const char *fmt, ...)
{
    va_list arglist;
    va_start(arglist, fmt);
    if (gLogCcaLevel & CAMERA_DEBUG_CCA_LOG_INFO) {
        VLOG(1) << base::StringPrintf(LOG_HEADER, "I/", CAMHAL_TAG)
                << base::StringPrintV(fmt, arglist);
    }
    va_end(arglist);
}

void setDebugLevel(void)
{
    // The camera HAL adapter handled the logging initialization already.

    if (__getEnviromentValue(ENV_CAMERA_HAL_DEBUG, &gLogLevel)) {
        LOGD("Debug level is 0x%x", gLogLevel);

        // "setprop camera.hal.debug 2" is expected
        // to enable both LOG1 and LOG2 traces
        if (gLogLevel & CAMERA_DEBUG_LOG_LEVEL2)
            gLogLevel |= CAMERA_DEBUG_LOG_LEVEL1;
    }

    if (__getEnviromentValue(ENV_CAMERA_CCA_DEBUG, &gLogCcaLevel)) {
        LOGD("CCA debug level is 0x%x", gLogCcaLevel);
    }

    //Performance property
    if (__getEnviromentValue(ENV_CAMERA_HAL_PERF, &gPerfLevel)) {
    }
    // dump property, it's used to dump images or some parameters to a file.
    if (__getEnviromentValue(ENV_CAMERA_HAL_DUMP, &gDumpType)) {
        LOGD("Dump type is 0x%x", gDumpType);

        if (gDumpType) {
            // Read options for dump.
            // skip number
            if (__getEnviromentValue(ENV_CAMERA_HAL_DUMP_SKIP_NUM, &gDumpSkipNum)) {
                LOGD("Skip %d frames before dump", gDumpSkipNum);
            }
            // set dump interval
            if (__getEnviromentValue(ENV_CAMERA_HAL_DUMP_INTERVAL, &gDumpInterval)) {
                LOGD("dump 1 frame every %d frames", gDumpInterval);
            }
            // total frame number for dump
            if (__getEnviromentValue(ENV_CAMERA_HAL_DUMP_COUNT, &gDumpCount)) {
                LOGD("Total %d frames will be dumped", gDumpCount);
            }
            // dump path
            if (__getEnviromentValue(ENV_CAMERA_HAL_DUMP_PATH, gDumpPath, sizeof(gDumpPath))) {
                LOGD("Dump path: %s", gDumpPath);
            }
        }
    }

    // Enforce DVS for debugging
    if (__getEnviromentValue(ENV_CAMERA_HAL_DVS, &gEnforceDvs)) {
        LOGD("EnforceDvs level is 0x%x", gEnforceDvs);
    }

    if (__getEnviromentValue(ENV_CAMERA_HAL_GUI_TRACE, &gGuiLogLevel)) {
        LOGD("Gui Debug level is 0x%x", gGuiLogLevel);
    }

    // Dump stats grid data
    if (__getEnviromentValue(ENV_CAMERA_RGBS_GRID_DUMP, &gRgbsGridDump)) {
        LOGD("RGBS grid dump level is 0x%x", gRgbsGridDump);
    }

    if (__getEnviromentValue(ENV_CAMERA_AF_GRID_DUMP, &gAfGridDump)) {
        LOGD("AF grid dump level is 0x%x", gAfGridDump);
    }

}

bool isDumpTypeEnable(int dumpType)
{
    return gDumpType & dumpType;
}

bool isDebugTypeEnable(int debugType)
{
    return gLogLevel & debugType;
}

bool isPerfDumpTypeEnable(int dumpType)
{
    return gPerfLevel & dumpType;
}

bool __setEnviromentValue(const char* variable, const int value)
{
    std::string env_val = std::to_string(value);

    if (!variable) {
        return false;
    }

    if (setenv(variable, env_val.c_str(), 1) != 0) {
        LOGE("setenv error for %s", variable);
        return false;
    }

    return true;
}

bool __setEnviromentValue(const char* variable, const char* value)
{
    if (!variable || !value) {
        return false;
    }

    if (setenv(variable, value, 1) != 0) {
        LOGE("setenv error for %s", variable);
        return false;
    }

    return true;
}

bool __getEnviromentValue(const char* variable, int* value)
{
    if (!variable || !value) {
        return false;
    }

    char* valueStr = getenv(variable);
    if (valueStr) {
        *value = strtoul(valueStr, nullptr, 0);
        return true;
    }
    return false;
}

bool __getEnviromentValue(const char* variable, char *value, size_t buf_size)
{
    char* valueStr = getenv(variable);

    if (valueStr) {
        MEMCPY_S(value, buf_size, valueStr, strlen(valueStr) + 1);
        value[buf_size - 1] = '\0';
        return true;
    }
    return false;
}

} // namespace LogHelper
} /* namespace intel */
} /* namespace cros */
