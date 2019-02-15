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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_POSTREDZONE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_POSTREDZONE_H_

#include <cstddef>
#include <cstdint>
#include <sys/mman.h>

namespace NSCam {
namespace NSCamFeature {

class PostRedZone {
 private:
  // can query via "getconf PAGE_SIZE" or sysconf()
  static constexpr intptr_t __PAGE_SIZE = 4096;

 public:
  static void* mynew(std::size_t count) {
    intptr_t __DATA_PAGES =
        (sizeof(std::size_t) + sizeof(char*) + count + __PAGE_SIZE - 1) /
        __PAGE_SIZE;
    char* spaceAddr = new char[__PAGE_SIZE * (__DATA_PAGES + 2)];
    intptr_t redZone = reinterpret_cast<intptr_t>(spaceAddr) +
                       (__PAGE_SIZE * (__DATA_PAGES + 1));
    redZone = (redZone & ~(__PAGE_SIZE - 1));
    mprotect(reinterpret_cast<void*>(redZone), __PAGE_SIZE, PROT_READ);
    intptr_t objAddr = redZone - count;
    *(reinterpret_cast<char**>(objAddr - sizeof(char*))) = spaceAddr;
    *(reinterpret_cast<std::size_t*>(objAddr - sizeof(char*) -
                                     sizeof(std::size_t))) = count;
    return reinterpret_cast<void*>(objAddr);
  }

  static void mydelete(void* objAddrP) noexcept {
    intptr_t objAddr = reinterpret_cast<intptr_t>(objAddrP);
    std::size_t count = *(reinterpret_cast<std::size_t*>(
        objAddr - sizeof(char*) - sizeof(std::size_t)));
    intptr_t redZone = objAddr + count;
    mprotect(reinterpret_cast<void*>(redZone), __PAGE_SIZE,
             PROT_READ | PROT_WRITE);
    char* spaceAddr = *(reinterpret_cast<char**>(objAddr - sizeof(char*)));
    delete[] spaceAddr;
  }
};

}  // namespace NSCamFeature
}  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_POSTREDZONE_H_
