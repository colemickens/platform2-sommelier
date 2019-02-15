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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_GYROCOLLECTOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_GYROCOLLECTOR_H_

/**
 *  Defines MTKCAM_HAVE_GYROCOLLECTOR_SUPPORT to makes GyroCollector work, or
 *  defines it to 0 to disable.
 */
#define MTKCAM_HAVE_GYROCOLLECTOR_SUPPORT 0

/**
 *  Define this definition to force GyroCollector::GyroInfo be as 4 bytes
 * algined. In x64 machine, it's better to leave free alignment by compiler. But
 * if there's memory concern, you can force as 4 bytes alignment.
 */
#define GYROCOLLECTOR_GYROINFO_4_BYTES 0

// STL
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

#if GYROCOLLECTOR_GYROINFO_4_BYTES
#define __GYROINFO_PACK__ __attribute__((packed, aligned(4)))
#else
#define __GYROINFO_PACK__
#endif

namespace NSCam {
namespace Utils {

// GyroCollector is a class for caller to retrieve gyro information.
//  [ This class is thread-safe ]
class GyroCollector {
  //
  // Constants
  //
 public:
  // For better performance, gyro information queue is a limited size queue.
  constexpr static size_t STACK_SIZE = 50;

  // Gyro info retrieve interval, in milliseconds.
  constexpr static size_t INTERVAL = 33;  // ms

  // GyroCollector is an event drived mechanism. Which means if the module
  // is idle for a while, IDLE_TIMEOUT, we will stop listening gyro information
  // anymore. As-Is: the GyroCollector is now triggered by P1 node, which means
  // triggered every frame. The minimum interval is 66 ms, the maximum may
  // be around 100 ms. Please consider it to decide the IDLE_TIMEOUT.
  // Note: It's better to use more than 500ms due to SW overhead.
  constexpr static size_t IDLE_TIMEOUT = 1500;  // ms

  // We have to limit data collecting interval be greater than 15
  static_assert(
      GyroCollector::INTERVAL >= 15,
      "The interval of GyroCollector collecting data is supposed to be "
      "greater than 15, or it may have performance issue");

  //
  // Types
  //
 public:
  // GyroInfo is a trivial copyable structure contains gyro information
  struct __GYROINFO_PACK__ GyroInfo {
    float x;
    float y;
    float z;
    int64_t timestamp;  // nanosecond, including deep sleep duration
    GyroInfo() noexcept : x(0), y(0), z(0), timestamp(0) {}
    ~GyroInfo() = default;
  };

  static_assert(std::is_trivially_copyable<GyroInfo>::value,
                "GyroCollector::GyroInfo is not a trivial copyable structure, "
                "please makes it as a trivial copyable struct for the better "
                "performance");

  // GyroInfoContainer is a fixed size container, where means there's always
  // a memory chunk be created with size STACK_SIZE * sizeof(GyroInfo).
  // Caller can invoke GyroInfoContainer::size to get the available size of
  // gyro info in this container.
  struct GyroInfoContainer {
    // methods
    inline size_t size() const { return _size; }
    inline void setSize(size_t s) { _size = s; }
    inline void clear() {
      _data.clear();
      _data.resize(STACK_SIZE);
      _size = 0;
    }
    inline GyroInfo* data() { return _data.data(); }
    inline const GyroInfo* data() const { return _data.data(); }
    inline size_t dataSize() const { return _size * sizeof(GyroInfo); }
    // operator
    inline GyroInfo& operator[](size_t i) { return _data[i]; }
    inline const GyroInfo& operator[](size_t i) const { return _data[i]; }
    // constructor
    GyroInfoContainer() noexcept {
      _size = 0;
      _data.resize(STACK_SIZE);
    }
    ~GyroInfoContainer() = default;
    // attributes
   private:
    std::vector<GyroInfo> _data;
    size_t _size;
  };

  //
  // Constructor, destructor are forbidden
  //
 public:
  GyroCollector() = delete;
  ~GyroCollector() = delete;

  //
  // Method
  //
 public:
  // GyroCollector has to be triggered at least once during IDLE_TIMEOUT, or
  // it will stop listening gyro information anymore. Once it stopped, all
  // data in GyroCollector will be cleared.
  static void trigger();

  // Get the GyroInfo in range [ts_start, ts_end].
  //  @param ts_start                 The timestamp in the unit ns, excluding
  //                                  deep sleep duration.
  //  @param ts_end                   The timestamp in the unit ns, excluding
  //                                  deep sleep duration.
  //  @return                         A container contains matched items.
  //  @note                           The item is sorted by timestamp ascending.
  static GyroInfoContainer getData(int64_t ts_start = 0,
                                   int64_t ts_end = INT64_MAX);
};      // class GyroCollector
};      // namespace Utils
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_GYROCOLLECTOR_H_
