/*
 * Copyright (C) 2016-2017 Intel Corporation
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

#include <algorithm>
#include <iomanip>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <string.h>
#include <base/logging.h>
#include "LogHelper.h"
#include "LogHelperChrome.h"

NAMESPACE_DECLARATION {

namespace LogHelper {

static int gLogOutType = 1; // output to sys by default.
                            // zero: output to stdout

static void getLogTime(char *timeBuf, int bufLen)
{
    // The format of time is: 01-22 15:24:53.071
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t nowtime = tv.tv_sec;
    struct tm* nowtm = localtime(&nowtime);
    if (nowtm) { // If nowtm is nullptr, simply print nothing for time info
        char tmbuf[bufLen];
        tmbuf[0] = '\0';
        strftime(tmbuf, bufLen, "%m-%d %H:%M:%S", nowtm);
        std::ostringstream ssmsec;
        ssmsec << std::setfill('0') << std::setw(3) << tv.tv_usec/1000;
        std::ostringstream stringStream;
        stringStream << tmbuf << "." << ssmsec.str();
        std::string str = stringStream.str();
        const char *tmp = str.c_str();
        int srcLen = str.length();
        MEMCPY_S(timeBuf, bufLen, tmp, srcLen);
        size_t nullPos = std::min(bufLen - 1, srcLen);
        timeBuf[nullPos] = '\0';
    }
}

static void printLogToStd(const char *module, const char *level, const char *fmt, va_list ap)
{
    // Add time into beginning of the log.
    const int BUF_LEN = 64;
    char timeBuf[BUF_LEN] = {'\0'};

    getLogTime(timeBuf, BUF_LEN);

    fprintf(stdout, "%s [%s] %s:", timeBuf, level, module);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
}

static void printLogToSys(const char *module, int priority, const char *level, const char *fmt, va_list ap)
{
    va_list args_copy;
    va_copy(args_copy, ap);

    const auto sz = std::vsnprintf( nullptr, 0, fmt, ap ) + 1 ;

    std::string outStr(sz, ' ');
    std::vsnprintf(&outStr.front(), sz, fmt, args_copy);

    switch (priority) {
        case logging::LOG_ERROR:
            LOG(ERROR) << level << " " << module << ":" << outStr;
        break;
        case logging::LOG_WARNING:
            LOG(WARNING) << level << " " << module << ":" << outStr;
        break;
        case logging::LOG_INFO:
            LOG(INFO) << level << " " << module << ":" << outStr;
        break;
        case logging::LOG_VERBOSE:
            VLOG(1) << level << " " << module << ":" << outStr;
        break;
        case logging::LOG_VERBOSE - 1:
            VLOG(2) << level << " " << module << ":" << outStr;
        break;
        case logging::LOG_VERBOSE - 2:
            VLOG(3) << level << " " << module << ":" << outStr;
        break;
        default:
            VLOG(1) << level << " " << module << ":" << outStr;
            break;
    }
}

void __camera_hal_log(bool condition, int prio, const char *tag,
                      const char *fmt, ...)
{
    if (!condition) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    __camera_hal_log_ap(condition, prio, tag, fmt, ap);
    va_end(ap);
}

void __camera_hal_log_ap(bool condition, int prio, const char *tag,
                      const char *fmt, va_list ap)
{
    if (!condition) {
        return;
    }

    const char *levelStr = nullptr;

    int syslogPrio = logging::LOG_INFO;
    switch(prio) {
        // The first five priorities (LOG_LEVEL1 to LOG_XML) are controlled by
        // the cameraDebug environment variable.
        case CAMERA_DEBUG_LOG_LEVEL1:
            levelStr = "D/L1";
        break;
        case CAMERA_DEBUG_LOG_LEVEL2:
            levelStr = "D/L2";
        break;
        case CAMERA_DEBUG_LOG_REQ_STATE:
            levelStr = "D/REQ";
        break;
        case CAMERA_DEBUG_LOG_AIQ:
            levelStr = "D/AIQ";
        break;
        case CAMERA_DEBUG_LOG_XML:
            levelStr = "D/XML";
        break;
        case CAMERA_DEBUG_TYPE_ERROR:
            levelStr = "E/";
            syslogPrio = logging::LOG_ERROR;
        break;
        case CAMERA_DEBUG_TYPE_WARN:
            levelStr = "W/";
            syslogPrio = logging::LOG_WARNING;
        break;
        case CAMERA_DEBUG_TYPE_INFO:
            levelStr = "I/";
            // VLOG(1)
            syslogPrio = logging::LOG_VERBOSE;
        break;
        case CAMERA_DEBUG_TYPE_VERBOSE:
            levelStr = "V/";
            // VLOG(2)
            syslogPrio = logging::LOG_VERBOSE - 1;
        break;
        case CAMERA_DEBUG_TYPE_DEBUG:
            levelStr = "D/";
            // VLOG(3)
            syslogPrio = logging::LOG_VERBOSE - 2;
        break;
        default:
            levelStr = "UNKNOWN/";
            syslogPrio = logging::LOG_VERBOSE - 2;
        break;
    }

    if (gLogOutType)
        printLogToSys(tag, syslogPrio, levelStr, fmt, ap);
    else
        printLogToStd(tag, levelStr, fmt, ap);

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

void __initOsEnviroment()
{
    const char* CAMERA_LOG_OUTPUT = ENV_CAMERA_HAL_DEBUG;
    __getEnviromentValue(CAMERA_LOG_OUTPUT, &gLogOutType);
}

} // namespace LogHelper

} NAMESPACE_DECLARATION_END
