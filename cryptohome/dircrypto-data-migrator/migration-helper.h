// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_DIRCRYPTO_DATA_MIGRATOR_MIGRATION_HELPER_H_
#define CRYPTOHOME_DIRCRYPTO_DATA_MIGRATOR_MIGRATION_HELPER_H_

#include <base/files/file_path.h>
#include <base/macros.h>

#include "cryptohome/platform.h"

namespace cryptohome {
namespace dircrypto_data_migrator {

// A helper class for migrating files to new file system with small overhead of
// diskspace.
class MigrationHelper {
 public:
  MigrationHelper(Platform* platform, const base::FilePath& status_files_dir);
  virtual ~MigrationHelper();

  // Moves all files under |from| into |to|.
  //
  // This function splits each file into chunks that size is |chunk_size|, then
  // copy chunks into new file system one by one.
  //
  // Parameters
  //   from - Where to move files from
  //   to - Where to move files into
  //   chunk_size - The size of a chunk
  bool Migrate(const base::FilePath& from,
               const base::FilePath& to,
               const uint64_t chunk_size);

 private:
  Platform* platform_;
  const base::FilePath status_files_dir_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MigrationHelper);
};

}  // namespace dircrypto_data_migrator
}  // namespace cryptohome

#endif  // CRYPTOHOME_DIRCRYPTO_DATA_MIGRATOR_MIGRATION_HELPER_H_
