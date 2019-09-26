// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This utility reports power consumption for certain Intel SoCs, calculated by
// averaging the running energy consumption counter provided by the Intel RAPL
// (Running Average Power Limit) MSRs (Model Specific Registers). RAPL provides
// info per Power Domain: DRAM and PKG. PKG refers to the processor die, and
// includes the PP0 (cores) and PP1 (graphics) subdomains.
//
// MSRs reference can be found in "Sec. 14.9 Platform Specific Power Management
// Support" of the "Intel 64 and IA-32 Architectures Software Developer’s
// Manual Volume 3B: System Programming Guide, Part 2" [1].
//
// [1] https://www.intel.com/content/www/us/en/architecture-and-technology/64-ia-32-architectures-software-developer-vol-3b-part-2-manual.html

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

#define CPU_MODEL_KABYLAKE 142

// MSR values found from "Intel 64 and IA-32 Architectures Software Developer’s
// Manual Volume 4: Model-Specifi Register", e.g.:
//
// https://software.intel.com/en-us/download/intel-64-and-ia-32-architectures-software-developers-manual-volume-4-model-specific-registers
#define MSR_RAPL_POWER_UNIT 0x00000606

#define MSR_PKG_ENERGY_STATUS 0x00000611
#define MSR_DRAM_ENERGY_STATUS 0x00000619
#define MSR_PP0_ENERGY_STATUS 0x00000639
#define MSR_PP1_ENERGY_STATUS 0x00000641

// MSR_RAPL_POWER_UNIT registers bits containing the energy units scaler.
#define MSR_ENERGY_UNIT_OFFSET 0x08
#define MSR_ENERGY_UNIT_MASK 0x1F

namespace {

// Path of the dev file containing the MSR access to the CPU #0, through which
// we read all the power and energy consumptions.
constexpr const char kCpu0MsrPath[] = "/dev/cpu/0/msr";

struct {
  int64_t register_offset;
  const char* name;
} const kRegistersAndNames[] = {
    {MSR_PKG_ENERGY_STATUS, "pkg"},
    {MSR_PP0_ENERGY_STATUS, "pp0"},
    {MSR_PP1_ENERGY_STATUS, "gfx"},
    {MSR_DRAM_ENERGY_STATUS, "dram"},
};
const size_t kNumRegisters = arraysize(kRegistersAndNames);

// Reads a uint64_t from |file| at |offset| and returns its value or crashes.
uint64_t ReadEnergyStatus(base::File* file, int64_t offset) {
  uint64_t register_value = 0;
  CHECK_EQ(sizeof(uint64_t),
           file->Read(offset, reinterpret_cast<char*>(&register_value),
                     sizeof(uint64_t)));
  return register_value &= 0xFFFFFFFF;
}

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
  // TODO(mcasas): Support older models; in principle MSR energy consumption is
  // present from Sandy Bridge onwards. https://crbug.com/768518.
  CHECK_EQ(CPU_MODEL_KABYLAKE, cpu.model()) << "Only Kaby Lake CPU supported";

  base::FilePath cpu_dev_path(kCpu0MsrPath);
  PCHECK(base::PathExists(cpu_dev_path))
      << "Seems like MSR is unsupported, couldn't find "
      << cpu_dev_path.MaybeAsASCII();

  base::File cpu_dev(cpu_dev_path,
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
  PCHECK(cpu_dev.IsValid()) << "Error opening " << cpu_dev_path.MaybeAsASCII();

  // MSR_RAPL_POWER_UNIT has the units for power and energy measurements.
  uint64_t power_unit_register = 0;
  CHECK_EQ(sizeof(uint64_t),
           cpu_dev.Read(MSR_RAPL_POWER_UNIT,
                        reinterpret_cast<char*>(&power_unit_register),
                        sizeof(uint64_t)));

  const double energy_units_scaler = pow(
      0.5, static_cast<double>((power_unit_register >> MSR_ENERGY_UNIT_OFFSET) &
                               MSR_ENERGY_UNIT_MASK));

  if (FLAGS_verbose)
    printf("Energy units = %.8fJ/unit\n", energy_units_scaler);

  for (const auto& register_and_name : kRegistersAndNames)
    printf("%10s ", register_and_name.name);
  printf(" (Note: 'pkg' includes 'pp0' and 'gfx'. Values in Watts)\n");

  uint32_t values_before[kNumRegisters] = {};
  uint32_t values_after[kNumRegisters] = {};
  do {
    for (int i = 0; i < kNumRegisters; ++i) {
      values_before[i] =
          ReadEnergyStatus(&cpu_dev, kRegistersAndNames[i].register_offset);
    }
    const base::TimeTicks ticks_before = base::TimeTicks::Now();

    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(FLAGS_interval_ms));

    for (int i = 0; i < kNumRegisters; ++i) {
      values_after[i] =
          ReadEnergyStatus(&cpu_dev, kRegistersAndNames[i].register_offset);
    }
    const base::TimeDelta time_delta = base::TimeTicks::Now() - ticks_before;

    for (int i = 0; i < kNumRegisters; ++i) {
      printf("%10.6f ", (values_after[i] - values_before[i]) *
                            energy_units_scaler / time_delta.InSecondsF());
    }
    printf("\n");
  } while (FLAGS_repeat);

  return 0;
}
