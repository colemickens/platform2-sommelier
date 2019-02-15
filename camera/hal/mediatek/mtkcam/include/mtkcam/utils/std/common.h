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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_COMMON_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_COMMON_H_
/******************************************************************************
 *
 ******************************************************************************/
//
#include "Misc.h"
#include "Format.h"
#include "Profile.h"
#include "Time.h"

#include <base/threading/thread.h>

#define gettid() base::PlatformThread::CurrentId()

#ifndef __unused
#define __unused __attribute__((__unused__))
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ROUND_UP(x, div) (((x) + (div - 1)) / div)
#define ALIGN16(x) x = (((x) + 15) & ~(15))

#define ALIGN_UPPER(x, a) (((x) + ((typeof(x))(a)-1)) & ~((typeof(x))(a)-1))
#define ALIGN_LOWER(x, a) (((x)) & ~((typeof(x))(a)-1))

// example: align 5 bits => align 32
#define ALIGNX(x, y) (((x) + ((0x01 << y) - 1)) & (~((0x01 << y) - 1)))

#define APPLY_2_ALIGN(x) ((static_cast<int>(x) >> 1) << 1)

#define ROUND_UP(x, div) (((x) + (div - 1)) / div)
#define ALIGN16(x) x = (((x) + 15) & ~(15))

/******************************************************************************
 *
 ******************************************************************************/

#define MY_LOGD1(...) MY_LOGD_IF((mLogLevel >= 1), __VA_ARGS__)
#define MY_LOGD2(...) MY_LOGD_IF(CC_UNLIKELY(mLogLevel >= 2), __VA_ARGS__)
#define MY_LOGD3(...) MY_LOGD_IF(CC_UNLIKELY(mLogLevel >= 3), __VA_ARGS__)

/******************************************************************************
 *
 ******************************************************************************/
#ifndef RETURN_NULLPTR_IF_NULLPTR
#define RETURN_NULLPTR_IF_NULLPTR(_expr_, ...) \
  do {                                         \
    if (CC_UNLIKELY((_expr_) == nullptr)) {    \
      MY_LOGE(__VA_ARGS__);                    \
      return nullptr;                          \
    }                                          \
  } while (0)
#endif

#ifndef RETURN_NULLPTR_IF_NOT_OK
#define RETURN_NULLPTR_IF_NOT_OK(_expr_, fmt, arg...)             \
  do {                                                            \
    int const err = (_expr_);                                     \
    if (CC_UNLIKELY(err != 0)) {                                  \
      MY_LOGE("err:%d(%s) - " fmt, err, ::strerror(-err), ##arg); \
      return nullptr;                                             \
    }                                                             \
  } while (0)
#endif

#ifndef RETURN_ERROR_IF_NULLPTR
#define RETURN_ERROR_IF_NULLPTR(_expr_, _err_, ...) \
  do {                                              \
    if (CC_UNLIKELY((_expr_) == nullptr)) {         \
      MY_LOGE(__VA_ARGS__);                         \
      return (_err_);                               \
    }                                               \
  } while (0)
#endif

#ifndef RETURN_ERROR_IF_NOT_OK
#define RETURN_ERROR_IF_NOT_OK(_expr_, fmt, arg...)               \
  do {                                                            \
    int const err = (_expr_);                                     \
    if (CC_UNLIKELY(err != 0)) {                                  \
      MY_LOGE("err:%d(%s) - " fmt, err, ::strerror(-err), ##arg); \
      return err;                                                 \
    }                                                             \
  } while (0)
#endif

#define RETURN_IF_ERROR(_expr_, fmt, arg...)                      \
  do {                                                            \
    int const err = (_expr_);                                     \
    if (CC_UNLIKELY(err != 0)) {                                  \
      MY_LOGE("err:%d(%s) - " fmt, err, ::strerror(-err), ##arg); \
      return err;                                                 \
    }                                                             \
  } while (0)

//
/******************************************************************************
 *
 ******************************************************************************/
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_COMMON_H_
