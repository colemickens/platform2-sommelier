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

#include "../include/DebugUtil.h"
#include <property_service/property_lib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../include/DebugControl.h"
#define PIPE_TRACE TRACE_DEBUG_UTIL
#define PIPE_CLASS_TAG "DebugUtil"
#include "../include/PipeLog.h"

#define DEFAULT_PROPERTY_VALUE 0

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

MINT32 getPropertyValue(const char* key) {
  return getPropertyValue(key, DEFAULT_PROPERTY_VALUE);
}

MINT32 getPropertyValue(const char* key, MINT32 defVal) {
  TRACE_FUNC_ENTER();
  MINT32 value = defVal;
  if (key && *key) {
    value = property_get_int32(key, defVal);
    if (value != defVal) {
      MY_LOGD("getPropertyValue %s=%d", key, value);
    }
  }
  TRACE_FUNC_EXIT();
  return value;
}

MINT32 getFormattedPropertyValue(const char* fmt, ...) {
  TRACE_FUNC_ENTER();
  const int maxLen = PROPERTY_KEY_MAX * 2;
  char key[PROPERTY_KEY_MAX * 2];
  va_list args;
  int keyLen;
  MINT32 value = DEFAULT_PROPERTY_VALUE;

  if (fmt && *fmt) {
    va_start(args, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    keyLen = vsnprintf(key, sizeof(key), fmt, args);
#pragma clang diagnostic pop
    va_end(args);

    if (keyLen >= maxLen) {
      MY_LOGE("Property key[%s...] length exceed %d char. Can not get prop!",
              key, maxLen);
    }
    if (keyLen > 0 && keyLen < maxLen) {
      value = getPropertyValue(key, DEFAULT_PROPERTY_VALUE);
    }
  }

  TRACE_FUNC_EXIT();
  return value;
}
static bool do_mkdir(char const* const path, MUINT32 const mode) {
  struct stat st;
  //
  if (0 != ::stat(path, &st)) {
    //  Directory does not exist.
    if (0 != ::mkdir(path, mode) && EEXIST != errno) {
      return false;
    }
  } else if (!S_ISDIR(st.st_mode)) {
    return false;
  }
  //
  return true;
}
//
bool makePath(char const* const path, MUINT32 const mode) {
  bool ret = true;
  char* copypath = strdup(path);
  char* pp = copypath;
  char* sp;
  while (ret && 0 != (sp = strchr(pp, '/'))) {
    if (sp != pp) {
      *sp = '\0';
      ret = do_mkdir(copypath, mode);
      *sp = '/';
    }
    pp = sp + 1;
  }
  if (ret) {
    ret = do_mkdir(path, mode);
  }
  free(copypath);
  return ret;
}
};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam
