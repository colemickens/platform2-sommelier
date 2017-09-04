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

NAMESPACE_DECLARATION {
int32_t gLogLevel = 0;
int32_t gLogCcaLevel = CAMERA_DEBUG_CCA_LOG_ERROR;
int32_t gPerfLevel = 0;
int32_t gDumpType = 0;
// Skip frame number before dump. Default: 0, not skip
int32_t gDumpSkipNum = 0;
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
// Declare external functions (Depends on OS)
extern void __initOsEnviroment();

void cca_print_error(const char *fmt, va_list ap)
{
    if (gLogCcaLevel & CAMERA_DEBUG_CCA_LOG_ERROR)
        __camera_hal_log_ap(true, CAMERA_DEBUG_TYPE_ERROR, LOG_TAG_CCA, fmt, ap);
}

void cca_print_debug(const char *fmt, va_list ap)
{
    if (gLogCcaLevel & CAMERA_DEBUG_CCA_LOG_DEBUG)
        __camera_hal_log_ap(true, CAMERA_DEBUG_TYPE_DEBUG, LOG_TAG_CCA, fmt, ap);
}

void cca_print_info(const char *fmt, va_list ap)
{
    if (gLogCcaLevel & CAMERA_DEBUG_CCA_LOG_INFO)
       __camera_hal_log_ap(true, CAMERA_DEBUG_TYPE_INFO, LOG_TAG_CCA, fmt, ap);
}

void setDebugLevel(void)
{
    __initOsEnviroment();

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

} // namespace LogHelper
} NAMESPACE_DECLARATION_END
