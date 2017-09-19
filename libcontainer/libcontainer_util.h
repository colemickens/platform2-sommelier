// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCONTAINER_LIBCONTAINER_UTIL_H_
#define LIBCONTAINER_LIBCONTAINER_UTIL_H_

#include <string>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/macros.h>

namespace libcontainer {

// Simple class that saves errno.
class SaveErrno {
 public:
  SaveErrno();
  ~SaveErrno();

 private:
  const int saved_errno_;

  DISALLOW_COPY_AND_ASSIGN(SaveErrno);
};

// The comma operator will discard the SaveErrno instance, but will keep it
// alive until after the whole expression has been evaluated.
#define PLOG_PRESERVE(verbose_level) \
  ::libcontainer::SaveErrno(), PLOG(verbose_level)

// Given a uid/gid map of "inside1 outside1 length1, ...", and an id inside of
// the user namespace, return the equivalent outside id, or return < 0 on error.
int GetUsernsOutsideId(const std::string& map, int id);

int MakeDir(const base::FilePath& path, int uid, int gid, int mode);

int TouchFile(const base::FilePath& path, int uid, int gid, int mode);

// Find a free loop device and attach it.
int LoopdevSetup(const base::FilePath& source,
                 base::FilePath* loopdev_path_out);

// Detach the specified loop device.
int LoopdevDetach(const base::FilePath& loopdev);

// Create a new device mapper target for the source.
int DeviceMapperSetup(const base::FilePath& source,
                      const std::string& verity_cmdline,
                      base::FilePath* dm_path_out,
                      std::string* dm_name_out);

// Tear down the device mapper target.
int DeviceMapperDetach(const std::string& dm_name);

// Match mount_one in minijail, mount one mountpoint with
// consideration for combination of MS_BIND/MS_RDONLY flag.
int MountExternal(const std::string& src,
                  const std::string& dest,
                  const std::string& type,
                  unsigned long flags,
                  const std::string& data);

}  // namespace libcontainer

#endif  // LIBCONTAINER_LIBCONTAINER_UTIL_H_
