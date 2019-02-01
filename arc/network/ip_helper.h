// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_IP_HELPER_H_
#define ARC_NETWORK_IP_HELPER_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include <map>
#include <memory>
#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <brillo/daemons/daemon.h>

#include "arc/network/arc_ip_config.h"
#include "arc/network/device.h"
#include "arc/network/ipc.pb.h"

using MessageLoopForIO = base::MessageLoopForIO;

namespace arc_networkd {

// Main loop for the IP helper process.
// This object is used in the subprocess.
class IpHelper : public brillo::Daemon, public base::MessageLoopForIO::Watcher {
 public:
  explicit IpHelper(base::ScopedFD control_fd);

 protected:
  // Overrides Daemon init callback. Returns 0 on success and < 0 on error.
  int OnInit() override;

  // Overrides MessageLoopForIO callbacks for new data on |control_fd_|.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override {}

  void InitialSetup();

  // Callbacks from Daemon to notify that a signal was received
  // indicating the container is either booting up or going down.
  bool OnContainerStart(const struct signalfd_siginfo& info);
  bool OnContainerStop(const struct signalfd_siginfo& info);

  // Handle |pending_command_|.
  void HandleCommand();
  void EnableInbound(const std::string& dev,
                     const std::string& ifname,
                     pid_t pid);

  // Helper function to extract raw IPv6 address from a protobuf string.
  const struct in6_addr& ExtractAddr6(const std::string& in);

  // Validate interface name string.
  bool ValidateIfname(const std::string& in);

  void AddDevice(const std::string& ifname, const DeviceConfig& config);

 private:
  base::ScopedFD control_fd_;
  MessageLoopForIO::FileDescriptorWatcher control_watcher_;

  pid_t con_pid_;
  int con_init_tries_{0};

  IpHelperMessage pending_command_;

  // Keyed by device interface.
  std::map<std::string, std::unique_ptr<ArcIpConfig>> arc_ip_configs_;

  base::WeakPtrFactory<IpHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IpHelper);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_IP_HELPER_H_
