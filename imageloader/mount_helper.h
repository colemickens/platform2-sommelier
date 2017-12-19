// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGELOADER_MOUNT_HELPER_H_
#define IMAGELOADER_MOUNT_HELPER_H_

#include <base/files/scoped_file.h>
#include <base/message_loop/message_loop.h>
#include <brillo/daemons/daemon.h>

#include "imageloader/ipc.pb.h"
#include "imageloader/verity_mounter.h"

using MessageLoopForIO = base::MessageLoopForIO;

struct cmsghdr;

namespace imageloader {

// Main loop for the Mount helper process.
// This object is used in the subprocess.
class MountHelper : public brillo::Daemon,
                    public base::MessageLoopForIO::Watcher {
 public:
  explicit MountHelper(base::ScopedFD control_fd);

 protected:
  // OVerrides Daemon init callback.
  int OnInit() override;

  // Overrides MessageLoopForIO callbacks for new data on |control_fd_|.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override {}

 private:
  CommandResponse HandleCommand(ImageCommand& command,
                                struct cmsghdr* cmsg);
  void SendResponse(CommandResponse& response);

  base::ScopedFD control_fd_;
  MessageLoopForIO::FileDescriptorWatcher control_watcher_;
  int pending_fd_;
  VerityMounter mounter_;

  DISALLOW_COPY_AND_ASSIGN(MountHelper);
};

}  // namespace imageloader

#endif  // IMAGELOADER_MOUNT_HELPER_H_
