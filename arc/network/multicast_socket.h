// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_MULTICAST_SOCKET_H_
#define ARC_NETWORK_MULTICAST_SOCKET_H_

#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include <memory>
#include <string>

#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>

namespace arc_networkd {

// Wrapper around various syscalls used for multicast socket communications.
class MulticastSocket {
 public:
  MulticastSocket();
  virtual ~MulticastSocket();

  bool Bind(const std::string& ifname,
            const struct in_addr& mcast_addr,
            unsigned short port,
            base::Callback<void(int)> callback);
  bool SendTo(const void* data, size_t len, const struct sockaddr_in& addr);

  int fd() const { return fd_.get(); }
  time_t last_used() const { return last_used_; }

  struct sockaddr_in int_addr;

 protected:
  base::ScopedFD fd_;
  time_t last_used_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watcher_;
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MULTICAST_SOCKET_H_
