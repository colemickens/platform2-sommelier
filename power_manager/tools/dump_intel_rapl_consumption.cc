// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This utility reports power consumption for certain Intel SoCs, calculated by
// averaging the running energy consumption counter provided by Linux powercap
// driver subset of Intel RAPL (Running Average Power Limit) energy report.
// RAPL provides info per Power Domain: DRAM and PKG. PKG refers to the
// processor die, and includes the PP0 (cores) and PP1 (graphics) subdomains.
//
// MSRs reference can be found in "Sec. 14.9 Platform Specific Power Management
// Support" of the "Intel 64 and IA-32 Architectures Software Developer’s
// Manual Volume 3B: System Programming Guide, Part 2" [1].
// Info of Linux powercap driver can be reached in kernel documentation [2].
//
// [1] https://www.intel.com/content/www/us/en/architecture-and-technology/64-ia-32-architectures-software-developer-vol-3b-part-2-manual.html
// [2] https://github.com/torvalds/linux/blob/master/Documentation/power/powercap/powercap.rst

#include <math.h>

#include <base/cpu.h>
#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/sys_info.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "power_manager/common/util.h"

namespace {

// Path to the powercap driver sysfs interface, if it doesn't exist,
// either the kernel is old w/o powercap driver, or it is not configured.
constexpr const char kPowercapPath[] = "/sys/class/powercap";

struct {
  const char* node;
  const char* name;
} const kPowercapDomains[] = {
    {"intel-rapl:0", "pkg"},
    {"intel-rapl:0:0", "pp0"},
    {"intel-rapl:0:1", "gfx"},
    {"intel-rapl:0:2", "dram"},
    {"intel-rapl:1", "psys"},
};
const size_t kMaxPowercapDomains = arraysize(kPowercapDomains);

}  // namespace

int main(int argc, char** argv) {
  DEFINE_int32(interval_ms, 1000, "Interval to collect consumption (ms).");
  DEFINE_bool(repeat, false, "Repeat forever.");
  DEFINE_bool(verbose, false, "Verbose logging.");
  brillo::FlagHelper::Init(
      argc, argv, "Print average power consumption per domain for Intel SoCs");

  brillo::InitLog(brillo::kLogToStderr);

  const base::CPU cpu;
  CHECK_EQ("GenuineIntel", cpu.vendor_name()) << "Only GenuineIntel supported";

  // Kernel v3.13+ supports powercap, it also requires a proper configuration
  // enabling it; leave a verbose footprint of the kernel string, and examine
  // whether or not the system supports the powercap driver.
  if (FLAGS_verbose) {
    const base::SysInfo sys;
    printf("OS version: %s\n", sys.OperatingSystemVersion().c_str());
  }
  base::FilePath powercap_file_path(kPowercapPath);
  PCHECK(base::PathExists(powercap_file_path))
      << "No powercap driver sysfs interface, couldn't find "
      << powercap_file_path.value();

  std::vector<std::pair<base::FilePath, std::string>> power_domains;
  for (int i = 0; i < kMaxPowercapDomains; ++i) {
    base::FilePath energy_file_path(base::StringPrintf("%s/%s/energy_uj",
        kPowercapPath, kPowercapDomains[i].node));

    if (!base::PathExists(energy_file_path))
      continue;

    if (FLAGS_verbose)
      printf("Found RAPL domain %s\n", kPowercapDomains[i].name);
    power_domains.push_back({energy_file_path, kPowercapDomains[i].name});
  }

  for (const auto& domain : power_domains)
    printf("%10s ", domain.second.c_str());
  printf(" (Note: 'pkg' includes 'pp0' and 'gfx'. Values in Watts)\n");

  uint32_t num_domains = power_domains.size();
  uint64_t energy_before[kMaxPowercapDomains] = {};
  uint64_t energy_after[kMaxPowercapDomains] = {};
  do {
    for (int i = 0; i < num_domains; ++i)
      power_manager::util::ReadUint64File(power_domains[i].first,
                                          &energy_before[i]);
    const base::TimeTicks ticks_before = base::TimeTicks::Now();

    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(FLAGS_interval_ms));

    for (int i = 0; i < num_domains; ++i)
      power_manager::util::ReadUint64File(power_domains[i].first,
                                          &energy_after[i]);
    const base::TimeDelta time_delta = base::TimeTicks::Now() - ticks_before;

    for (int i = 0; i < num_domains; ++i) {
      printf("%10.6f ", (energy_after[i] - energy_before[i]) /
                            (time_delta.InSecondsF() * 1e6));
    }
    printf("\n");
  } while (FLAGS_repeat);

  return 0;
}
