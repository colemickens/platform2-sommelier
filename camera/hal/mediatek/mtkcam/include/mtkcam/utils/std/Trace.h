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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_TRACE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_TRACE_H_

/******************************************************************************
 *
 ******************************************************************************/
#ifndef ATRACE_TAG
#define ATRACE_TAG ATRACE_TAG_CAMERA
#endif
//
#if 0  // to do
#define CAM_TRACE_NAME_LENGTH 32
#define CAM_TRACE_CALL() ATRACE_CALL()
#define CAM_TRACE_NAME(name) ATRACE_NAME(name)
#define CAM_TRACE_INT(name, value) ATRACE_INT(name, value)
#define CAM_TRACE_BEGIN(name) ATRACE_BEGIN(name)
#define CAM_TRACE_END() ATRACE_END()
#define CAM_TRACE_ASYNC_BEGIN(name, cookie) ATRACE_ASYNC_BEGIN(name, cookie)
#define CAM_TRACE_ASYNC_END(name, cookie) ATRACE_ASYNC_END(name, cookie)
#define CAM_TRACE_FMT_BEGIN(fmt, arg...)                \
  do {                                                  \
    if (ATRACE_ENABLED()) {                             \
      char buf[CAM_TRACE_NAME_LENGTH];                  \
      snprintf(buf, CAM_TRACE_NAME_LENGTH, fmt, ##arg); \
      CAM_TRACE_BEGIN(buf);                             \
    }                                                   \
  } while (0)
#define CAM_TRACE_FMT_END() CAM_TRACE_END()
#else
#define CAM_TRACE_CALL()
#define CAM_TRACE_NAME(name)
#define CAM_TRACE_INT(name, value)
#define CAM_TRACE_BEGIN(name)
#define CAM_TRACE_END()
#define CAM_TRACE_ASYNC_BEGIN(name, cookie)
#define CAM_TRACE_ASYNC_END(name, cookie)
#define CAM_TRACE_FMT_BEGIN(fmt, arg...)
#define CAM_TRACE_FMT_END()
#endif

/******************************************************************************
 *
 ******************************************************************************/
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_TRACE_H_
