// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_DIRCRYPTO_DATA_MIGRATOR_MIGRATION_HELPER_H_
#define CRYPTOHOME_DIRCRYPTO_DATA_MIGRATOR_MIGRATION_HELPER_H_

#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>

#include "cryptohome/platform.h"

namespace cryptohome {
namespace dircrypto_data_migrator {

extern const base::FilePath::CharType kMigrationStartedFileName[];

// A helper class for migrating files to new file system with small overhead of
// diskspace.
// This class is only designed to migrate data from ecryptfs to ext4 encryption,
// and therefore makes some assumptions about the underlying file systems.  In
// particular:
//   Sparse files in the source tree are not supported.  They will be treated as
//   normal files, and therefore cause disk usage to increase after the
//   migration.
//   Support for sparse files in the destination tree are required.  If they are
//   not supported a minimum free space equal to the largest single file on disk
//   will be required for the migration.
//   The destination filesystem needs to support flushing hardware buffers on
//   fsync.  In the case of Ext4, this means not disabling the barrier mount
//   option.
class MigrationHelper {
 public:
  MigrationHelper(Platform* platform,
                  const base::FilePath& status_files_dir,
                  uint64_t chunk_size);
  virtual ~MigrationHelper();

  void set_namespaced_mtime_xattr_name_for_testing(const std::string& name) {
    namespaced_mtime_xattr_name_ = name;
  }
  void set_namespaced_atime_xattr_name_for_testing(const std::string& name) {
    namespaced_atime_xattr_name_ = name;
  }

  // Moves all files under |from| into |to|.
  //
  // This function copies chunks of a file at a time, requiring minimal free
  // space overhead.
  //
  // Parameters
  //   from - Where to move files from.  Must be an absolute path.
  //   to - Where to move files into.  Must be an absolute path.
  bool Migrate(const base::FilePath& from, const base::FilePath& to);

  // Returns true if the migration has been started, but not finished.
  bool IsMigrationStarted() const;

 private:
  // Creates a new directory that is the result of appending |child| to |to|,
  // migrating recursively all contents of the source directory.
  //
  // Parameters
  //   from - Base directory which is the root of the migration source.  Should
  //   be an absolute path.
  //   to - Base direction which is the root of the migration destination.
  //   Should be an absolute path.
  //   child - relative path under |from| and |to| to migrate.
  bool MigrateDir(const base::FilePath& from,
                  const base::FilePath& to,
                  const base::FilePath& child,
                  const FileEnumerator::FileInfo& info);
  // Creates a new link |to|/|child| which has the same attributes and target as
  // |from|/|child|.  If the target points to an absolute path under |from|, it
  // is rewritten to point to the same relative path under |to|.
  bool MigrateLink(const base::FilePath& from,
                   const base::FilePath& to,
                   const base::FilePath& child,
                   const FileEnumerator::FileInfo& info);
  bool MigrateFile(const base::FilePath& from,
                   const base::FilePath& to,
                   const FileEnumerator::FileInfo& info);
  bool CopyAttributes(const base::FilePath& from,
                      const base::FilePath& to,
                      const FileEnumerator::FileInfo& info);
  bool FixTimes(const base::FilePath& file);
  bool CopyExtendedAttributes(const base::FilePath& from,
                              const base::FilePath& to);
  bool SetExtendedAttributeIfNotPresent(const base::FilePath& file,
                                        const std::string& xattr,
                                        char* value,
                                        ssize_t size);

  Platform* platform_;
  const base::FilePath status_files_dir_;
  const uint64_t chunk_size_;
  std::string namespaced_mtime_xattr_name_;
  std::string namespaced_atime_xattr_name_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MigrationHelper);
};

}  // namespace dircrypto_data_migrator
}  // namespace cryptohome

#endif  // CRYPTOHOME_DIRCRYPTO_DATA_MIGRATOR_MIGRATION_HELPER_H_
