// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/dircrypto_data_migrator/migration_helper.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/ioctl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/timer/elapsed_timer.h>
#include <chromeos/dbus/service_constants.h>

#include "cryptohome/cryptohome_metrics.h"

extern "C" {
#include <attr/xattr.h>
#include <linux/fs.h>
}

namespace cryptohome {
namespace dircrypto_data_migrator {

namespace {
constexpr char kMtimeXattrName[] = "trusted.CrosDirCryptoMigrationMtime";
constexpr char kAtimeXattrName[] = "trusted.CrosDirCryptoMigrationAtime";
// Expected maximum erasure block size on devices (4MB).
constexpr uint64_t kErasureBlockSize = 4 << 20;
// Minimum amount of free space required to begin migrating.
constexpr uint64_t kMinFreeSpace = kErasureBlockSize * 2;
// Free space required for migration overhead (FS metadata, duplicated
// in-progress directories, etc).  Must be smaller than kMinFreeSpace.
constexpr uint64_t kFreeSpaceBuffer = kErasureBlockSize;

// Sends the UMA stat for the start/end status of migration respectively in the
// constructor/destructor. By default the "generic error" end status is set, so
// to report other status, call an appropriate method to overwrite it.
class MigrationStartAndEndStatusReporter {
 public:
  explicit MigrationStartAndEndStatusReporter(bool resumed) :
      resumed_(resumed),
      end_status_(resumed ? kResumedMigrationFailedGeneric :
                  kNewMigrationFailedGeneric) {
    ReportDircryptoMigrationStartStatus(
        resumed_ ? kMigrationResumed : kMigrationStarted);
  }

  ~MigrationStartAndEndStatusReporter() {
    ReportDircryptoMigrationEndStatus(end_status_);
  }

  void SetSuccess() {
    end_status_ = resumed_ ? kResumedMigrationFinished : kNewMigrationFinished;
  }

  void SetLowDiskSpaceFailure() {
    end_status_ = resumed_ ?
        kResumedMigrationFailedLowDiskSpace : kNewMigrationFailedLowDiskSpace;
  }

  void SetFileErrorFailure(DircryptoMigrationFailedOperationType operation,
                           base::File::Error error) {
    // Some notable special cases are given distinct enum values.
    if (operation == kMigrationFailedAtOpenSourceFile &&
        error == base::File::FILE_ERROR_IO) {
      end_status_ = resumed_ ?
          kResumedMigrationFailedFileErrorOpenEIO :
          kNewMigrationFailedFileErrorOpenEIO;
    } else {
      end_status_ = resumed_ ?
          kResumedMigrationFailedFileError : kNewMigrationFailedFileError;
    }
  }

 private:
  const bool resumed_;
  DircryptoMigrationEndStatus end_status_;

  DISALLOW_COPY_AND_ASSIGN(MigrationStartAndEndStatusReporter);
};

struct PathTypeMapping {
  const base::FilePath::CharType* path;
  DircryptoMigrationFailedPathType type;
};

const PathTypeMapping kPathTypeMappings[] = {
  {FILE_PATH_LITERAL("root/android-data"), kMigrationFailedUnderAndroidOther},
  {FILE_PATH_LITERAL("user/Downloads"), kMigrationFailedUnderDownloads},
  {FILE_PATH_LITERAL("user/Cache"), kMigrationFailedUnderCache},
  {FILE_PATH_LITERAL("user/GCache"), kMigrationFailedUnderGcache},
};

}  // namespace

constexpr base::FilePath::CharType kMigrationStartedFileName[] =
    "crypto-migration.started";
// A file to store a list of files skipped during migration.  This lives in
// root/ of the destination directory so that it is encrypted.
constexpr base::FilePath::CharType kSkippedFileListFileName[] =
    "root/crypto-migration.files-skipped";
// TODO(dspaid): Determine performance impact so we can potentially increase
// frequency.
constexpr base::TimeDelta kStatusSignalInterval =
    base::TimeDelta::FromSeconds(1);

MigrationHelper::MigrationHelper(Platform* platform,
                                 const base::FilePath& from,
                                 const base::FilePath& to,
                                 const base::FilePath& status_files_dir,
                                 uint64_t max_chunk_size)
    : platform_(platform),
      from_base_path_(from),
      to_base_path_(to),
      status_files_dir_(status_files_dir),
      max_chunk_size_(max_chunk_size),
      effective_chunk_size_(0),
      total_byte_count_(0),
      migrated_byte_count_(0),
      namespaced_mtime_xattr_name_(kMtimeXattrName),
      namespaced_atime_xattr_name_(kAtimeXattrName),
      failed_operation_type_(kMigrationFailedAtOtherOperation),
      failed_path_type_(kMigrationFailedUnderOther),
      failed_error_type_(base::File::FILE_OK) {}

MigrationHelper::~MigrationHelper() {}

bool MigrationHelper::Migrate(const ProgressCallback& progress_callback) {
  base::ElapsedTimer timer;
  skipped_file_list_path_ = to_base_path_.Append(kSkippedFileListFileName);
  const bool resumed = IsMigrationStarted();
  MigrationStartAndEndStatusReporter status_reporter(resumed);

  if (progress_callback.is_null()) {
    LOG(ERROR) << "Invalid progress callback";
    return false;
  }
  progress_callback_ = progress_callback;
  ReportStatus(DIRCRYPTO_MIGRATION_INITIALIZING);
  if (!from_base_path_.IsAbsolute() || !to_base_path_.IsAbsolute()) {
    LOG(ERROR) << "Migrate must be given absolute paths";
    return false;
  }

  if (!platform_->DirectoryExists(from_base_path_)) {
    LOG(ERROR) << "Directory does not exist: " << from_base_path_.value();
    return false;
  }

  if (!platform_->TouchFileDurable(
          status_files_dir_.Append(kMigrationStartedFileName))) {
    LOG(ERROR) << "Failed to create migration-started file";
    return false;
  }

  int64_t free_space = platform_->AmountOfFreeDiskSpace(to_base_path_);
  if (free_space < 0) {
    LOG(ERROR) << "Failed to determine free disk space";
    return false;
  }
  if (static_cast<uint64_t>(free_space) < kMinFreeSpace) {
    LOG(ERROR) << "Not enough space to begin the migration";
    status_reporter.SetLowDiskSpaceFailure();
    return false;
  }
  effective_chunk_size_ =
      std::min(max_chunk_size_, free_space - kFreeSpaceBuffer);
  if (effective_chunk_size_ > kErasureBlockSize)
    effective_chunk_size_ =
        effective_chunk_size_ - (effective_chunk_size_ % kErasureBlockSize);

  CalculateDataToMigrate(from_base_path_);
  ReportStatus(DIRCRYPTO_MIGRATION_IN_PROGRESS);
  struct stat from_stat;
  if (!platform_->Stat(from_base_path_, &from_stat)) {
    PLOG(ERROR) << "Failed to stat from directory";
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtStat, base::FilePath());
    status_reporter.SetFileErrorFailure(
        failed_operation_type_, failed_error_type_);
    return false;
  }
  ReportTimerStart(kDircryptoMigrationTimer);
  LOG(INFO) << "Preparation took " << timer.Elapsed().InMilliseconds()
            << " ms.";
  if (!MigrateDir(base::FilePath(""),
                  FileEnumerator::FileInfo(from_base_path_, from_stat))) {
    LOG(ERROR) << "Migration Failed, aborting.";
    status_reporter.SetFileErrorFailure(
        failed_operation_type_, failed_error_type_);
    return false;
  }
  if (!resumed)
    ReportTimerStop(kDircryptoMigrationTimer);

  // One more progress update to say that we've hit 100%
  ReportStatus(DIRCRYPTO_MIGRATION_IN_PROGRESS);
  status_reporter.SetSuccess();
  const int elapsed_ms = timer.Elapsed().InMilliseconds();
  const int speed_kb_per_s = elapsed_ms ? (total_byte_count_ / elapsed_ms) : 0;
  LOG(INFO) << "Migrated " << total_byte_count_ << " bytes in " <<  elapsed_ms
            << " ms at " <<  speed_kb_per_s << " KB/s.";
  return true;
}

bool MigrationHelper::IsMigrationStarted() const {
  return platform_->FileExists(
      status_files_dir_.Append(kMigrationStartedFileName));
}

void MigrationHelper::CalculateDataToMigrate(const base::FilePath& from) {
  total_byte_count_ = 0;
  migrated_byte_count_ = 0;
  int n_files = 0, n_dirs = 0, n_symlinks = 0;
  std::unique_ptr<FileEnumerator> enumerator(platform_->GetFileEnumerator(
      from,
      true /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
      base::FileEnumerator::SHOW_SYM_LINKS));
  for (base::FilePath entry = enumerator->Next(); !entry.empty();
       entry = enumerator->Next()) {
    const FileEnumerator::FileInfo& info = enumerator->GetInfo();
    total_byte_count_ += info.GetSize();

    if (S_ISREG(info.stat().st_mode))
      ++n_files;
    if (S_ISDIR(info.stat().st_mode))
      ++n_dirs;
    if (S_ISLNK(info.stat().st_mode))
      ++n_symlinks;
  }
  LOG(INFO) << "Number of files: " << n_files;
  LOG(INFO) << "Number of directories: " << n_dirs;
  LOG(INFO) << "Number of symlinks: " << n_symlinks;
}

void MigrationHelper::IncrementMigratedBytes(uint64_t bytes) {
  migrated_byte_count_ += bytes;
  if (next_report_ < base::TimeTicks::Now())
    ReportStatus(DIRCRYPTO_MIGRATION_IN_PROGRESS);
}

void MigrationHelper::ReportStatus(DircryptoMigrationStatus status) {
  progress_callback_.Run(status, migrated_byte_count_, total_byte_count_);
  next_report_ = base::TimeTicks::Now() + kStatusSignalInterval;
}

bool MigrationHelper::MigrateDir(const base::FilePath& child,
                                 const FileEnumerator::FileInfo& info) {
  const base::FilePath from_dir = from_base_path_.Append(child);
  const base::FilePath to_dir = to_base_path_.Append(child);

  if (!platform_->CreateDirectory(to_dir)) {
    LOG(ERROR) << "Failed to create directory " << to_dir.value();
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtMkdir, child);
    return false;
  }
  if (!platform_->SyncDirectory(to_dir.DirName())) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtSync, child);
    return false;
  }
  if (!CopyAttributes(child, info))
    return false;

  std::unique_ptr<FileEnumerator> enumerator(platform_->GetFileEnumerator(
      from_dir,
      false /* is_recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
      base::FileEnumerator::SHOW_SYM_LINKS));

  for (base::FilePath entry = enumerator->Next(); !entry.empty();
       entry = enumerator->Next()) {
    const FileEnumerator::FileInfo& entry_info = enumerator->GetInfo();
    const base::FilePath& new_child = child.Append(entry.BaseName());
    mode_t mode = entry_info.stat().st_mode;
    if (S_ISLNK(mode)) {
      // Symlink
      if (!MigrateLink(new_child, entry_info))
        return false;
      IncrementMigratedBytes(entry_info.GetSize());
    } else if (S_ISDIR(mode)) {
      // Directory.
      if (!MigrateDir(new_child, entry_info))
        return false;
      IncrementMigratedBytes(entry_info.GetSize());
    } else if (S_ISREG(mode)) {
      // File
      if (!MigrateFile(new_child, entry_info))
        return false;
    } else {
      LOG(ERROR) << "Unknown file type: " << entry.value();
    }

    if (!platform_->DeleteFile(entry, false /* recursive */)) {
      LOG(ERROR) << "Failed to delete file " << entry.value();
      RecordFileErrorWithCurrentErrno(kMigrationFailedAtDelete, new_child);
      return false;
    }
  }
  if (!FixTimes(child))
    return false;
  if (!platform_->SyncDirectory(to_dir)) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtSync, child);
    return false;
  }

  return true;
}

bool MigrationHelper::MigrateLink(const base::FilePath& child,
                                  const FileEnumerator::FileInfo& info) {
  const base::FilePath source = from_base_path_.Append(child);
  const base::FilePath new_path = to_base_path_.Append(child);
  base::FilePath target;
  if (!platform_->ReadLink(source, &target)) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtReadLink, child);
    return false;
  }

  if (from_base_path_.IsParent(target)) {
    base::FilePath new_target = to_base_path_;
    from_base_path_.AppendRelativePath(target, &new_target);
    target = new_target;
  }
  // In the case that the link was already created by a previous migration
  // it should be removed to prevent errors recreating it below.
  if (!platform_->DeleteFile(new_path, false /* recursive */)) {
    PLOG(ERROR) << "Failed to delete existing symlink " << new_path.value();
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtDelete, child);
    return false;
  }
  if (!platform_->CreateSymbolicLink(new_path, target)) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtCreateLink, child);
    return false;
  }

  if (!CopyAttributes(child, info))
    return false;
  // mtime is copied here instead of in the general CopyAttributes call because
  // symlinks can't (and don't need to) use xattrs to preserve the time during
  // migration.
  if (!platform_->SetFileTimes(new_path,
                               info.stat().st_atim,
                               info.stat().st_mtim,
                               false /* follow_links */)) {
    PLOG(ERROR) << "Failed to set mtime for " << new_path.value();
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtSetAttribute, child);
    return false;
  }
  // We can't explicitly f(data)sync symlinks, so we have to do a full FS sync.
  platform_->Sync();
  return true;
}

bool MigrationHelper::MigrateFile(const base::FilePath& child,
                                  const FileEnumerator::FileInfo& info) {
  const base::FilePath& from_child = from_base_path_.Append(child);
  const base::FilePath& to_child = to_base_path_.Append(child);
  base::File from_file;
  platform_->InitializeFile(
      &from_file,
      from_child,
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
  if (!from_file.IsValid()) {
    if (from_file.error_details() == base::File::FILE_ERROR_IO) {
      // b/37444422 causes IO errors when opening this file in some cases.  User
      // had a unreadable file, skipping this file means user will no longer
      // have a file but not worse off.
      LOG(WARNING) << "Found file that cannot be opened with EIO, skipping "
                   << from_child.value();
      RecordFileError(kMigrationFailedAtOpenSourceFileNonFatal, child,
                      from_file.error_details());
      RecordSkippedFile(child);
      return true;
    }
    PLOG(ERROR) << "Failed to open file " << from_child.value();
    RecordFileError(kMigrationFailedAtOpenSourceFile, child,
                    from_file.error_details());
    return false;
  }

  base::File to_file;
  platform_->InitializeFile(
      &to_file,
      to_child,
      base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  if (!to_file.IsValid()) {
    PLOG(ERROR) << "Failed to open file " << to_child.value();
    RecordFileError(kMigrationFailedAtOpenDestinationFile, child,
                    to_file.error_details());
    return false;
  }
  if (!platform_->SyncDirectory(to_child.DirName())) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtSync, child);
    return false;
  }

  int64_t from_length = from_file.GetLength();
  int64_t to_length = to_file.GetLength();
  if (from_length < 0) {
    LOG(ERROR) << "Failed to get length of " << from_child.value();
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtStat, child);
    return false;
  }
  if (to_length < 0) {
    LOG(ERROR) << "Failed to get length of " << to_child.value();
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtStat, child);
    return false;
  }
  if (to_length < from_length) {
    // SetLength will call truncate, which on filesystems supporting sparse
    // files should not cause any actual disk space usage.  Instead only the
    // file's metadata is updated to reflect the new size.  Actual block
    // allocation will occur when attempting to write into space in the file
    // which is not yet allocated.
    if (!to_file.SetLength(from_length)) {
      PLOG(ERROR) << "Failed to set file length of " << to_child.value();
      RecordFileErrorWithCurrentErrno(kMigrationFailedAtTruncate, child);
      return false;
    }
  }

  if (!CopyAttributes(child, info))
    return false;

  while (from_length > 0) {
    size_t to_read = from_length % effective_chunk_size_;
    if (to_read == 0) {
      to_read = effective_chunk_size_;
    }
    off_t offset = from_length - to_read;
    if (to_file.Seek(base::File::FROM_BEGIN, offset) != offset) {
      LOG(ERROR) << "Failed to seek in " << to_child.value();
      RecordFileErrorWithCurrentErrno(kMigrationFailedAtSeek, child);
      return false;
    }
    // Sendfile is used here instead of a read to memory then write since it is
    // more efficient for transferring data from one file to another.  In
    // particular the data is passed directly from the read call to the write
    // in the kernel, never making a trip back out to user space.
    if (!platform_->SendFile(to_file, from_file, offset, to_read)) {
      RecordFileErrorWithCurrentErrno(kMigrationFailedAtSendfile, child);
      return false;
    }
    // For the last chunk, SyncFile will be called later so no need to flush
    // here. The same goes for SetLength as from_file will be deleted soon.
    if (offset > 0) {
      if (!to_file.Flush()) {
        PLOG(ERROR) << "Failed to flush " << to_child.value();
        RecordFileErrorWithCurrentErrno(kMigrationFailedAtSync, child);
        return false;
      }
      if (!from_file.SetLength(offset)) {
        PLOG(ERROR) << "Failed to truncate file " << from_child.value();
        RecordFileErrorWithCurrentErrno(kMigrationFailedAtTruncate, child);
        return false;
      }
    }
    from_length = offset;
    IncrementMigratedBytes(to_read);
  }

  from_file.Close();
  to_file.Close();
  if (!FixTimes(child))
    return false;
  if (!platform_->SyncFile(to_child)) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtSync, child);
    return false;
  }

  return true;
}

bool MigrationHelper::CopyAttributes(const base::FilePath& child,
                                     const FileEnumerator::FileInfo& info) {
  const base::FilePath from = from_base_path_.Append(child);
  const base::FilePath to = to_base_path_.Append(child);

  uid_t user_id = info.stat().st_uid;
  gid_t group_id = info.stat().st_gid;
  if (!platform_->SetOwnership(to, user_id, group_id,
                               false /* follow_links */)) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtSetAttribute, child);
    return false;
  }

  mode_t mode = info.stat().st_mode;
  // Symlinks don't support user extended attributes or permissions in linux
  if (S_ISLNK(mode))
    return true;
  if (!platform_->SetPermissions(to, mode)) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtSetAttribute, child);
    return false;
  }

  const auto& mtime = info.stat().st_mtim;
  const auto& atime = info.stat().st_atim;
  if (!SetExtendedAttributeIfNotPresent(to,
                                        namespaced_mtime_xattr_name_,
                                        reinterpret_cast<const char*>(&mtime),
                                        sizeof(mtime))) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtSetAttribute, child);
    return false;
  }
  if (!SetExtendedAttributeIfNotPresent(to,
                                        namespaced_atime_xattr_name_,
                                        reinterpret_cast<const char*>(&atime),
                                        sizeof(atime))) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtSetAttribute, child);
    return false;
  }
  if (!CopyExtendedAttributes(child))
    return false;

  int flags;
  if (!platform_->GetExtFileAttributes(from, &flags)) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtGetAttribute, child);
    return false;
  }
  if (!platform_->SetExtFileAttributes(to, flags)) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtSetAttribute, child);
    return false;
  }
  return true;
}

bool MigrationHelper::FixTimes(const base::FilePath& child) {
  const base::FilePath file = to_base_path_.Append(child);

  struct timespec mtime;
  if (!platform_->GetExtendedFileAttribute(file,
                                           namespaced_mtime_xattr_name_,
                                           reinterpret_cast<char*>(&mtime),
                                           sizeof(mtime))) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtGetAttribute, child);
    return false;
  }
  struct timespec atime;
  if (!platform_->GetExtendedFileAttribute(file,
                                           namespaced_atime_xattr_name_,
                                           reinterpret_cast<char*>(&atime),
                                           sizeof(atime))) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtGetAttribute, child);
    return false;
  }
  if (!platform_->SetFileTimes(file, atime, mtime, true /* follow_links */)) {
    PLOG(ERROR) << "Failed to set mtime on " << file.value();
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtSetAttribute, child);
    return false;
  }
  return true;
}

bool MigrationHelper::CopyExtendedAttributes(const base::FilePath& child) {
  const base::FilePath from = from_base_path_.Append(child);
  const base::FilePath to = to_base_path_.Append(child);

  std::vector<std::string> xattr_names;
  if (!platform_->ListExtendedFileAttributes(from, &xattr_names)) {
    RecordFileErrorWithCurrentErrno(kMigrationFailedAtGetAttribute, child);
    return false;
  }

  for (const std::string& name : xattr_names) {
    std::string value;
    if (name == namespaced_mtime_xattr_name_ ||
        name == namespaced_atime_xattr_name_)
      continue;
    if (!platform_->GetExtendedFileAttributeAsString(from, name, &value)) {
      RecordFileErrorWithCurrentErrno(kMigrationFailedAtGetAttribute, child);
      return false;
    }
    if (!platform_->SetExtendedFileAttribute(
            to, name, value.data(), value.length())) {
      RecordFileErrorWithCurrentErrno(kMigrationFailedAtSetAttribute, child);
      return false;
    }
  }

  return true;
}

bool MigrationHelper::SetExtendedAttributeIfNotPresent(
    const base::FilePath& file,
    const std::string& xattr,
    const char* value,
    ssize_t size) {
  // If the attribute already exists we assume it was set during a previous
  // migration attempt and use the existing one instead of writing a new one.
  if (platform_->HasExtendedFileAttribute(file, xattr)) {
    return true;
  }
  if (errno != ENOATTR) {
    PLOG(ERROR) << "Failed to get extended attribute " << xattr << " for "
                << file.value();
    return false;
  }
  return platform_->SetExtendedFileAttribute(file, xattr, value, size);
}

void MigrationHelper::RecordFileError(
    DircryptoMigrationFailedOperationType operation,
    const base::FilePath& child,
    base::File::Error error) {
  DircryptoMigrationFailedPathType path = kMigrationFailedUnderOther;
  for (const auto& path_type_mapping : kPathTypeMappings) {
    if (base::FilePath(path_type_mapping.path).IsParent(child)) {
      path = path_type_mapping.type;
      break;
    }
  }
  // Android cache files are either under
  //   root/android-data/data/data/<package name>/cache
  //   root/android-data/data/media/0/Android/data/<package name>/cache
  if (path == kMigrationFailedUnderAndroidOther) {
    std::vector<base::FilePath::StringType> components;
    child.GetComponents(&components);
    if ((components.size() >= 7u &&
         components[2] == FILE_PATH_LITERAL("data") &&
         components[3] == FILE_PATH_LITERAL("data") &&
         components[5] == FILE_PATH_LITERAL("cache")) ||
        (components.size() >= 10u &&
         components[2] == FILE_PATH_LITERAL("data") &&
         components[3] == FILE_PATH_LITERAL("media") &&
         components[4] == FILE_PATH_LITERAL("0") &&
         components[5] == FILE_PATH_LITERAL("Android") &&
         components[6] == FILE_PATH_LITERAL("data") &&
         components[8] == FILE_PATH_LITERAL("cache"))) {
       path = kMigrationFailedUnderAndroidCache;
    }
  }

  // Report UMA stats here for each single error.
  ReportDircryptoMigrationFailedOperationType(operation);
  ReportDircryptoMigrationFailedPathType(path);
  ReportDircryptoMigrationFailedErrorCode(error);

  // Record the data for the final end-status report.
  failed_operation_type_ = operation;
  failed_path_type_ = path;
  failed_error_type_ = error;
}

void MigrationHelper::RecordFileErrorWithCurrentErrno(
    DircryptoMigrationFailedOperationType operation,
    const base::FilePath& child) {
  RecordFileError(operation, child, base::File::OSErrorToFileError(errno));
}

void MigrationHelper::RecordSkippedFile(const base::FilePath& rel_path) {
  base::File skipped_file_list;
  platform_->InitializeFile(
      &skipped_file_list,
      skipped_file_list_path_,
      base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (!skipped_file_list.IsValid()) {
    PLOG(ERROR) << "Could not open list of skipped files at"
                << skipped_file_list_path_.value() << ", " << rel_path.value()
                << " not added";
    return;
  }
  std::string data = rel_path.value() + "\n";
  int write_size = data.size();
  if (write_size != skipped_file_list.Write(
                        0 /* offset ignored */, data.data(), write_size)) {
    PLOG(ERROR) << "Failed to write " << rel_path.value()
                << " to the list of skipped files";
    return;
  }
  if (!skipped_file_list.Flush()) {
    PLOG(ERROR) << "Failed to flush " << rel_path.value()
                << " to the list of skipped files";
  }
  if (skipped_file_list.created()) {
    if (!platform_->SyncDirectory(skipped_file_list_path_.DirName()))
      PLOG(ERROR) << "Failed to sync parent directory when creating list of "
                     "skipped files "
                  << skipped_file_list_path_.value();
  }
}

}  // namespace dircrypto_data_migrator
}  // namespace cryptohome
