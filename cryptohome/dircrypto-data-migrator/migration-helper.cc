// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/dircrypto-data-migrator/migration-helper.h"

#include <algorithm>
#include <string>
#include <vector>

#include <fcntl.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/ioctl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/files/file.h>
#include <base/files/file_path.h>

extern "C" {
#include <attr/xattr.h>
#include <linux/fs.h>
}

namespace cryptohome {
namespace dircrypto_data_migrator {

namespace {
constexpr char kMtimeXattrName[] = "trusted.CrosDirCryptoMigrationMtime";
constexpr char kAtimeXattrName[] = "trusted.CrosDirCryptoMigrationAtime";
}  // namespace

constexpr base::FilePath::CharType kMigrationStartedFileName[] =
    "crypto-migration.started";

MigrationHelper::MigrationHelper(Platform* platform,
                                 const base::FilePath& status_files_dir,
                                 uint64_t chunk_size)
    : platform_(platform),
      status_files_dir_(status_files_dir),
      chunk_size_(chunk_size),
      namespaced_mtime_xattr_name_(kMtimeXattrName),
      namespaced_atime_xattr_name_(kAtimeXattrName) {}

MigrationHelper::~MigrationHelper() {}

bool MigrationHelper::Migrate(const base::FilePath& from,
                              const base::FilePath& to) {
  if (!from.IsAbsolute() || !to.IsAbsolute()) {
    LOG(ERROR) << "Migrate must be given absolute paths";
    return false;
  }

  if (!platform_->TouchFileDurable(
          status_files_dir_.Append(kMigrationStartedFileName))) {
    LOG(ERROR) << "Failed to create migration-started file";
    return false;
  }

  struct stat from_stat;
  if (!platform_->Stat(from, &from_stat)) {
    PLOG(ERROR) << "Failed to stat from directory";
    return false;
  }
  if (!MigrateDir(from,
                  to,
                  base::FilePath(""),
                  FileEnumerator::FileInfo(from, from_stat)))
    return false;
  return true;
}

bool MigrationHelper::IsMigrationStarted() const {
  return platform_->FileExists(
      status_files_dir_.Append(kMigrationStartedFileName));
}

bool MigrationHelper::MigrateDir(const base::FilePath& from,
                                 const base::FilePath& to,
                                 const base::FilePath& child,
                                 const FileEnumerator::FileInfo& info) {
  const base::FilePath from_dir = from.Append(child);
  const base::FilePath to_dir = to.Append(child);

  if (!platform_->CreateDirectory(to_dir)) {
    LOG(ERROR) << "Failed to create directory " << to_dir.value();
    return false;
  }
  if (!platform_->SyncDirectory(to_dir.DirName()))
    return false;
  if (!CopyAttributes(from_dir, to_dir, info))
    return false;

  FileEnumerator* enumerator = platform_->GetFileEnumerator(
      from_dir,
      false /* is_recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
          base::FileEnumerator::SHOW_SYM_LINKS);

  for (base::FilePath entry = enumerator->Next(); !entry.empty();
       entry = enumerator->Next()) {
    FileEnumerator::FileInfo entry_info = enumerator->GetInfo();
    base::FilePath new_path = to_dir.Append(entry.BaseName());
    mode_t mode = entry_info.stat().st_mode;
    if (S_ISLNK(mode)) {
      // Symlink
      if (!MigrateLink(from, to, child.Append(entry.BaseName()), entry_info))
        return false;
    } else if (S_ISDIR(mode)) {
      // Directory.
      if (!MigrateDir(from, to, child.Append(entry.BaseName()), entry_info))
        return false;
    } else if (S_ISREG(mode)) {
      // File
      if (!MigrateFile(entry, new_path, entry_info))
        return false;
    } else {
      LOG(ERROR) << "Unknown file type: " << entry.value();
    }

    if (!platform_->DeleteFile(entry, false /* recursive */)) {
      LOG(ERROR) << "Failed to delete file " << entry.value();
      return false;
    }
  }
  if (!FixTimes(to_dir))
    return false;
  if (!platform_->SyncDirectory(to_dir))
    return false;

  return true;
}

bool MigrationHelper::MigrateLink(const base::FilePath& from,
                                  const base::FilePath& to,
                                  const base::FilePath& child,
                                  const FileEnumerator::FileInfo& info) {
  const base::FilePath source = from.Append(child);
  const base::FilePath new_path = to.Append(child);
  base::FilePath target;
  if (!platform_->ReadLink(source, &target))
    return false;

  if (from.IsParent(target)) {
    base::FilePath new_target = to;
    from.AppendRelativePath(target, &new_target);
    target = new_target;
  }
  if (!platform_->CreateSymbolicLink(new_path, target))
    return false;

  if (!CopyAttributes(source, new_path, info))
    return false;
  // mtime is copied here instead of in the general CopyAttributes call because
  // symlinks can't (and don't need to) use xattrs to preserve the time during
  // migration.
  if (!platform_->SetFileTimes(new_path,
                               info.stat().st_atim,
                               info.stat().st_mtim,
                               false /* follow_links */)) {
    PLOG(ERROR) << "Failed to set mtime for " << new_path.value();
    return false;
  }
  // We can't explicitly f(data)sync symlinks, so we have to do a full FS sync.
  platform_->Sync();
  return true;
}

bool MigrationHelper::MigrateFile(const base::FilePath& from,
                                  const base::FilePath& to,
                                  const FileEnumerator::FileInfo& info) {
  // TODO(dspaid): Create a Platform method for opening these files to be
  // consistent with the rest of our file operations.
  base::File from_file(
      from,
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
  if (!from_file.IsValid()) {
    LOG(ERROR) << "Failed to open file " << from.value();
    return false;
  }

  base::File to_file(to, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  if (!to_file.IsValid()) {
    LOG(ERROR) << "Failed to open file " << to.value();
    return false;
  }

  int64_t from_length = from_file.GetLength();
  int64_t to_length = to_file.GetLength();
  if (from_length < 0) {
    LOG(ERROR) << "Failed to get length of " << from.value();
    return false;
  }
  if (to_length < 0) {
    LOG(ERROR) << "Failed to get length of " << to.value();
    return false;
  }
  bool new_file = to_file.created();
  if (to_length < from_length) {
    // SetLength will call truncate, which on filesystems supporting sparse
    // files should not cause any actual disk space usage.  Instead only the
    // file's metadata is updated to reflect the new size.  Actual block
    // allocation will occur when attempting to write into space in the file
    // which is not yet allocated.
    if (!to_file.SetLength(from_length)) {
      LOG(ERROR) << "Failed to set file length of " << to.value();
      return false;
    }
  }

  if (!CopyAttributes(from, to, info))
    return false;

  int64_t length;
  while ((length = from_file.GetLength()) > 0) {
    size_t to_read = length % chunk_size_;
    if (to_read == 0) {
      to_read = chunk_size_;
    }
    off_t offset = length - to_read;
    if (to_file.Seek(base::File::FROM_BEGIN, offset) != offset) {
      LOG(ERROR) << "Failed to seek in " << to.value();
      return false;
    }
    // Sendfile is used here instead of a read to memory then write since it is
    // more efficient for transferring data from one file to another.  In
    // particular the data is passed directly from the read call to the write
    // in the kernel, never making a trip back out to user space.
    if (!platform_->SendFile(to_file, from_file, offset, to_read)) {
      return false;
    }
    if (!to_file.Flush()) {
      LOG(ERROR) << "Failed to flush " << to.value();
      return false;
    }
    if (new_file) {
      if (!platform_->SyncDirectory(to.DirName()))
        return false;
      new_file = false;
    }
    if (!from_file.SetLength(offset)) {
      LOG(ERROR) << "Failed to truncate file " << from.value();
      return false;
    }
  }
  if (length < 0) {
    LOG(ERROR) << "Failed to get length of " << from.value();
    return false;
  }

  from_file.Close();
  to_file.Close();
  if (!FixTimes(to))
    return false;
  if (!platform_->SyncFile(to))
    return false;

  return true;
}

bool MigrationHelper::CopyAttributes(const base::FilePath& from,
                                     const base::FilePath& to,
                                     const FileEnumerator::FileInfo& info) {
  uid_t user_id = info.stat().st_uid;
  gid_t group_id = info.stat().st_gid;
  if (!platform_->SetOwnership(to, user_id, group_id, false /* follow_links */))
    return false;

  mode_t mode = info.stat().st_mode;
  // Symlinks don't support user extended attributes or permissions in linux
  if (S_ISLNK(mode))
    return true;
  if (!platform_->SetPermissions(to, mode))
    return false;

  struct timespec mtime = info.stat().st_mtim;
  struct timespec atime = info.stat().st_atim;
  if (!SetExtendedAttributeIfNotPresent(to,
                                        namespaced_mtime_xattr_name_,
                                        reinterpret_cast<char*>(&mtime),
                                        sizeof(mtime)))
    return false;
  if (!SetExtendedAttributeIfNotPresent(to,
                                        namespaced_atime_xattr_name_,
                                        reinterpret_cast<char*>(&atime),
                                        sizeof(atime)))
    return false;
  if (!CopyExtendedAttributes(from, to))
    return false;

  int flags;
  if (!platform_->GetExtFileAttributes(from, &flags))
    return false;
  if (!platform_->SetExtFileAttributes(to, flags))
    return false;
  return true;
}

bool MigrationHelper::FixTimes(const base::FilePath& file) {
  struct timespec mtime;
  if (!platform_->GetExtendedFileAttribute(file,
                                           namespaced_mtime_xattr_name_,
                                           reinterpret_cast<char*>(&mtime),
                                           sizeof(mtime)))
    return false;
  struct timespec atime;
  if (!platform_->GetExtendedFileAttribute(file,
                                           namespaced_atime_xattr_name_,
                                           reinterpret_cast<char*>(&atime),
                                           sizeof(atime)))
    return false;
  if (!platform_->SetFileTimes(file, atime, mtime, true /* follow_links */)) {
    PLOG(ERROR) << "Failed to set mtime on " << file.value();
    return false;
  }
  return true;
}

bool MigrationHelper::CopyExtendedAttributes(const base::FilePath& from,
                                             const base::FilePath& to) {
  std::vector<std::string> xattr_names;
  if (!platform_->ListExtendedFileAttributes(from, &xattr_names))
    return false;

  for (const std::string name : xattr_names) {
    std::string value;
    if (name == namespaced_mtime_xattr_name_ ||
        name == namespaced_atime_xattr_name_)
      continue;
    if (!platform_->GetExtendedFileAttributeAsString(from, name, &value))
      return false;
    if (!platform_->SetExtendedFileAttribute(
            to, name, value.data(), value.length()))
      return false;
  }

  return true;
}

bool MigrationHelper::SetExtendedAttributeIfNotPresent(
    const base::FilePath& file,
    const std::string& xattr,
    char* value,
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

}  // namespace dircrypto_data_migrator
}  // namespace cryptohome
