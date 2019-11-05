// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_MOCK_SYSTEM_FILES_SERVICE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_MOCK_SYSTEM_FILES_SERVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "diagnostics/wilco_dtc_supportd/telemetry/system_files_service.h"

namespace diagnostics {

class MockSystemFilesService : public SystemFilesService {
 public:
  // MockSystemFilesService creates a default file dump and array of file dumps
  MockSystemFilesService() = default;
  ~MockSystemFilesService() override = default;

  // SystemFilesService overrides:
  bool GetFileDump(File location, SystemFilesService::FileDump* dump) override;
  bool GetDirectoryDump(Directory location,
                        std::vector<std::unique_ptr<FileDump>>* dumps) override;

  void set_file_dump(
      const std::unique_ptr<SystemFilesService::FileDump>& file_dump);

  void set_directory_dump(
      const std::vector<std::unique_ptr<SystemFilesService::FileDump>>&
          directory_dump);

  MOCK_METHOD(bool, GetFileDumpImpl, (SystemFilesService::File location), ());

  MOCK_METHOD(bool,
              GetDirectoryDumpImpl,
              (SystemFilesService::Directory location),
              ());

 private:
  std::unique_ptr<SystemFilesService::FileDump> file_dump_;
  std::vector<std::unique_ptr<SystemFilesService::FileDump>> directory_dump_;

  DISALLOW_COPY_AND_ASSIGN(MockSystemFilesService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_MOCK_SYSTEM_FILES_SERVICE_H_
