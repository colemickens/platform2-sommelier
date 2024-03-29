// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_SYSTEM_FILES_SERVICE_IMPL_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_SYSTEM_FILES_SERVICE_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>

#include "diagnostics/wilco_dtc_supportd/telemetry/system_files_service.h"

namespace diagnostics {

class SystemFilesServiceImpl final : public SystemFilesService {
 public:
  static base::FilePath GetPathForDirectory(Directory location);
  static base::FilePath GetPathForFile(File location);

  SystemFilesServiceImpl();
  ~SystemFilesServiceImpl() override;

  // SystemFilesService overrides:
  bool GetFileDump(File location, FileDump* dump) override;
  bool GetDirectoryDump(Directory location,
                        std::vector<std::unique_ptr<FileDump>>* dumps) override;

  void set_root_dir_for_testing(const base::FilePath& dir);

 private:
  // Makes a dump of the specified file. Returns whether the dumping succeeded.
  bool MakeFileDump(const base::FilePath& path, FileDump* dump) const;

  // Constructs and, if successful, appends the dump of every file in the
  // specified directory (with the path given relative to |root_dir|) to the
  // given vector. This will follow allowable symlinks - see
  // ShouldFollowSymlink() for details.
  void SearchDirectory(
      const base::FilePath& root_dir,
      std::set<std::string>* visited_paths,
      std::vector<std::unique_ptr<FileDump>>* file_dumps) const;

  // While dumping files in a directory, determines if we should follow a
  // symlink or not. Currently, we only follow symlinks one level down from
  // /sys/class/*/. For example, we would follow a symlink from
  // /sys/class/hwmon/hwmon0, but we would not follow a symlink from
  // /sys/class/hwmon/hwmon0/device.
  bool ShouldFollowSymlink(const base::FilePath& link) const;

  base::FilePath root_dir_{"/"};

  DISALLOW_COPY_AND_ASSIGN(SystemFilesServiceImpl);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_SYSTEM_FILES_SERVICE_IMPL_H_
