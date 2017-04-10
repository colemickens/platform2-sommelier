// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_DIRCRYPTO_DATA_MIGRATOR_MIGRATION_HELPER_H_
#define CRYPTOHOME_DIRCRYPTO_DATA_MIGRATOR_MIGRATION_HELPER_H_

#include <string>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <chromeos/dbus/service_constants.h>

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
  // Callback for monitoring migration progress.  The first parameter is the
  // number of bytes migrated so far, and the second parameter is the total
  // number of bytes that need to be migrated, including what has already been
  // migrated.  If status is not DIRCRYPTO_MIGRATION_IN_PROGRESS the values in
  // migrated should be ignored as they are undefined.
  using ProgressCallback = base::Callback<void(
      DircryptoMigrationStatus status, uint64_t migrated, uint64_t total)>;

  // Creates a new MigrationHelper.  Status files will be stored in
  // |status_files_dir|, which should not be in the directory tree to be
  // migrated.  |max_chunk_size| is treated as a hint for the desired size of
  // data to transfer at once, but may be reduced if thee is not enough free
  // space on disk or the provided max_chunk_size is inefficient.
  MigrationHelper(Platform* platform,
                  const base::FilePath& status_files_dir,
                  uint64_t max_chunk_size);
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
  // space overhead.  This method should only ever be called once in the
  // lifetime of the object.
  //
  // Parameters
  //   from - Where to move files from.  Must be an absolute path.
  //   to - Where to move files into.  Must be an absolute path.
  //   progress_callback - function that will be called regularly to update on
  //   the progress of the migration.  Callback will be executed from the same
  //   thread as the migration, so long-running callbacks may block the
  //   migration.  May not be null.
  bool Migrate(const base::FilePath& from,
               const base::FilePath& to,
               const ProgressCallback& progress_callback);

  // Returns true if the migration has been started, but not finished.
  bool IsMigrationStarted() const;

 private:
  FRIEND_TEST(MigrationHelperTest, CopyOwnership);

  // Calculate the total number of bytes to be migrated, populating
  // |total_byte_count_| with the result.
  void CalculateDataToMigrate(const base::FilePath& from);
  // Increment the number of bytes migrated, potentially reporting the status if
  // its time for a new report.
  void IncrementMigratedBytes(uint64_t bytes);
  // Call |progress_callback_| with the number of bytes already migrated, the
  // total number of bytes to be migrated, and the migration status.
  void ReportStatus(DircryptoMigrationStatus status);
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
  ProgressCallback progress_callback_;
  const base::FilePath status_files_dir_;
  uint64_t max_chunk_size_;
  uint64_t effective_chunk_size_;
  uint64_t total_byte_count_;
  uint64_t migrated_byte_count_;
  base::TimeTicks next_report_;
  std::string namespaced_mtime_xattr_name_;
  std::string namespaced_atime_xattr_name_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MigrationHelper);
};

}  // namespace dircrypto_data_migrator
}  // namespace cryptohome

#endif  // CRYPTOHOME_DIRCRYPTO_DATA_MIGRATOR_MIGRATION_HELPER_H_
