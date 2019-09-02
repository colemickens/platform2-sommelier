// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbfs/smbfs_daemon.h"

#include <sysexits.h>
#include <unistd.h>

#include <utility>

#include <base/bind.h>
#include <brillo/message_loops/message_loop.h>
#include <brillo/daemons/dbus_daemon.h>

#include "smbfs/fuse_session.h"
#include "smbfs/smb_filesystem.h"
#include "smbfs/smbfs.h"
#include "smbfs/test_filesystem.h"

namespace smbfs {

SmbFsDaemon::SmbFsDaemon(fuse_chan* chan, const Options& options)
    : chan_(chan),
      use_test_fs_(options.use_test),
      share_path_(options.share_path),
      uid_(options.uid ? options.uid : getuid()),
      gid_(options.gid ? options.gid : getgid()) {
  DCHECK(chan_);
}

SmbFsDaemon::~SmbFsDaemon() = default;

int SmbFsDaemon::OnInit() {
  int ret = brillo::DBusDaemon::OnInit();
  if (ret != EX_OK) {
    return ret;
  }

  if (!share_path_.empty()) {
    auto fs = std::make_unique<SmbFilesystem>(share_path_, uid_, gid_);
    SmbFilesystem::ConnectError error = fs->EnsureConnected();
    if (error != SmbFilesystem::ConnectError::kOk) {
      LOG(ERROR) << "Unable to connect to SMB filesystem: " << error;
      return EX_SOFTWARE;
    }
    fs_ = std::move(fs);
  }

  return EX_OK;
}

int SmbFsDaemon::OnEventLoopStarted() {
  int ret = brillo::DBusDaemon::OnEventLoopStarted();
  if (ret != EX_OK) {
    return ret;
  }

  std::unique_ptr<Filesystem> fs;
  if (use_test_fs_) {
    fs = std::make_unique<TestFilesystem>(uid_, gid_);
  } else if (fs_) {
    fs = std::move(fs_);
  } else {
    NOTREACHED();
  }

  session_ = std::make_unique<FuseSession>(std::move(fs), chan_);
  chan_ = nullptr;

  if (!session_->Start(base::BindOnce(&Daemon::Quit, base::Unretained(this)))) {
    return EX_SOFTWARE;
  }

  return EX_OK;
}

}  // namespace smbfs
