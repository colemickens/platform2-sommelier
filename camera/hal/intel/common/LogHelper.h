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

#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include <utils/Errors.h>
#include "CommonUtilMacros.h"

namespace cros {
namespace intel {
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
extern int32_t gDumpInterval;         //< camera.hal.dump.interval
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
    CAMERA_DEBUG_LOG_AIQ = 1 << 2,
    CAMERA_DEBUG_LOG_XML = 1 << 3,
    CAMERA_DEBUG_LOG_METADATA = 1 << 4,
    CAMERA_DEBUG_LOG_MEDIA_CONTROL = 1 << 5

    /* space reserved for new log levels */
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

void cca_print_error(const char *fmt, ...);

void cca_print_debug(const char *fmt, ...);

void cca_print_info(const char *fmt, ...);

/**
 * Runtime selection of debugging level.
 */
void setDebugLevel(void);
bool isDebugLevelEnable(int level);
bool isDumpTypeEnable(int dumpType);
bool isDebugTypeEnable(int debugType);
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

#define LOG_HEADER "%s %s:"

class ScopedTrace {
public:
inline ScopedTrace(int level, const char* name, const char* tag) :
        mLevel(level),
        mName(name),
        mTag(tag) {
    if (gLogLevel & mLevel) {
        VLOG(3) << base::StringPrintf("%s %s%s:" "ENTER-%s", "D/", CAMHAL_PREFIX, mTag, mName);
    }
}

inline ~ScopedTrace() {
    if (gLogLevel & mLevel) {
        VLOG(3) << base::StringPrintf("%s %s%s:" "EXIT-%s", "D/", CAMHAL_PREFIX, mTag, mName);
    }
}

private:
    int mLevel;
    const char* mName;
    const char* mTag;
};


} // namespace LogHelper

#define LOGE(...)                                                  \
    LOG(ERROR) << base::StringPrintf(LOG_HEADER, "E/", CAMHAL_TAG) \
               << base::StringPrintf(__VA_ARGS__)
#define LOGW(...)                                                    \
    LOG(WARNING) << base::StringPrintf(LOG_HEADER, "W/", CAMHAL_TAG) \
                 << base::StringPrintf(__VA_ARGS__)

#ifdef CAMERA_HAL_DEBUG
#define LOG1(...)                                                               \
    !(gLogLevel & CAMERA_DEBUG_LOG_LEVEL1)                                      \
        ? (void)(0) : LOG(INFO)                                                 \
                          << base::StringPrintf(LOG_HEADER, "D/L1", CAMHAL_TAG) \
                          << base::StringPrintf(__VA_ARGS__)
#define LOG2(...)                                                               \
    !(gLogLevel & CAMERA_DEBUG_LOG_LEVEL2)                                      \
        ? (void)(0) : LOG(INFO)                                                 \
                          << base::StringPrintf(LOG_HEADER, "D/L2", CAMHAL_TAG) \
                          << base::StringPrintf(__VA_ARGS__)
#define LOGAIQ(...)                                                              \
    !(gLogLevel & CAMERA_DEBUG_LOG_AIQ)                                          \
        ? (void)(0) : LOG(INFO)                                                  \
                          << base::StringPrintf(LOG_HEADER, "D/AIQ", CAMHAL_TAG) \
                          << base::StringPrintf(__VA_ARGS__)

#define LOGXML(...)                                                              \
    !(gLogLevel & CAMERA_DEBUG_LOG_XML)                                          \
        ? (void)(0) : LOG(INFO)                                                  \
                          << base::StringPrint(LOG_HEADER, "D/XML", CAMHAL_TAG) \
                          << base::StringPrint(__VA_ARGS__)

#define LOGMETA(...)                                                             \
    !(gLogLevel & CAMERA_DEBUG_LOG_METADATA)                                     \
        ? (void)(0) : LOG(INFO)                                                  \
                          << base::StringPrint(LOG_HEADER, "D/META", CAMHAL_TAG) \
                          << base::StringPrint(__VA_ARGS__)

#define LOGMC(...)                                                                 \
        !(gLogLevel & CAMERA_DEBUG_LOG_MEDIA_CONTROL)                              \
            ? (void)(0) : LOG(INFO)                                                \
                              << base::StringPrint(LOG_HEADER, "D/MC", CAMHAL_TAG) \
                              << base::StringPrint(__VA_ARGS__)

#define LOGI(...)                                               \
    VLOG(1) << base::StringPrintf(LOG_HEADER, "I/", CAMHAL_TAG) \
            << base::StringPrintf(__VA_ARGS__)
#define LOGV(...)                                               \
    VLOG(2) << base::StringPrintf(LOG_HEADER, "V/", CAMHAL_TAG) \
            << base::StringPrintf(__VA_ARGS__)
#define LOGD(...)                                               \
    VLOG(3) << base::StringPrintf(LOG_HEADER, "D/", CAMHAL_TAG) \
            << base::StringPrintf(__VA_ARGS__)

#define HAL_TRACE_CALL(level, tag) LogHelper::ScopedTrace ___tracer(level, __FUNCTION__, tag)
#else
#define LOG1(...)
#define LOG2(...)
#define LOGAIQ(...)

#define LOGD(...)
#define LOGV(...)
#define LOGI(...)

#define HAL_TRACE_CALL(level, tag)
#endif

} /* namespace intel */
} /* namespace cros */
#endif // _LOGHELPER_H_
