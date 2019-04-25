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

#ifndef CAMERA_HAL_MEDIATEK_MTKLIBV4L2_COMMONUTILMACROS_H_
#define CAMERA_HAL_MEDIATEK_MTKLIBV4L2_COMMONUTILMACROS_H_

#include <mtkcam/utils/std/common.h>
#include <mtkcam/utils/std/Log.h>

/**
 * Use to check input parameter and if failed, return err_code and print error
 * message
 */
#define VOID_VALUE
#define CheckError(condition, err_code, err_msg, args...) \
  do {                                                    \
    if (condition) {                                      \
      LOGE(err_msg, ##args);                              \
      return err_code;                                    \
    }                                                     \
  } while (0)

/**
 * Use to check input parameter and if failed, return err_code and print warning
 * message, this should be used for non-vital error checking.
 */
#define CheckWarning(condition, err_code, err_msg, args...) \
  do {                                                      \
    if (condition) {                                        \
      LOGW(err_msg, ##args);                                \
      return err_code;                                      \
    }                                                       \
  } while (0)

/**
 * Macro to calculate array size in unit of elements.
 */
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

/**
 * Inline function to clear a given type T instance.
 */
template <typename T>
inline void CLEAR(T* val) {
  ::memset(reinterpret_cast<void*>(val), 0x0, sizeof(T));
}

#endif  // CAMERA_HAL_MEDIATEK_MTKLIBV4L2_COMMONUTILMACROS_H_
