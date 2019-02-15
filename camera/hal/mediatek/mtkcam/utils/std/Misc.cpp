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

#define LOG_TAG "MtkCam/Utils"
//
#include "MyUtils.h"
//
#include <errno.h>
#include <fcntl.h>
#include <mutex>
#include <property_service/property.h>
#include <property_service/property_lib.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/resource.h>

/******************************************************************************
 *
 ******************************************************************************/

namespace NSCam {
namespace Utils {
/******************************************************************************
 *
 ******************************************************************************/

/******************************************************************************
 * @brief dump call stack
 ******************************************************************************/
void dumpCallStack(char const* prefix) {}

/******************************************************************************
 *
 ******************************************************************************/
static bool do_mkdir(char const* const path, unsigned int const mode) {
  struct stat st;
  //
  if (0 != ::stat(path, &st)) {
    //  Directory does not exist.
    if (0 != ::mkdir(path, mode) && EEXIST != errno) {
      MY_LOGE("fail to mkdir [%s]: %d[%s]", path, errno, ::strerror(errno));
      return false;
    }
  } else if (!S_ISDIR(st.st_mode)) {
    MY_LOGE("!S_ISDIR");
    return false;
  }
  //
  return true;
}

/******************************************************************************
 * @brief make all directories in path.
 *
 * @details
 * @note
 *
 * @param[in] path: a specified path to create.
 * @param[in] mode: the argument specifies the permissions to use, like 0777
 *                 (the same to that in mkdir).
 *
 * @return
 * -    true indicates success
 * -    false indicates failure
 *****************************************************************************/
bool makePath(char const* const path, unsigned int const mode) {
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

/******************************************************************************
 * @brief save the buffer to the file
 *
 * @details
 *
 * @note
 *
 * @param[in] fname: The file name
 * @param[in] buf: The buffer to save
 * @param[in] size: The size in bytes to save
 *
 * @return
 * -   true indicates success
 * -   false indicates failure
 ******************************************************************************/
bool saveBufToFile(char const* const fname,
                   unsigned char* const buf,
                   unsigned int const size) {
  int nw, cnt = 0;
  uint32_t written = 0;

  MY_LOGD("opening file [%s]", fname);
  int fd = ::open(fname, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
  if (fd < 0) {
    MY_LOGE("failed to create file [%s]: %s", fname, ::strerror(errno));
    return false;
  }

  MY_LOGD("writing %d bytes to file [%s]", size, fname);
  while (written < size) {
    nw = ::write(fd, buf + written, size - written);
    if (nw < 0) {
      MY_LOGE("failed to write to file [%s]: %s", fname, ::strerror(errno));
      break;
    }
    written += nw;
    cnt++;
  }
  MY_LOGD("done writing %d bytes to file [%s] in %d passes", size, fname, cnt);
  ::close(fd);
  return true;
}

/******************************************************************************
 * @brief load the file to the buffer
 *
 * @details
 *
 * @note
 *
 * @param[in] fname: The file name
 * @param[out] buf: The output buffer
 * @param[in] capacity: The buffer capacity in bytes
 *
 * @return
 * -   The loaded size in bytes.
 ******************************************************************************/
unsigned int loadFileToBuf(char const* const fname,
                           unsigned char* const buf,
                           unsigned int const capacity) {
  int nr, cnt = 0;
  uint32_t readCnt = 0;
  unsigned int size = capacity;

  MY_LOGD("opening file [%s]", fname);
  int fd = ::open(fname, O_RDONLY);
  if (fd < 0) {
    MY_LOGE("failed to create file [%s]: %s", fname, strerror(errno));
    return readCnt;
  }
  //
  if (size == 0) {
    size = ::lseek(fd, 0, SEEK_END);
    ::lseek(fd, 0, SEEK_SET);
  }
  //
  MY_LOGD("read %d bytes from file [%s]", size, fname);
  while (readCnt < size) {
    nr = ::read(fd, buf + readCnt, size - readCnt);
    if (nr < 0) {
      MY_LOGE("failed to read from file [%s]: %s", fname, strerror(errno));
      break;
    }
    readCnt += nr;
    cnt++;
  }
  MY_LOGD("done reading %d bytes to file [%s] in %d passes", size, fname, cnt);
  ::close(fd);

  return readCnt;
}

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace Utils
};  // namespace NSCam

static int32_t determinePersistLogLevel() {
  int32_t level =
      ::property_get_int32("persist.vendor.mtk.camera.log_level", -1);
  MY_LOGD("###### get camera log property =%d", level);
  if (-1 == level) {
    level = 3;  // MTKCAM_LOG_LEVEL_DEFAULT;
  }
  return level;
}

__BEGIN_DECLS
static int32_t gLogLevel = determinePersistLogLevel();
int mtkcam_testLog(char const* /*tag*/, int prio) {
  switch (prio) {
    case 'V':
      return (gLogLevel >= 4);
    case 'D':
      return (gLogLevel >= 3);
    case 'I':
      return (gLogLevel >= 2);
    case 'W':
      return (gLogLevel >= 1);
    case 'E':
      return (1);
    default:
      break;
  }
  return 0;
}

void setLogLevelToEngLoad(bool is_camera_on_off_timing,
                          bool set_log_level_to_eng,
                          int logCount) {
#if (((1 == MTKCAM_USER_DEBUG_LOAD) && (0 == MTKCAM_USER_DBG_LOG_OFF)) || \
     (1 == MTKCAM_ENG_LOAD))
  char value[16];
  if (is_camera_on_off_timing) {
    int32_t mtk_internal =
        ::property_get_int32("ro.vendor.mtklog_internal", -1);
    if (set_log_level_to_eng) {
      if (logCount == -1) {
        snprintf(value, sizeof(value), "%d", MTKCAM_ANDROID_LOG_MUCH_COUNT);
      } else {
        snprintf(value, sizeof(value), "%d", logCount);
      }
      if (mtk_internal == 1) {
        property_set("vendor.logmuch.value", value);
        MY_LOGI("###### set log level to %s", value);
      } else {
        MY_LOGI(
            "[enter camera]not mtk_internal_project (%d), no need to change "
            "log level",
            mtk_internal);
      }
    } else {
      if (mtk_internal == 1) {
        property_set("vendor.logmuch.value", "0");
        MY_LOGI("###### set log level to default");
      } else {
        MY_LOGI(
            "[exit camera]not mtk_internal_project (%d), no need to change log "
            "level",
            mtk_internal);
      }
    }
  }
#endif
}

__END_DECLS
