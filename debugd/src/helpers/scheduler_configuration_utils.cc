// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/helpers/scheduler_configuration_utils.h"

#include <fcntl.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>

namespace debugd {

namespace {

constexpr char kLineTerminator = 0xa;
constexpr char kCPUSubpath[] = "devices/system/cpu";
constexpr char kCPUOfflineSubath[] = "devices/system/cpu/offline";
constexpr char kCPUOnlineSubpath[] = "devices/system/cpu/online";
constexpr char kDisableCPUFlag[] = "0";
constexpr char kEnableCPUFlag[] = "1";

}  // namespace

// static
bool SchedulerConfigurationUtils::WriteFlagToCPUControlFile(
    const base::ScopedFD& fd, const std::string& flag) {
  // WriteFileDescriptor returns true iff |size| bytes of |data| were written to
  // |fd|.
  return base::WriteFileDescriptor(fd.get(), flag.c_str(), flag.size());
}

// static
bool SchedulerConfigurationUtils::ParseCPUNumbers(
    const std::string& cpus, std::vector<std::string>* result) {
  DCHECK(result);
  std::vector<std::string> tokens = base::SplitString(
      cpus, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.size() < 1)
    return false;

  for (const auto& token : tokens) {
    // If it's a number, push immediately to the list.
    unsigned unused;
    if (base::StringToUint(token, &unused)) {
      result->push_back(token);
      continue;
    }

    // Otherwise it must be a hyphen separated range.
    std::vector<std::string> range_tokens = base::SplitString(
        token, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (range_tokens.size() != 2) {
      return false;
    }

    unsigned cpu_start, cpu_end;
    if (!base::StringToUint(range_tokens[0], &cpu_start) ||
        !base::StringToUint(range_tokens[1], &cpu_end)) {
      return false;
    }

    if (cpu_end <= cpu_start)
      return false;

    for (unsigned i = cpu_start; i <= cpu_end; i++) {
      result->push_back(base::UintToString(i));
    }
  }

  return true;
}

bool SchedulerConfigurationUtils::LookupFDAndWriteFlag(
    const std::string& cpu_number, const std::string& flag) {
  auto fd = fd_map_.find(cpu_number);
  if (fd == fd_map_.end()) {
    LOG(ERROR) << "Failed to find CPU control file for CPU: " << cpu_number;
    return false;
  }

  bool result = WriteFlagToCPUControlFile(fd->second, flag);
  if (!result)
    PLOG(ERROR) << "write";
  return result;
}

bool SchedulerConfigurationUtils::DisableCPU(const std::string& cpu_number) {
  return LookupFDAndWriteFlag(cpu_number, kDisableCPUFlag);
}

// This writes the flag to enable the given CPU by number.
bool SchedulerConfigurationUtils::EnableCPU(const std::string& cpu_number) {
  return LookupFDAndWriteFlag(cpu_number, kEnableCPUFlag);
}

// This enables all cores.
bool SchedulerConfigurationUtils::EnablePerformanceConfiguration() {
  std::string failed_cpus;
  bool result = true;
  for (const auto& cpu : offline_cpus_) {
    if (!EnableCPU(cpu)) {
      failed_cpus = failed_cpus + " " + cpu;
      result = false;
    }
  }

  if (!result)
    LOG(ERROR) << "Failed to enable CPU(s): " << failed_cpus;

  return result;
}

base::FilePath SchedulerConfigurationUtils::GetSiblingPath(
    const std::string& cpu_num) {
  return base_path_.Append(kCPUSubpath)
      .Append("cpu" + cpu_num)
      .Append("topology")
      .Append("thread_siblings_list");
}

// This disables sibling threads of physical cores.
bool SchedulerConfigurationUtils::DisableSiblings(const std::string& cpu_num) {
  base::FilePath path = GetSiblingPath(cpu_num);

  std::string siblings_list;
  if (!base::ReadFileToString(path, &siblings_list)) {
    PLOG(ERROR) << "Failed to read sibling thread list from: " << path.value();
    return false;
  }

  std::vector<std::string> sibling_nums;
  if (!ParseCPUNumbers(siblings_list, &sibling_nums)) {
    LOG(ERROR) << "Unknown range: " << siblings_list;
    return false;
  }

  // The physical core is the first number in the range.
  if (cpu_num != sibling_nums[0])
    return DisableCPU(cpu_num);

  return true;
}

bool SchedulerConfigurationUtils::EnableConservativeConfiguration() {
  bool status = true;
  for (const auto& cpu_num : online_cpus_) {
    if (!DisableSiblings(cpu_num)) {
      status = false;
      LOG(ERROR) << "Failed to disable CPU: " << cpu_num;
    }
  }

  return status;
}

bool SchedulerConfigurationUtils::GetFDsFromControlFile(
    const base::FilePath& path, std::vector<std::string>* cpu_nums) {
  DCHECK(cpu_nums);

  std::string cpus_str;
  if (!base::ReadFileToString(path, &cpus_str)) {
    PLOG(ERROR) << "Failed to read CPU list";
    return false;
  }

  // The kernel returns 0xa if the file is empty.
  if (cpus_str == std::string(1, kLineTerminator))
    return true;

  if (!ParseCPUNumbers(cpus_str, cpu_nums)) {
    LOG(ERROR) << "Unknown range: " << cpus_str;
    return false;
  }

  for (const auto& cpu_num : *cpu_nums) {
    // There is no control file for cpu0, which cannot be turned off.
    if (cpu_num == "0")
      continue;

    base::FilePath cpu_path =
        base_path_.Append(kCPUSubpath).Append("cpu" + cpu_num).Append("online");
    base::ScopedFD cpu_fd(
        HANDLE_EINTR(open(cpu_path.value().c_str(), O_RDWR | O_CLOEXEC)));
    if (cpu_fd.get() < 0) {
      PLOG(ERROR) << "Failed to open: " << cpu_path.value();
      return false;
    }
    if (fd_map_.insert(std::make_pair(cpu_num, std::move(cpu_fd))).second ==
        false) {
      LOG(ERROR) << "Duplicate control file.";
      return false;
    }
  }
  return true;
}

bool SchedulerConfigurationUtils::GetControlFDs() {
  return GetFDsFromControlFile(base_path_.Append(kCPUOnlineSubpath),
                               &online_cpus_) &&
         GetFDsFromControlFile(base_path_.Append(kCPUOfflineSubath),
                               &offline_cpus_);
}

}  // namespace debugd
