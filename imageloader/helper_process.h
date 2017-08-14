// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGELOADER_HELPER_PROCESS_H_
#define IMAGELOADER_HELPER_PROCESS_H_

#include <sys/types.h>

#include <base/files/scoped_file.h>
#include <base/macros.h>

namespace imageloader {

// Forward declare the FileSystem enum.
enum class FileSystem;

// Tracks a helper subprocess. Handles forking, cleaning up on termination, and
// IPC.
class HelperProcess {
 public:
  HelperProcess() = default;
  virtual ~HelperProcess() = default;

  // Re-execs imageloader with a new argument: "|fd_arg|=N", where N is the side
  // of |control_fd|. This tells the subprocess to start up a different
  // mainloop.
  void Start(int argc, char* argv[], const std::string& fd_arg);

  // Sends a message telling the helper process to mount the file backed by |fd|
  // at the |path|.
  virtual bool SendMountCommand(int fd, const std::string& path,
                                FileSystem fs_type,
                                const std::string& table);

  const pid_t pid() { return pid_; }

 protected:
  // Waits for a reply from the helper process indicating if the mount succeeded
  // or failed.
  virtual bool WaitForResponse();

  pid_t pid_{0};
  base::ScopedFD control_fd_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HelperProcess);
};

}  // namespace imageloader

#endif  // IMAGELOADER_HELPER_PROCESS_H_
