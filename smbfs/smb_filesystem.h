// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBFS_SMB_FILESYSTEM_H_
#define SMBFS_SMB_FILESYSTEM_H_

#include <libsmbclient.h>
#include <sys/types.h>

#include <memory>
#include <ostream>
#include <string>

#include <base/id_map.h>
#include <base/macros.h>
#include <base/threading/thread.h>

#include "smbfs/filesystem.h"
#include "smbfs/inode_map.h"

namespace smbfs {

class SmbFilesystem : public Filesystem {
 public:
  enum class ConnectError {
    kOk = 0,
    kNotFound,
    kAccessDenied,
    kSmb1Unsupported,
    kUnknownError,
  };

  SmbFilesystem(const std::string& share_path, uid_t uid, gid_t gid);
  ~SmbFilesystem() override;

  // Ensures that the SMB share can be connected to. Must NOT be called after
  // the filesystem is attached to a FUSE session.
  ConnectError EnsureConnected();

  // Filesystem overrides.
  void Lookup(std::unique_ptr<EntryRequest> request,
              fuse_ino_t parent_inode,
              const std::string& name) override;
  void Forget(fuse_ino_t inode, uint64_t count) override;
  void GetAttr(std::unique_ptr<AttrRequest> request, fuse_ino_t inode) override;
  void OpenDir(std::unique_ptr<OpenRequest> request,
               fuse_ino_t inode,
               int flags) override;
  void ReadDir(std::unique_ptr<DirentryRequest> request,
               fuse_ino_t inode,
               uint64_t file_handle,
               off_t offset) override;
  void ReleaseDir(std::unique_ptr<SimpleRequest> request,
                  fuse_ino_t inode,
                  uint64_t file_handle) override;

 private:
  // Filesystem implementations that execute on |samba_thread_|.
  void LookupInternal(std::unique_ptr<EntryRequest> request,
                      fuse_ino_t parent_inode,
                      const std::string& name);
  void ForgetInternal(fuse_ino_t inode, uint64_t count);
  void GetAttrInternal(std::unique_ptr<AttrRequest> request, fuse_ino_t inode);
  void OpenDirInternal(std::unique_ptr<OpenRequest> request,
                       fuse_ino_t inode,
                       int flags);
  void ReadDirInternal(std::unique_ptr<DirentryRequest> request,
                       fuse_ino_t inode,
                       uint64_t file_handle,
                       off_t offset);
  void ReleaseDirInternal(std::unique_ptr<SimpleRequest> request,
                          fuse_ino_t inode,
                          uint64_t file_handle);

  // Constructs a sanitised stat struct for sending as a response.
  struct stat MakeStat(ino_t inode, const struct stat& in_stat) const;

  // Constructs a share file path suitable for passing to libsmbclient from the
  // given absolute file path.
  std::string MakeShareFilePath(const base::FilePath& path) const;

  // Construct a share file path from the |inode|. |inode| must be a valid inode
  // number.
  std::string ShareFilePathFromInode(ino_t inode) const;

  const std::string share_path_;
  const uid_t uid_;
  const gid_t gid_;
  base::Thread samba_thread_;
  InodeMap inode_map_{FUSE_ROOT_ID};
  IDMap<SMBCFILE*, uint64_t> open_files_;

  SMBCCTX* context_ = nullptr;
  smbc_closedir_fn smbc_closedir_ctx_ = nullptr;
  smbc_lseekdir_fn smbc_lseekdir_ctx_ = nullptr;
  smbc_opendir_fn smbc_opendir_ctx_ = nullptr;
  smbc_readdir_fn smbc_readdir_ctx_ = nullptr;
  smbc_stat_fn smbc_stat_ctx_ = nullptr;
  smbc_telldir_fn smbc_telldir_ctx_ = nullptr;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SmbFilesystem);
};

std::ostream& operator<<(std::ostream& out, SmbFilesystem::ConnectError error);

}  // namespace smbfs

#endif  // SMBFS_SMB_FILESYSTEM_H_
