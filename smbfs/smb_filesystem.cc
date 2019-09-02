// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbfs/smb_filesystem.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/posix/safe_strerror.h>

namespace smbfs {

namespace {

constexpr char kSambaThreadName[] = "smbfs-libsmb";

constexpr double kAttrTimeoutSeconds = 5.0;
constexpr mode_t kAllowedFileTypes = S_IFREG | S_IFDIR;
constexpr mode_t kFileModeMask = kAllowedFileTypes | 0770;

void SambaLog(void* private_ptr, int level, const char* msg) {
  VLOG(level) << "libsmbclient: " << msg;
}

bool IsAllowedFileMode(mode_t mode) {
  return mode & kAllowedFileTypes;
}

}  // namespace

SmbFilesystem::SmbFilesystem(const std::string& share_path,
                             uid_t uid,
                             gid_t gid)
    : share_path_(share_path),
      uid_(uid),
      gid_(gid),
      samba_thread_(kSambaThreadName) {
  // Ensure files are not owned by root.
  CHECK_GT(uid_, 0);
  CHECK_GT(gid_, 0);

  CHECK(!share_path_.empty());
  CHECK_NE(share_path_.back(), '/');

  context_ = smbc_new_context();
  CHECK(context_);
  CHECK(smbc_init_context(context_));

  smbc_setOptionUserData(context_, this);
  smbc_setOptionFallbackAfterKerberos(context_, 1);

  smbc_setLogCallback(context_, nullptr, &SambaLog);
  int vlog_level = logging::GetVlogVerbosity();
  if (vlog_level > 0) {
    smbc_setDebug(context_, vlog_level);
  }

  smbc_closedir_ctx_ = smbc_getFunctionClosedir(context_);
  smbc_opendir_ctx_ = smbc_getFunctionOpendir(context_);
  smbc_stat_ctx_ = smbc_getFunctionStat(context_);

  CHECK(samba_thread_.Start());
}

SmbFilesystem::~SmbFilesystem() {
  // Stop the Samba processing thread before destroying the context to avoid a
  // UAF on the context.
  samba_thread_.Stop();
  smbc_free_context(context_, 1 /* shutdown_ctx */);
}

SmbFilesystem::ConnectError SmbFilesystem::EnsureConnected() {
  SMBCFILE* dir = smbc_opendir_ctx_(context_, share_path_.c_str());
  if (!dir) {
    int err = errno;
    PLOG(INFO) << "EnsureConnected smbc_opendir_ctx_ failed";
    switch (err) {
      case EPERM:
      case EACCES:
        return ConnectError::kAccessDenied;
      case ENODEV:
      case ENOENT:
      case ETIMEDOUT:
      // This means unable to resolve host, in some, but not necessarily all
      // cases.
      case EINVAL:
      // Host unreachable.
      case EHOSTUNREACH:
      // Host not listening on SMB port.
      case ECONNREFUSED:
        return ConnectError::kNotFound;
      case ECONNABORTED:
        return ConnectError::kSmb1Unsupported;
      default:
        LOG(WARNING) << "Unexpected error code " << err << ": "
                     << base::safe_strerror(err);
        return ConnectError::kUnknownError;
    }
  }

  smbc_closedir_ctx_(context_, dir);
  return ConnectError::kOk;
}

struct stat SmbFilesystem::MakeStat(ino_t inode,
                                    const struct stat& in_stat) const {
  struct stat stat = {0};
  stat.st_ino = inode;
  stat.st_mode = in_stat.st_mode & kFileModeMask;
  stat.st_uid = uid_;
  stat.st_gid = gid_;
  stat.st_nlink = 1;
  stat.st_size = in_stat.st_size;
  stat.st_atim = in_stat.st_atim;
  stat.st_ctim = in_stat.st_ctim;
  stat.st_mtim = in_stat.st_mtim;
  return stat;
}

void SmbFilesystem::Lookup(std::unique_ptr<EntryRequest> request,
                           fuse_ino_t parent_inode,
                           const std::string& name) {
  samba_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SmbFilesystem::LookupInternal, base::Unretained(this),
                     std::move(request), parent_inode, name));
}

void SmbFilesystem::LookupInternal(std::unique_ptr<EntryRequest> request,
                                   fuse_ino_t parent_inode,
                                   const std::string& name) {
  if (request->IsInterrupted()) {
    return;
  }

  const base::FilePath parent_path = inode_map_.GetPath(parent_inode);
  CHECK(!parent_path.empty())
      << "Lookup on invalid parent inode: " << parent_inode;
  const base::FilePath file_path = parent_path.Append(name);
  const std::string share_file_path = share_path_ + file_path.value();

  struct stat smb_stat = {0};
  int error = smbc_stat_ctx_(context_, share_file_path.c_str(), &smb_stat);
  if (error < 0) {
    request->ReplyError(errno);
    return;
  } else if (!IsAllowedFileMode(smb_stat.st_mode)) {
    VLOG(1) << "Disallowed file mode " << smb_stat.st_mode << " for path "
            << share_file_path;
    request->ReplyError(EACCES);
    return;
  }

  ino_t inode = inode_map_.IncInodeRef(file_path);
  struct stat entry_stat = MakeStat(inode, smb_stat);

  fuse_entry_param entry = {0};
  entry.ino = inode;
  entry.generation = 1;
  entry.attr = entry_stat;
  entry.attr_timeout = kAttrTimeoutSeconds;
  entry.entry_timeout = kAttrTimeoutSeconds;
  request->ReplyEntry(entry);
}

void SmbFilesystem::Forget(fuse_ino_t inode, uint64_t count) {
  samba_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&SmbFilesystem::ForgetInternal,
                                base::Unretained(this), inode, count));
}

void SmbFilesystem::ForgetInternal(fuse_ino_t inode, uint64_t count) {
  inode_map_.Forget(inode, count);
}

void SmbFilesystem::GetAttr(std::unique_ptr<AttrRequest> request,
                            fuse_ino_t inode) {
  samba_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SmbFilesystem::GetAttrInternal, base::Unretained(this),
                     std::move(request), inode));
}

void SmbFilesystem::GetAttrInternal(std::unique_ptr<AttrRequest> request,
                                    fuse_ino_t inode) {
  if (request->IsInterrupted()) {
    return;
  }

  const base::FilePath file_path = inode_map_.GetPath(inode);
  CHECK(!file_path.empty()) << "Inode not found: " << inode;
  const std::string share_file_path =
      share_path_ + (file_path == base::FilePath("/") ? "" : file_path.value());

  struct stat smb_stat = {0};
  int error = smbc_stat_ctx_(context_, share_file_path.c_str(), &smb_stat);
  if (error < 0) {
    request->ReplyError(errno);
    return;
  } else if (!IsAllowedFileMode(smb_stat.st_mode)) {
    VLOG(1) << "Disallowed file mode " << smb_stat.st_mode << " for path "
            << share_file_path;
    request->ReplyError(EACCES);
    return;
  }

  struct stat reply_stat = MakeStat(inode, smb_stat);
  request->ReplyAttr(reply_stat, kAttrTimeoutSeconds);
}

std::ostream& operator<<(std::ostream& out, SmbFilesystem::ConnectError error) {
  switch (error) {
    case SmbFilesystem::ConnectError::kOk:
      return out << "kOk";
    case SmbFilesystem::ConnectError::kNotFound:
      return out << "kNotFound";
    case SmbFilesystem::ConnectError::kAccessDenied:
      return out << "kAccessDenied";
    case SmbFilesystem::ConnectError::kSmb1Unsupported:
      return out << "kSmb1Unsupported";
    case SmbFilesystem::ConnectError::kUnknownError:
      return out << "kUnknownError";
    default:
      NOTREACHED();
      return out << "INVALID_ERROR";
  }
}

}  // namespace smbfs
