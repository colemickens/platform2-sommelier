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

#include <base/logging.h>
#include <mtkcam/utils/log_service/platform_log.h>
#include <property_service/property_lib.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

static int platform_log_level = -1;

int determine_platform_log_level(void) {
  int level = ::property_get_int32("persist.mtk.camera.log_level", -1);
  if (level == -1) {
    return 1;  // default level to E.
  }

  return level;
}

int test_log_level(int prio) {
  switch (prio) {
    case 'V':
      return (platform_log_level >= 4);
    case 'D':
      return (platform_log_level >= 3);
    case 'I':
      return (platform_log_level >= 2);
    case 'W':
      return (platform_log_level >= 1);
    case 'E':
      return (1);
    default:
      break;
  }
  return 0;
}

int platform_log_print(int prio, const char* tag, const char* fmt, ...) {
  char buf[256];
  va_list tail;

  if (platform_log_level == -1) {
    platform_log_level = determine_platform_log_level();
  }

  if (test_log_level(prio)) {
    va_start(tail, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    vsnprintf(buf, sizeof(buf), fmt, tail);
#pragma clang diagnostic pop
    va_end(tail);

    switch (prio) {
      case 'V':
        VLOG(2) << tag << buf;
        break;
      case 'D':
        VLOG(3) << tag << buf;
        break;
      case 'I':
        VLOG(1) << tag << buf;
        break;
      case 'W':
        LOG(WARNING) << tag << buf;
        break;
      case 'E':
        LOG(ERROR) << tag << buf;
        break;
      default:
        VLOG(3) << tag << buf;
        break;
    }
  }
  return 0;
}

#ifdef __cplusplus
}
#endif
