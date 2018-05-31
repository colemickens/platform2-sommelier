// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_APPFUSE_APPFUSE_MOUNT_H_
#define ARC_APPFUSE_APPFUSE_MOUNT_H_

#include <stdint.h>
#include <sys/types.h>

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>

namespace arc {
namespace appfuse {

// AppfuseMount represents a mountpoint for each appfuse mount.
class AppfuseMount {
 public:
  AppfuseMount(const base::FilePath& mount_root, uid_t uid, int mount_id);
  ~AppfuseMount();

  // Mounts an appfuse file system and returns the /dev/fuse FD associated with
  // the mounted appfuse file system.
  base::ScopedFD Mount();

  // Unmounts the appfuse file system and returns true on success.
  bool Unmount();

  // Opens a file in the appfuse file system.
  base::ScopedFD OpenFile(int file_id, int flags);

 private:
  const base::FilePath mount_root_;
  const uid_t uid_;
  const int mount_id_;
  const base::FilePath mount_point_;

  DISALLOW_COPY_AND_ASSIGN(AppfuseMount);
};

}  // namespace appfuse
}  // namespace arc

#endif  // ARC_APPFUSE_APPFUSE_MOUNT_H_
