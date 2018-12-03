// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_HELPER_PROCESS_H_
#define ARC_NETWORK_HELPER_PROCESS_H_

#include <sys/types.h>

#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <google/protobuf/message_lite.h>

namespace arc_networkd {

// Tracks a helper subprocess.  Handles forking, cleaning up on termination,
// and IPC.
// This object is used by the main Manager process.
class HelperProcess {
 public:
  HelperProcess() = default;
  virtual ~HelperProcess() = default;

  // Re-execs arc-networkd with a new argument: "|fd_arg|=N", where N is the
  // side of |control_fd|.  This tells the subprocess to start up a different
  // mainloop.
  void Start(int argc, char* argv[], const std::string& fd_arg);

  // Serializes a protobuf and sends it to the helper process.
  void SendMessage(const google::protobuf::MessageLite& proto) const;

  const pid_t pid() { return pid_; }

 protected:
  pid_t pid_{0};
  base::ScopedFD control_fd_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HelperProcess);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_HELPER_PROCESS_H_
