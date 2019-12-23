/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_LOG_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_LOG_H_

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <sys/syscall.h>

#define CAMHAL_PREFIX "CAMHAL_"
#define CAMHAL_TAG CAMHAL_PREFIX LOG_TAG

#define LOG_HEADER "(%d)%s %s "

#include <mtkcam/utils/property_service/property.h>
#include <mtkcam/utils/property_service/property_lib.h>
#include <mtkcam/utils/std/common.h>
#include <mtkcam/utils/std/Header.h>
#include <mtkcam/utils/std/Profile.h>

/******************************************************************************
 *
 *  Usage:
 *      [.c/.cpp]
 *          #define LOG_TAG "<your-module-name>"
 *          #include <mtkcam/utils/std/Log.h>
 *
 *      [Android.mk]
 *          LOCAL_SHARED_LIBRARIES += libmtkcam_stdutils
 *          PS:
 *              Only needed in EXECUTABLE and SHARED LIBRARY.
 *              Not needed in STATIC LIBRARY.
 *
 *  Note:
 *      1)  Make sure to define LOG_TAG 'before' including this file.
 *      2)  LOG_TAG should follow the syntax of system property naming.
 *              Allowed:    '0'~'9', 'a'~'z', 'A'~'Z', '.', '-', or '_'
 *              Disallowed: '/'
 *      3)  In your module public API header files,
 *              Do not define LOG_TAG.
 *              Do not include this file.
 *
 ******************************************************************************/
#ifdef ALOGE
#undef ALOGE
#endif
#ifdef ALOGD
#undef ALOGD
#endif

#ifdef _COMMON_UTIL_MACROS_H_
#undef LOGE
#undef LOGW
#undef LOGI
#undef LOGV
#undef LOGD
#endif

#define LOGE(...)                                                          \
  LOG(ERROR) << base::StringPrintf(LOG_HEADER, gettid(), "E/", CAMHAL_TAG) \
             << base::StringPrintf(__VA_ARGS__)
#define LOGW(...)                                                            \
  LOG(WARNING) << base::StringPrintf(LOG_HEADER, gettid(), "W/", CAMHAL_TAG) \
               << base::StringPrintf(__VA_ARGS__)
#define LOGI(...)                                                       \
  VLOG(1) << base::StringPrintf(LOG_HEADER, gettid(), "I/", CAMHAL_TAG) \
          << base::StringPrintf(__VA_ARGS__)
#define LOGV(...)                                                       \
  VLOG(2) << base::StringPrintf(LOG_HEADER, gettid(), "V/", CAMHAL_TAG) \
          << base::StringPrintf(__VA_ARGS__)
#define LOGD(...)                                                       \
  VLOG(3) << base::StringPrintf(LOG_HEADER, gettid(), "D/", CAMHAL_TAG) \
          << base::StringPrintf(__VA_ARGS__)

#define ALOGE(...)                                                         \
  LOG(ERROR) << base::StringPrintf(LOG_HEADER, gettid(), "E/", CAMHAL_TAG) \
             << base::StringPrintf(__VA_ARGS__)
#define ALOGW(...)                                                           \
  LOG(WARNING) << base::StringPrintf(LOG_HEADER, gettid(), "W/", CAMHAL_TAG) \
               << base::StringPrintf(__VA_ARGS__)

#define ALOGI(...)                                                      \
  VLOG(1) << base::StringPrintf(LOG_HEADER, gettid(), "I/", CAMHAL_TAG) \
          << base::StringPrintf(__VA_ARGS__)
#define ALOGV(...)                                                      \
  VLOG(2) << base::StringPrintf(LOG_HEADER, gettid(), "V/", CAMHAL_TAG) \
          << base::StringPrintf(__VA_ARGS__)
#define ALOGD(...)                                                      \
  VLOG(3) << base::StringPrintf(LOG_HEADER, gettid(), "D/", CAMHAL_TAG) \
          << base::StringPrintf(__VA_ARGS__)

#define CAM_LOGE(...)                                                      \
  LOG(ERROR) << base::StringPrintf(LOG_HEADER, gettid(), "E/", CAMHAL_TAG) \
             << base::StringPrintf(__VA_ARGS__)
#define CAM_LOGW(...)                                                        \
  LOG(WARNING) << base::StringPrintf(LOG_HEADER, gettid(), "W/", CAMHAL_TAG) \
               << base::StringPrintf(__VA_ARGS__)

#define CAM_LOGI(...)                                                   \
  VLOG(1) << base::StringPrintf(LOG_HEADER, gettid(), "I/", CAMHAL_TAG) \
          << base::StringPrintf(__VA_ARGS__)
#define CAM_LOGV(...)                                                   \
  VLOG(2) << base::StringPrintf(LOG_HEADER, gettid(), "V/", CAMHAL_TAG) \
          << base::StringPrintf(__VA_ARGS__)
#define CAM_LOGD(...)                                                   \
  VLOG(3) << base::StringPrintf(LOG_HEADER, gettid(), "D/", CAMHAL_TAG) \
          << base::StringPrintf(__VA_ARGS__)

//
//  ASSERT
#define CAM_LOGA(...)                  \
  do {                                 \
    CAM_LOGE("[Assert] " __VA_ARGS__); \
    while (1) {                        \
      ::usleep(500 * 1000);            \
    }                                  \
  } while (0)
//
//
//  FATAL
#define CAM_LOGF(...)                                      \
  do {                                                     \
    CAM_LOGE("[Fatal] " __VA_ARGS__);                      \
    LOG_ALWAYS_FATAL_IF(1,                                 \
                        "(%s){#%d:%s}"                     \
                        "\r\n",                            \
                        __FUNCTION__, __LINE__, __FILE__); \
  } while (0)

#define CAM_LOGV_IF(cond, ...) \
  do {                         \
    if ((cond)) {              \
      CAM_LOGV(__VA_ARGS__);   \
    }                          \
  } while (0)
#define CAM_LOGD_IF(cond, ...) \
  do {                         \
    if ((cond)) {              \
      CAM_LOGD(__VA_ARGS__);   \
    }                          \
  } while (0)
#define CAM_LOGI_IF(cond, ...) \
  do {                         \
    if ((cond)) {              \
      CAM_LOGI(__VA_ARGS__);   \
    }                          \
  } while (0)
#define CAM_LOGW_IF(cond, ...) \
  do {                         \
    if ((cond)) {              \
      CAM_LOGW(__VA_ARGS__);   \
    }                          \
  } while (0)
#define CAM_LOGE_IF(cond, ...) \
  do {                         \
    if ((cond)) {              \
      CAM_LOGE(__VA_ARGS__);   \
    }                          \
  } while (0)
#define CAM_LOGA_IF(cond, ...) \
  do {                         \
    if ((cond)) {              \
      CAM_LOGA(__VA_ARGS__);   \
    }                          \
  } while (0)
#define CAM_LOGF_IF(cond, ...) \
  do {                         \
    if ((cond)) {              \
      CAM_LOGF(__VA_ARGS__);   \
    }                          \
  } while (0)

#define MY_LOGD_WITH_OPENID(fmt, arg...) \
  CAM_LOGD("[%s] [Cam::%d] " fmt, __FUNCTION__, mOpenId, ##arg)
#define MY_LOGI_WITH_OPENID(fmt, arg...) \
  CAM_LOGI("[%s] [Cam::%d] " fmt, __FUNCTION__, mOpenId, ##arg)
#define MY_LOGD_IF_P1(cond, ...)        \
  do {                                  \
    if ((cond)) {                       \
      MY_LOGD_WITH_OPENID(__VA_ARGS__); \
    }                                   \
  } while (0)
#define MY_LOGI_IF_P1(cond, ...)        \
  do {                                  \
    if ((cond)) {                       \
      MY_LOGI_WITH_OPENID(__VA_ARGS__); \
    }                                   \
  } while (0)
#define FUNC_START MY_LOGD("+")
#define FUNC_END MY_LOGD("-")
#define FUNC_START_PUBLIC MY_LOGD("+")
#define FUNC_END_PUBLIC MY_LOGD("-")
#define FP_STR ::NSCam::Utils::getLogStr
#define FF_LOG(v, f, ...) CAM_LOG##v(f, __FUNCTION__, ##__VA_ARGS__)
#define FPS_LOG(v, f, ...) FF_LOG(v, "[%s]" f, ##__VA_ARGS__)
#define FP_LOG(v, ...) FPS_LOG(v, "" __VA_ARGS__)
#define FP_DO(cmd) \
  do {             \
    cmd;           \
  } while (0)
#define PT_DO(lv, cmd) FP_DO(cmd)
#define XPS_LOG(v, x, f, ...) FF_LOG(v, "[%s%s]" f, x, ##__VA_ARGS__)
#define FSS_LOG(v, s, f, ...) FF_LOG(v, "[%s] %s: " f, s, ##__VA_ARGS__)
#define XSS_LOG(v, x, s, f, ...) FF_LOG(v, "[%s%s] %s: " f, x, s, ##__VA_ARGS__)
#define XP_LOG(v, x, ...) XPS_LOG(v, x, "" __VA_ARGS__)
#define FS_LOG(v, s, ...) FSS_LOG(v, FP_STR(s), "" __VA_ARGS__)
#define XS_LOG(v, x, s, ...) XSS_LOG(v, x, FP_STR(s), "" __VA_ARGS__)
#define MY_LOGV(f, arg...) FP_DO(FP_LOG(V, f, ##arg))
#define MY_LOGD(f, arg...) FP_DO(FP_LOG(D, f, ##arg))
#define MY_LOGI(f, arg...) FP_DO(FP_LOG(I, f, ##arg))
#define MY_LOGW(f, arg...) FP_DO(FP_LOG(W, f, ##arg))
#define MY_LOGE(f, arg...) FP_DO(FP_LOG(E, f, ##arg))
#define MY_LOGA(f, arg...) FP_DO(FP_LOG(A, f, ##arg))
#define MY_LOGF(f, arg...) FP_DO(FP_LOG(F, f, ##arg))
#define MY_LOGV_IF(cond, ...) \
  do {                        \
    if (cond) {               \
      MY_LOGV(__VA_ARGS__);   \
    }                         \
  } while (0)
#define MY_LOGD_IF(cond, ...) \
  do {                        \
    if (cond) {               \
      MY_LOGD(__VA_ARGS__);   \
    }                         \
  } while (0)
#define MY_LOGI_IF(cond, ...) \
  do {                        \
    if (cond) {               \
      MY_LOGI(__VA_ARGS__);   \
    }                         \
  } while (0)
#define MY_LOGW_IF(cond, ...) \
  do {                        \
    if (cond) {               \
      MY_LOGW(__VA_ARGS__);   \
    }                         \
  } while (0)
#define MY_LOGE_IF(cond, ...) \
  do {                        \
    if (cond) {               \
      MY_LOGE(__VA_ARGS__);   \
    }                         \
  } while (0)
#define MY_LOGA_IF(cond, ...) \
  do {                        \
    if (cond) {               \
      MY_LOGA(__VA_ARGS__);   \
    }                         \
  } while (0)
#define MY_LOGF_IF(cond, ...) \
  do {                        \
    if (cond) {               \
      MY_LOGF(__VA_ARGS__);   \
    }                         \
  } while (0)
#define MY_S_LOGV(s, f, arg...) FP_DO(FS_LOG(V, s, f, ##arg))
#define MY_S_LOGD(s, f, arg...) FP_DO(FS_LOG(D, s, f, ##arg))
#define MY_S_LOGI(s, f, arg...) FP_DO(FS_LOG(I, s, f, ##arg))
#define MY_S_LOGW(s, f, arg...) FP_DO(FS_LOG(W, s, f, ##arg))
#define MY_S_LOGE(s, f, arg...) FP_DO(FS_LOG(E, s, f, ##arg))
#define MY_S_LOGA(s, f, arg...) FP_DO(FS_LOG(A, s, f, ##arg))
#define MY_S_LOGF(s, f, arg...) FP_DO(FS_LOG(F, s, f, ##arg))
#define MY_S_LOGV_IF(c, s, f, arg...) FP_DO(if (c) FS_LOG(V, s, f, ##arg))
#define MY_S_LOGD_IF(c, s, f, arg...) FP_DO(if (c) FS_LOG(D, s, f, ##arg))
#define MY_S_LOGI_IF(c, s, f, arg...) FP_DO(if (c) FS_LOG(I, s, f, ##arg))
#define MY_S_LOGW_IF(c, s, f, arg...) FP_DO(if (c) FS_LOG(W, s, f, ##arg))
#define MY_S_LOGE_IF(c, s, f, arg...) FP_DO(if (c) FS_LOG(E, s, f, ##arg))
#define MY_S_LOGA_IF(c, s, f, arg...) FP_DO(if (c) FS_LOG(A, s, f, ##arg))
#define MY_S_LOGF_IF(c, s, f, arg...) FP_DO(if (c) FS_LOG(F, s, f, ##arg))
#define PIPE_BASE_LOGD(fmt, arg...) \
  CAM_LOGD("[%s][%s] " fmt, m_name.c_str(), __FUNCTION__, ##arg)
#define PIPE_BASE_LOGI(fmt, arg...) \
  CAM_LOGI("[%s][%s] " fmt, m_name.c_str(), __FUNCTION__, ##arg)
#define PIPE_BASE_LOGW(fmt, arg...) \
  CAM_LOGW("[%s][%s] " fmt, m_name.c_str(), __FUNCTION__, ##arg)
#define PIPE_BASE_LOGE(fmt, arg...) \
  CAM_LOGE("[%s][%s] " fmt, m_name.c_str(), __FUNCTION__, ##arg)

#define FPHELP_LOG(fmt, arg...) CAM_LOGD("[%s]" fmt, __func__, ##arg)
#define FPHELP_INF(fmt, arg...) CAM_LOGI("[%s]" fmt, __func__, ##arg)
#define FPHELP_WRN(fmt, arg...) \
  CAM_LOGW("[%s] WRN(%5d):" fmt, __func__, __LINE__, ##arg)
#define FPHELP_ERR(fmt, arg...) \
  CAM_LOGE("[%s] %s ERROR(%5d):" fmt, __func__, __FILE__, __LINE__, ##arg)

#define MY_TRACE(fmt, arg...) FP_DO(FP_LOG(D, fmt, ##arg))
#define TRACE_FUNC_ENTER() MY_TRACE("+")
#define TRACE_FUNC_EXIT() MY_TRACE("-")
#define TRACE_FUNC(fmt, arg...) MY_TRACE(fmt, ##arg)
#define TRACE_N_FUNC_ENTER(n) MY_TRACE("(%s)+", n)
#define TRACE_N_FUNC_EXIT(n) MY_TRACE("(%s)-", n)
#define TRACE_N_FUNC(n, f, arg...) MY_TRACE("(%s)" f, n, ##arg)
#define TRACE_S_FUNC_ENTER(s, ...) PT_DO(1, XS_LOG(D, "+", s, ##__VA_ARGS__))
#define TRACE_S_FUNC_EXIT(s, ...) PT_DO(1, XS_LOG(D, "-", s, ##__VA_ARGS__))
#define TRACE_S_FUNC(s, ...) PT_DO(1, FS_LOG(D, s, ##__VA_ARGS__))
#define TRACE_FUNC_ENTER_2(...) PT_DO(2, XP_LOG(D, "+", ##__VA_ARGS__))
#define TRACE_FUNC_EXIT_2(...) PT_DO(2, XP_LOG(D, "-", ##__VA_ARGS__))
#define TRACE_FUNC_2(...) PT_DO(2, FP_LOG(D, ##__VA_ARGS__))

#define TRACE_S_FUNC_ENTER_2(s, ...) PT_DO(2, XS_LOG(D, "+", s, ##__VA_ARGS__))
#define TRACE_S_FUNC_EXIT_2(s, ...) PT_DO(2, XS_LOG(D, "-", s, ##__VA_ARGS__))
#define TRACE_S_FUNC_2(s, ...) PT_DO(2, FS_LOG(D, s, ##__VA_ARGS__))
#define MY_LOG_FUNC_ENTER(...) FP_DO(XP_LOG(D, "+", ##__VA_ARGS__))
#define MY_LOG_FUNC_EXIT(...) FP_DO(XP_LOG(D, "-", ##__VA_ARGS__))
#define MY_LOG_S_FUNC_ENTER(s, ...) FP_DO(XS_LOG(D, "+", s, ##__VA_ARGS__))
#define MY_LOG_S_FUNC_EXIT(s, ...) FP_DO(XS_LOG(D, "-", s, ##__VA_ARGS__))

#define FUNCTION_LOG_START MY_LOGD("[%s] - E.", __FUNCTION__)
#define FUNCTION_LOG_END                            \
  do {                                              \
    if (!ret)                                       \
      MY_LOGE("[%s] fail", __FUNCTION__);           \
    MY_LOGD("[%s] - X. ret=%d", __FUNCTION__, ret); \
  } while (0)
#define FUNCTION_LOG_END_MUM MY_LOGD("[%s] - X.", __FUNCTION__)

#define MEXIF_LOGV(fmt, arg...) CAM_LOGV("[%s] " fmt, __FUNCTION__, ##arg)
#define MEXIF_LOGD(fmt, arg...) CAM_LOGD("[%s] " fmt, __FUNCTION__, ##arg)
#define MEXIF_LOGI(fmt, arg...) CAM_LOGI("[%s] " fmt, __FUNCTION__, ##arg)
#define MEXIF_LOGW(fmt, arg...) CAM_LOGW("[%s] " fmt, __FUNCTION__, ##arg)
#define MEXIF_LOGE(fmt, arg...) CAM_LOGE("[%s] " fmt, __FUNCTION__, ##arg)
#define MEXIF_LOGA(fmt, arg...) CAM_LOGA("[%s] " fmt, __FUNCTION__, ##arg)
#define MEXIF_LOGF(fmt, arg...) CAM_LOGF("[%s] " fmt, __FUNCTION__, ##arg)
//
#define MEXIF_LOGV_IF(cond, ...) \
  do {                           \
    if ((cond)) {                \
      MEXIF_LOGV(__VA_ARGS__);   \
    }                            \
  } while (0)
#define MEXIF_LOGD_IF(cond, ...) \
  do {                           \
    if ((cond)) {                \
      MEXIF_LOGD(__VA_ARGS__);   \
    }                            \
  } while (0)
#define MEXIF_LOGI_IF(cond, ...) \
  do {                           \
    if ((cond)) {                \
      MEXIF_LOGI(__VA_ARGS__);   \
    }                            \
  } while (0)
#define MEXIF_LOGW_IF(cond, ...) \
  do {                           \
    if ((cond)) {                \
      MEXIF_LOGW(__VA_ARGS__);   \
    }                            \
  } while (0)
#define MEXIF_LOGE_IF(cond, ...) \
  do {                           \
    if ((cond)) {                \
      MEXIF_LOGE(__VA_ARGS__);   \
    }                            \
  } while (0)
#define MEXIF_LOGA_IF(cond, ...) \
  do {                           \
    if ((cond)) {                \
      MEXIF_LOGA(__VA_ARGS__);   \
    }                            \
  } while (0)
#define MEXIF_LOGF_IF(cond, ...) \
  do {                           \
    if ((cond)) {                \
      MEXIF_LOGF(__VA_ARGS__);   \
    }                            \
  } while (0)
#define ASSERT_IF_NOT_EQAL(_In1, _In2) \
  CAM_LOGA_IF(_In1 != _In2, "should be %f, but is %f", (float)_In2, (float)_In1)

#define MY_LOG_ID(cond, ...) \
  do {                       \
    if (cond) {              \
      MY_LOGI(__VA_ARGS__);  \
    } else {                 \
      MY_LOGD(__VA_ARGS__);  \
    }                        \
  } while (0)

#ifndef __unused
#define __unused __attribute__((__unused__))
#endif

#define ALOG_ASSERT(cond, ...) ((void)0)  // (!(cond)? (abort();) : ((void)0))
#define LOG_ALWAYS_FATAL(...)
#define LOG_ALWAYS_FATAL_IF(...)
#define LOG_FATAL_IF(...)

#define FUNCTION_NAME MY_LOGD("");
#define FUNCTION_IN MY_LOGD("+");
#define FUNCTION_OUT MY_LOGD("-");

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_LOG_H_
