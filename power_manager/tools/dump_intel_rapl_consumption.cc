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
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "power_manager/common/util.h"

namespace {

struct {
  const char* node;
  const char* name;
} const kPowercapDomains[] = {
    {"/sys/class/powercap/intel-rapl:0/energy_uj", "pkg"},
    {"/sys/class/powercap/intel-rapl:0:0/energy_uj", "pp0"},
    {"/sys/class/powercap/intel-rapl:0:1/energy_uj", "gfx"},
    {"/sys/class/powercap/intel-rapl:0:2/energy_uj", "dram"},
};
const size_t kNumPowercapDomains = arraysize(kPowercapDomains);

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

  base::FilePath energy_file_path[kNumPowercapDomains];
  for (int i = 0; i < kNumPowercapDomains; ++i) {
    energy_file_path[i] = base::FilePath(kPowercapDomains[i].node);
    PCHECK(base::PathExists(energy_file_path[i]))
        << "Error opening " << energy_file_path[i].value();
  }

  for (const auto& domain : kPowercapDomains)
    printf("%10s ", domain.name);
  printf(" (Note: 'pkg' includes 'pp0' and 'gfx'. Values in Watts)\n");

  uint64_t energy_before[kNumPowercapDomains] = {};
  uint64_t energy_after[kNumPowercapDomains] = {};
  do {
    for (int i = 0; i < kNumPowercapDomains; ++i)
      power_manager::util::ReadUint64File(energy_file_path[i], &energy_before[i]);
    const base::TimeTicks ticks_before = base::TimeTicks::Now();

    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(FLAGS_interval_ms));

    for (int i = 0; i < kNumPowercapDomains; ++i)
      power_manager::util::ReadUint64File(energy_file_path[i], &energy_after[i]);
    const base::TimeDelta time_delta = base::TimeTicks::Now() - ticks_before;

    for (int i = 0; i < kNumPowercapDomains; ++i) {
      printf("%10.6f ", (energy_after[i] - energy_before[i]) /
                            (time_delta.InSecondsF() * 1e6));
    }
    printf("\n");
  } while (FLAGS_repeat);

  return 0;
}
