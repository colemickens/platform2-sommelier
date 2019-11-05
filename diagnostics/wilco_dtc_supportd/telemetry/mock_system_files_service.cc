// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/telemetry/mock_system_files_service.h"

namespace diagnostics {

bool MockSystemFilesService::GetFileDump(File location, FileDump* dump) {
  DCHECK(dump);

  if (!GetFileDumpImpl(location))
    return false;

  DCHECK(file_dump_);
  dump->contents = file_dump_->contents;
  dump->path = file_dump_->path;
  dump->canonical_path = file_dump_->canonical_path;

  return true;
}

bool MockSystemFilesService::GetDirectoryDump(
    Directory location, std::vector<std::unique_ptr<FileDump>>* dumps) {
  DCHECK(dumps);

  if (!GetDirectoryDumpImpl(location))
    return false;

  for (const auto& file_dump : directory_dump_) {
    auto dump = std::make_unique<SystemFilesService::FileDump>();

    dump->contents = file_dump->contents;
    dump->path = file_dump->path;
    dump->canonical_path = file_dump->canonical_path;

    dumps->push_back(std::move(dump));
  }

  return true;
}

void MockSystemFilesService::set_file_dump(
    const std::unique_ptr<SystemFilesService::FileDump>& file_dump) {
  file_dump_ = std::make_unique<SystemFilesService::FileDump>();

  file_dump_->contents = file_dump->contents;
  file_dump_->path = file_dump->path;
  file_dump_->canonical_path = file_dump->canonical_path;
}

void MockSystemFilesService::set_directory_dump(
    const std::vector<std::unique_ptr<SystemFilesService::FileDump>>&
        directory_dump) {
  directory_dump_.clear();

  for (const auto& file_dump : directory_dump) {
    auto dump = std::make_unique<SystemFilesService::FileDump>();

    dump->contents = file_dump->contents;
    dump->path = file_dump->path;
    dump->canonical_path = file_dump->canonical_path;

    directory_dump_.push_back(std::move(dump));
  }
}

}  // namespace diagnostics
