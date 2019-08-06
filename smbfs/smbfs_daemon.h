// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBFS_SMBFS_DAEMON_H_
#define SMBFS_SMBFS_DAEMON_H_

#include <fuse_lowlevel.h>
#include <sys/types.h>

#include <memory>

#include <base/macros.h>
#include <brillo/daemons/dbus_daemon.h>

namespace smbfs {

class FuseSession;
struct Options;

class SmbFsDaemon : public brillo::DBusDaemon {
 public:
  SmbFsDaemon(fuse_chan* chan, const Options& options);
  ~SmbFsDaemon() override;

 protected:
  // brillo::Daemon overrides.
  int OnEventLoopStarted() override;

 private:
  fuse_chan* chan_;
  const bool use_test_fs_;
  const uid_t uid_;
  const gid_t gid_;
  std::unique_ptr<FuseSession> session_;

  DISALLOW_COPY_AND_ASSIGN(SmbFsDaemon);
};

}  // namespace smbfs

#endif  // SMBFS_SMBFS_DAEMON_H_
