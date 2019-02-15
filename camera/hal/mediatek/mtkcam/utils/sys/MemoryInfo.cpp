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

#define LOG_TAG "MemInfo"

#include <functional>
#include <memory>

#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/sys/MemoryInfo.h>

#include <regex>
#include <stdio.h>

namespace NSCam {
namespace NSMemoryInfo {

int64_t getFreeMemorySize() {
  /* Usually, we cat /proc/meminfo and sum up these three columns, which can
   * represents the "available" memory that application can use.
   * 1. MemFree
   * 2. Buffers
   * 3. Cached
   */
  size_t size = 256 * sizeof(char);
  char chunk[256];

  std::unique_ptr<FILE, std::function<void(FILE*)> > fp(
      ::fopen("/proc/meminfo", "r"), [](FILE* p) { ::fclose(p); });

  /* open fd failed */
  if (fp.get() == nullptr) {
    return -1;
  }

  /* read every line */
  int memFreeVal = -1;
  int buffersVal = -1;
  int cachedVal = -1;
  char* line = chunk;

  while (::getline(&line, &size, fp.get()) >= 0) {
    if (::strncmp(line, "MemFree", 7) == 0) {
      ::sscanf(line, "%*s%d", &memFreeVal);
    } else if (::strncmp(line, "Buffers", 7) == 0) {
      ::sscanf(line, "%*s%d", &buffersVal);
    } else if (::strncmp(line, "Cached", 6) == 0) {
      ::sscanf(line, "%*s%d", &cachedVal);
    }
  }

  fp = nullptr;  // close fp

  if (memFreeVal >= 0 && buffersVal >= 0 && cachedVal >= 0) {
    int64_t val = memFreeVal + buffersVal + cachedVal;  // unit: kB
    return val *= 1024LL;                               // convert unit to Bytes
  }

  return -1;
}

};  // namespace NSMemoryInfo
};  // namespace NSCam
