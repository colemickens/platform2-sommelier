/*
 * Copyright (C) 2014-2017 Intel Corporation
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

#ifndef _COMMON_UTIL_MACROS_H_
#define _COMMON_UTIL_MACROS_H_

#include <algorithm>
#include <string>

/**
 * Use to check input parameter and if failed, return err_code and print error message
 */
#define VOID_VALUE
#define CheckError(condition, err_code, err_msg, args...) \
            do { \
                if (condition) { \
                    LOGE(err_msg, ##args);\
                    return err_code;\
                }\
            } while (0)

/**
 * Use to check input parameter and if failed, return err_code and print warning message,
 * this should be used for non-vital error checking.
 */
#define CheckWarning(condition, err_code, err_msg, args...) \
            do { \
                if (condition) { \
                    LOGW(err_msg, ##args);\
                    return err_code;\
                }\
            } while (0)

// macro for memcpy
#ifndef MEMCPY_S
#define MEMCPY_S(dest, dmax, src, smax) memcpy((dest), (src), std::min((size_t)(dmax), (size_t)(smax)))
#endif

#define STDCOPY(dst, src, size) std::copy((src), ((src) + (size)), (dst))

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#define STRLEN_S(x) std::char_traits<char>::length(x)

#define PROPERTY_VALUE_MAX  92

#include <execinfo.h> // backtrace()

/**
 * \macro PRINT_BACKTRACE_LINUX
 * Debug macro for printing backtraces in Linux (GCC compatible) environments
 */
#define PRINT_BACKTRACE_LINUX() \
{ \
    char **btStrings = nullptr; \
    int btSize = 0; \
    void *btArray[10]; \
\
    btSize = backtrace(btArray, 10); \
    btStrings = backtrace_symbols(btArray, btSize); \
\
    LOGE("----------------------------------------"); \
    LOGE("-------------- backtrace ---------------"); \
    LOGE("----------------------------------------"); \
    for (int i = 0; i < btSize; i++) \
        LOGE("%s\n", btStrings[i]); \
    LOGE("----------------------------------------"); \
\
    free(btStrings); \
    btStrings = nullptr; \
    btSize = 0; \
}

/*
 * For Android, from N release (7.0) onwards the folder where the HAL has
 * permissions to dump files is /data/misc/cameraserver/
 * before that was /data/misc/media/
 * This is due to the introduction of the new cameraserver process.
 * For Linux, the same folder is used.
 */
#define CAMERA_OPERATION_FOLDER "/tmp/"

/**
 * \macro UNUSED
 *  applied to parameters not used in a method in order to avoid the compiler
 *  warning
 */
#define UNUSED(x) (void)(x)

#define UNUSED1(a)                                                  (void)(a)
#define UNUSED2(a,b)                                                (void)(a),UNUSED1(b)

#define VA_NUM_ARGS_IMPL(_1,_2,_3,_4,_5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, N,...) N
#define VA_NUM_ARGS(...) VA_NUM_ARGS_IMPL(__VA_ARGS__, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8 ,7 ,6, 5, 4, 3, 2, 1)

#define ALL_UNUSED_IMPL_(nargs) UNUSED ## nargs
#define ALL_UNUSED_IMPL(nargs) ALL_UNUSED_IMPL_(nargs)

#endif /* _COMMON_UTIL_MACROS_H_ */
