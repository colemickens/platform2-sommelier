// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_SYSTEM_FILES_SERVICE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_SYSTEM_FILES_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>

namespace diagnostics {

class SystemFilesService {
 public:
  struct FileDump {
    FileDump();
    ~FileDump();

    // Absolute path to the file.
    base::FilePath path;
    // Canonicalized path to the file. Unlike |path|, this path never contains
    // symbolic links.
    base::FilePath canonical_path;
    std::string contents;

   private:
    DISALLOW_COPY_AND_ASSIGN(FileDump);
  };

  enum class Directory {
    kProcAcpiButton,  // request contents of files under
                      // “/proc/acpi/button/"

    kSysClassHwmon,    // request information about hwmon devices (contents of
                       // files under /sys/class/hwmon/)
    kSysClassThermal,  // request information about thermal zone devices and
                       // cooling devices (contents of files under
                       // /sys/class/thermal/)
    kSysFirmwareDmiTables,  // request SMBIOS information as raw DMI tables
                            // (contents of files under
                            // /sys/firmware/dmi/tables/)
    kSysClassPowerSupply,   // request information about power supplies
                            // (contents of files under
                            // /sys/class/power_supply/)
    kSysClassBacklight,     // request information about brightness
                            // (contents of files under /sys/class/backlight/)
    kSysClassNetwork,       // request information about WLAN and Ethernet
                            // (contents of files under /sys/class/net/)
    kSysDevicesSystemCpu,   // request information about CPU details.
                            // (contents of files under
                            // /sys/devices/system/cpu/)
  };

  enum class File {
    kProcUptime,      // request contents of "/proc/uptime"
    kProcMeminfo,     // request contents of “/proc/meminfo"
    kProcLoadavg,     // request contents of “/proc/loadavg"
    kProcStat,        // request contents of “/proc/stat"
    kProcNetNetstat,  // request contents of “/proc/net/netstat"
    kProcNetDev,      // request contents of “/proc/net/dev"
    kProcDiskstats,   // request contents of “/proc/diskstats"
    kProcCpuinfo,     // request contents of “/proc/cpuinfo"
    kProcVmstat,      // request contents of “/proc/vmstat"
  };

  SystemFilesService() = default;
  virtual ~SystemFilesService() = default;

  // Gets the dump of the specified file. Returns true if successful.
  virtual bool GetFileDump(File location, FileDump* dump) = 0;

  // Gets the dumps of the files in the specified directory.  Returns true if
  // successful.
  virtual bool GetDirectoryDump(
      Directory location, std::vector<std::unique_ptr<FileDump>>* dumps) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemFilesService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_SYSTEM_FILES_SERVICE_H_
