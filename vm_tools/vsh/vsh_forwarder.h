// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_VSH_VSH_FORWARDER_H_
#define VM_TOOLS_VSH_VSH_FORWARDER_H_

#include <pwd.h>

#include <memory>
#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <brillo/asynchronous_signal_handler.h>
#include <google/protobuf/message_lite.h>

#include "vm_tools/vsh/scoped_termios.h"
#include "vsh.pb.h"  // NOLINT(build/include)

namespace vm_tools {
namespace vsh {

// VshForwarder encapsulates a vsh forwarder session.
// This class is not thread-safe.
class VshForwarder {
 public:
  static std::unique_ptr<VshForwarder> Create(base::ScopedFD sock_fd,
                                              bool inherit_env);
  ~VshForwarder() = default;

 private:
  VshForwarder(base::ScopedFD sock_fd, bool inherit_env);

  bool Init();

  bool HandleSigchld(const struct signalfd_siginfo& siginfo);
  void HandleVsockReadable();
  void HandleTargetReadable(int fd, StdioStream stream_type);

  bool SendConnectionResponse(vm_tools::vsh::ConnectionStatus status,
                              const std::string& description);
  void PrepareExec(
      const char* pts,
      const struct passwd* passwd,
      const vm_tools::vsh::SetupConnectionRequest& connection_request);

  void SendExitMessage();

  std::array<base::ScopedFD, 3> stdio_pipes_;
  brillo::MessageLoop::TaskId stdout_task_;
  brillo::MessageLoop::TaskId stderr_task_;
  base::ScopedFD ptm_fd_;
  base::ScopedFD sock_fd_;
  bool inherit_env_;
  bool interactive_;

  brillo::AsynchronousSignalHandler signal_handler_;

  bool exit_pending_;
  int exit_code_;

  DISALLOW_COPY_AND_ASSIGN(VshForwarder);
};

}  // namespace vsh
}  // namespace vm_tools

#endif  // VM_TOOLS_VSH_VSH_FORWARDER_H_
