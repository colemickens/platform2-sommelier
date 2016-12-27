// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/dircrypto-data-migrator/migration-helper.h"

namespace cryptohome {
namespace dircrypto_data_migrator {

MigrationHelper::MigrationHelper(Platform* platform,
                                 const base::FilePath& status_files_dir)
    : platform_(platform), status_files_dir_(status_files_dir) {}

MigrationHelper::~MigrationHelper() {}

bool MigrationHelper::Migrate(const base::FilePath& from,
                              const base::FilePath& to,
                              const uint64_t chunk_size) {
  return true;
}

}  // namespace dircrypto_data_migrator
}  // namespace cryptohome
