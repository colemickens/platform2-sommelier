// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_HELPERS_SCHEDULER_CONFIGURATION_UTILS_H_
#define DEBUGD_SRC_HELPERS_SCHEDULER_CONFIGURATION_UTILS_H_

#include <map>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/gtest_prod_util.h>
#include <base/macros.h>

namespace debugd {

// Class to provide functionality to the CPU control profiles in /sys.
//
// Functions are gathered into this class in order to provide a testable
// interface.
class SchedulerConfigurationUtils {
 public:
  // |base_path| is normally /sys but can be adjusted for testing.
  explicit SchedulerConfigurationUtils(const base::FilePath& base_path)
      : base_path_{base_path}, fd_map_{}, offline_cpus_{}, online_cpus_{} {}

  ~SchedulerConfigurationUtils() = default;

  // This enables all cores.
  bool EnablePerformanceConfiguration();

  // This disables virtual cores.
  bool EnableConservativeConfiguration();

  // Store a map of all the CPU control files. The CPU number is mapped to its
  // file descriptor. This also stores the vector of offline and offline CPUs,
  // to avoid it being re-calculated later.
  bool GetControlFDs();

 private:
  // This disables sibling threads of physical cores.
  bool DisableSiblings(const std::string& cpu_num);

  // Writes the online status to CPU control file fd.
  static bool WriteFlagToCPUControlFile(const base::ScopedFD& fd,
                                        const std::string& flag);

  // This takes a range of CPUs from the /sys filesystem, which could be a raw
  // number, comma separated list, or hyphen separated range, and converts it
  // into a vector. Note: the kernel will in fact return lists such as 0,2-3;
  // however, if that happens, something went wrong. Rather than support such
  // complicated logic, this checks for it and errors out.
  static bool ParseCPUNumbers(const std::string& cpus,
                              std::vector<std::string>* result);

  // This fetches the FD from the map, makes sure it exists, and then writes it.
  bool LookupFDAndWriteFlag(const std::string& cpu_number,
                            const std::string& flag);

  // This writes the flag to disable the given CPU by number.
  bool DisableCPU(const std::string& cpu_number);

  // This writes the flag to enable the given CPU by number.
  bool EnableCPU(const std::string& cpu_number);

  // This reads either the offline or online CPU list and opens FDs for the
  // listed CPUs, putting them into |fd_map_|.
  bool GetFDsFromControlFile(const base::FilePath& path,
                             std::vector<std::string>* cpu_nums);

  // Returns a the path to the sibling thread file for the purpose of unit
  // testing.
  base::FilePath GetSiblingPath(const std::string& cpu_num);

  // The base FilePath, adjustable for testing.
  base::FilePath base_path_;
  // A map of |cpu_num| to |fd|.
  std::map<std::string, base::ScopedFD> fd_map_;
  // A vector of offline CPUs.
  std::vector<std::string> offline_cpus_;
  // A vector of online CPUs.
  std::vector<std::string> online_cpus_;

  FRIEND_TEST_ALL_PREFIXES(SchedulerConfigurationHelperTest, WriteFlag);
  FRIEND_TEST_ALL_PREFIXES(SchedulerConfigurationHelperTest, ParseCPUs);
  FRIEND_TEST_ALL_PREFIXES(SchedulerConfigurationHelperTest, TestSchedulers);

  DISALLOW_COPY_AND_ASSIGN(SchedulerConfigurationUtils);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_HELPERS_SCHEDULER_CONFIGURATION_UTILS_H_
