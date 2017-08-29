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

#ifndef _LOGHELPER_H_
#define _LOGHELPER_H_

#include <inttypes.h> // For PRId64 used to print int64_t for both 64bits and 32bits
#include <cutils/compiler.h>
#include <limits.h>
#include <stdarg.h>

#include <utils/Errors.h>
#include "CommonUtilMacros.h"

NAMESPACE_DECLARATION {
/**
 * global log level
 * This global variable is set from system properties
 * It is used to control the level of verbosity of the traces in logcat
 * It is also used to store the status of certain RD features
 */
extern int32_t gLogLevel;    //< camera.hal.debug
extern int32_t gLogCcaLevel; //< camera.hal.cca
extern int32_t gPerfLevel;   //< camera.hal.perf
extern int32_t gEnforceDvs;  //< camera.hal.dvs

extern int32_t gDumpSkipNum;         //< camera.hal.dump.skip_num
extern int32_t gDumpCount;           //< camera.hal.dump.count
extern char    gDumpPath[PATH_MAX];  //< camera.hal.dump.path

// RGBS is one of the analog component video standards.
// composite sync, where the horizontal and vertical signals are mixed together on a separate wire (the S in RGBS)
extern int32_t gRgbsGridDump;
extern int32_t gAfGridDump;

/**
 * Debug LOG levels, set/clean bit to turn on/off
 *
 * LEVEL 1 is used to track events in the HAL that are relevant during
 * the operation of the camera, but are not happening on a per frame basis.
 * this ensures that the level of logging is not too verbose
 *
 * LEVEL 2 is used to track information on a per request basis
 *
 * REQ_STATE is used to track the state of each request. By state we mean a one
 * of the following request properties:
 *  - metadata result
 *  - buffer
 *  - shutter
 *  - error
 *
 * PERF TRACES enable only traces that provide performance metrics on the opera
 * tion of the HAL
 *
 * PERF TRACES BREAKDOWN provides further level of detail on the performance
 * metrics
 *
 * camera.hal.debug
 */
enum  {
    /* verbosity level of general traces */
    CAMERA_DEBUG_LOG_LEVEL1 = 1,
    CAMERA_DEBUG_LOG_LEVEL2 = 1 << 1,

    /* Bitmask to enable a concrete set of traces */
    CAMERA_DEBUG_LOG_REQ_STATE = 1 << 2,
    CAMERA_DEBUG_LOG_AIQ = 1 << 3,
    CAMERA_DEBUG_LOG_XML = 1 << 4,
    CAMERA_DEBUG_LOG_KERNEL_TOGGLE = 1 << 8,

    /* space reserved for new log levels */

    /* Make logs persistent, retrying if logcat is busy */
    CAMERA_DEBUG_LOG_PERSISTENT = 1 << 12, /* 4096 */

    /*
     * Reserved for log types, to support different OS
     * Don't change the orders of existing items
     */
    CAMERA_DEBUG_TYPE_START = 1 << 30,
    CAMERA_DEBUG_TYPE_DEBUG = CAMERA_DEBUG_TYPE_START,
    CAMERA_DEBUG_TYPE_ERROR,
    CAMERA_DEBUG_TYPE_WARN,
    CAMERA_DEBUG_TYPE_VERBOSE,
    CAMERA_DEBUG_TYPE_INFO,
    CAMERA_DEBUG_TYPE_END
};

// All dump related definition
// camera.hal.dump
enum {
// Dump image related flags
    CAMERA_DUMP_PREVIEW =         1 << 0, // 1
    CAMERA_DUMP_VIDEO =           1 << 1, // 2
    CAMERA_DUMP_SNAPSHOT =        1 << 2, // 4
    CAMERA_DUMP_JPEG =            1 << 3, // 8
    CAMERA_DUMP_RAW =             1 << 4, // 16
    CAMERA_DUMP_RAW_WITHOUT_MKN = (CAMERA_DUMP_RAW | (1 << 7)), // 128 + 16

// Dump parameter related flags
    CAMERA_DUMP_ISP_PARAM =       1 << 5, // 32
    CAMERA_DUMP_DVS2 =            1 << 6, // 64
    CAMERA_DUMP_MEDIA_CTL =       1 << 8, // 256

// Dump for simulation
    CAMERA_DUMP_MIPI =            1 << 9,  // 0x0200, MIPI data
    CAMERA_DUMP_V420 =            1 << 10, // 0x0400, preGDC output
    CAMERA_DUMP_PG =              1 << 11, // 0x0800, PG dump assisted by "libiacss"
    CAMERA_DUMP_AIQ_STAT =        1 << 12, // 0x01000, rgbs_grid format stats for AIQ use
};

// camera.hal.perf
enum  {
    /* Emit well-formed performance traces */
    CAMERA_DEBUG_LOG_PERF_TRACES = 1,

    /* Print out detailed timing analysis */
    CAMERA_DEBUG_LOG_PERF_TRACES_BREAKDOWN = 2,

    /* Print out detailed timing analysis for IOCTL */
    CAMERA_DEBUG_LOG_PERF_IOCTL_BREAKDOWN = 1<<2,

    /* Print out detailed memory information analysis for IOCTL */
    CAMERA_DEBUG_LOG_PERF_MEMORY = 1<<3,

    /*set camera atrace level for pytimechart*/
    CAMERA_DEBUG_LOG_ATRACE_LEVEL = 1<<4,

    /*enable media topology dump*/
    CAMERA_DEBUG_LOG_MEDIA_TOPO_LEVEL = 1<<5,

    /*enable media controller info dump*/
    CAMERA_DEBUG_LOG_MEDIA_CONTROLLER_LEVEL = 1<<6
};

// camera.hal.cca
enum  {
    CAMERA_DEBUG_CCA_LOG_ERROR = 1 << 0, // 1
    CAMERA_DEBUG_CCA_LOG_DEBUG = 1 << 1, // 2
    CAMERA_DEBUG_CCA_LOG_INFO =  1 << 2 // 4
};

// enfore same prefix for all CameraHAL log tags
#define CAMHAL_PREFIX "CAMHAL_"
#define CAMHAL_TAG CAMHAL_PREFIX LOG_TAG

// Dedicated namespace section for function declarations
namespace LogHelper {

void __camera_hal_log(bool condition, int prio, const char *tag,
                      const char *fmt, ...);

void __camera_hal_log_ap(bool condition, int prio, const char *tag,
                      const char *fmt, va_list ap);

void cca_print_error(const char *fmt, va_list ap);

void cca_print_debug(const char *fmt, va_list ap);

void cca_print_info(const char *fmt, va_list ap);

/**
 * Runtime selection of debugging level.
 */
void setDebugLevel(void);
bool isDebugLevelEnable(int level);
bool isDumpTypeEnable(int dumpType);
bool isPerfDumpTypeEnable(int dumpType);
bool isDumpMediaTopo(void);
bool isDumpMediaInfo(void);
bool isHwWeavingEnabled(void);


/**
 * Helper for enviroment variable access
 * (Impementation depends on OS)
 */
bool __getEnviromentValue(const char* variable, int* value);
bool __getEnviromentValue(const char* variable, char *value, size_t buf_size);
bool __setEnviromentValue(const char* variable, const int value);
bool __setEnviromentValue(const char* variable, const char* value);

class ScopedTrace {
public:
inline ScopedTrace(int level, const char* name) :
        mLevel(level),
        mName(name) {
    __camera_hal_log(gLogLevel & mLevel, CAMERA_DEBUG_TYPE_DEBUG, CAMHAL_TAG, "ENTER-%s", mName);
}

inline ~ScopedTrace() {
    __camera_hal_log(gLogLevel & mLevel, CAMERA_DEBUG_TYPE_DEBUG, CAMHAL_TAG, "EXIT-%s", mName);
}

private:
    int mLevel;
    const char* mName;
};

} // namespace LogHelper

// Take the namespaced function to be used for unqualified
// lookup  in the LOG* macros below
using LogHelper::__camera_hal_log;

#define LOGE(...) __camera_hal_log(true, CAMERA_DEBUG_TYPE_ERROR, CAMHAL_TAG, __VA_ARGS__)
#define LOGW(...) __camera_hal_log(true, CAMERA_DEBUG_TYPE_WARN, CAMHAL_TAG, __VA_ARGS__)

#ifdef CAMERA_HAL_DEBUG
#define LOG1(...) __camera_hal_log(gLogLevel & CAMERA_DEBUG_LOG_LEVEL1, CAMERA_DEBUG_LOG_LEVEL1, CAMHAL_TAG, __VA_ARGS__)
#define LOG2(...) __camera_hal_log(gLogLevel & CAMERA_DEBUG_LOG_LEVEL2, CAMERA_DEBUG_LOG_LEVEL2, CAMHAL_TAG, __VA_ARGS__)
#define LOGR(...) __camera_hal_log(gLogLevel & CAMERA_DEBUG_LOG_REQ_STATE, CAMERA_DEBUG_LOG_REQ_STATE, CAMHAL_TAG, __VA_ARGS__)
#define LOGAIQ(...) __camera_hal_log(gLogLevel & CAMERA_DEBUG_LOG_AIQ, CAMERA_DEBUG_LOG_AIQ, CAMHAL_TAG, __VA_ARGS__)

#define LOGD(...) __camera_hal_log(true, CAMERA_DEBUG_TYPE_DEBUG, CAMHAL_TAG, __VA_ARGS__)
#define LOGV(...) __camera_hal_log(true, CAMERA_DEBUG_TYPE_VERBOSE, CAMHAL_TAG, __VA_ARGS__)
#define LOGI(...) __camera_hal_log(true, CAMERA_DEBUG_TYPE_INFO, CAMHAL_TAG, __VA_ARGS__)

// CAMTRACE_NAME traces the beginning and end of the current scope.  To trace
// the correct start and end times this macro should be declared first in the
// scope body.
#define HAL_TRACE_NAME(level, name) LogHelper::ScopedTrace ___tracer(level, name )
#define HAL_TRACE_CALL(level) HAL_TRACE_NAME(level, __FUNCTION__)
#define HAL_TRACE_CALL_PRETTY(level) HAL_TRACE_NAME(level, __PRETTY_FUNCTION__)
#else
#define LOG1(...)
#define LOG2(...)
#define LOGR(...)
#define LOGAIQ(...)

#define LOGD(...)
#define LOGV(...)
#define LOGI(...)

#define HAL_TRACE_NAME(level, name)
#define HAL_TRACE_CALL(level)
#define HAL_TRACE_CALL_PRETTY(level)
#endif

} NAMESPACE_DECLARATION_END
#endif // _LOGHELPER_H_
