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
#include "smbfs/smbfs.h"
#include "smbfs/test_filesystem.h"

namespace smbfs {

SmbFsDaemon::SmbFsDaemon(fuse_chan* chan, const Options& options)
    : chan_(chan),
      use_test_fs_(options.use_test),
      uid_(options.uid ? options.uid : getuid()),
      gid_(options.gid ? options.gid : getgid()) {
  DCHECK(chan_);
}

SmbFsDaemon::~SmbFsDaemon() = default;

int SmbFsDaemon::OnEventLoopStarted() {
  int ret = brillo::DBusDaemon::OnEventLoopStarted();
  if (ret != EX_OK) {
    return ret;
  }

  std::unique_ptr<Filesystem> fs;
  if (use_test_fs_) {
    fs = std::make_unique<TestFilesystem>(uid_, gid_);
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
