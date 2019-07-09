// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ML_UTIL_H_
#define ML_UTIL_H_

#include <base/files/file_path.h>

namespace ml {

// The memory usage (typically of a process).
// One can extend this struct to include more terms. Currently, it only
// includes `VmSwap` and `VmRSS` to fulfill the needs.
struct MemoryUsage {
  size_t VmRSSKb;
  size_t VmSwapKb;

  bool operator==(const MemoryUsage& other) const;
};

// Gets the memory usage by parsing a file (typically `/proc/[pid]/status`)
// This function assumes that the memory unit used in /proc/[pid]/status is
// "kB".
// Return true if successful, false otherwise.
bool GetProcessMemoryUsageFromFile(MemoryUsage* memory_usage,
                                   const base::FilePath& file_path);

// Same as GetProcessMemoryUsageFromFile(memory_usage, "/prod/[pid]/status")
// for the calling process's pid.
// Return true if successful, false otherwise.
bool GetProcessMemoryUsage(MemoryUsage* memory_usage);

// Gets the total memory usage for this process, which we define as VmSwap+VmRSS
// extracted from the /proc/pid/status file.
// Return true if successful, false otherwise.
bool GetTotalProcessMemoryUsage(size_t* total_memory);

}  // namespace ml

#endif  // ML_UTIL_H_
