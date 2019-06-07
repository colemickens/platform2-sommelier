// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_SOCKET_STREAM_H_
#define ARC_VM_VSOCK_PROXY_SOCKET_STREAM_H_

#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "arc/vm/vsock_proxy/stream_base.h"

namespace arc {

// Wrapper of socket file descriptor to support reading and writing
// Message protocol buffer.
class SocketStream : public StreamBase {
 public:
  // Instantiates SocketStream based on the given |socket_fd|.
  // |proxy| must not be null. If a new file descriptor is passed from
  // local socket, or contained in |message| or write(), it will be
  // registered to |proxy|.
  explicit SocketStream(base::ScopedFD socket_fd);
  ~SocketStream() override;

  // StreamBase overrides:
  ReadResult Read() override;
  bool Write(std::string blob, std::vector<base::ScopedFD> fds) override;
  bool Pread(uint64_t count,
             uint64_t offset,
             arc_proxy::PreadResponse* response) override;
  bool Fstat(arc_proxy::FstatResponse* response) override;

 private:
  void TrySendMsg();

  base::ScopedFD socket_fd_;

  struct Data {
    std::string blob;
    std::vector<base::ScopedFD> fds;
  };
  std::deque<Data> pending_write_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> writable_watcher_;

  base::WeakPtrFactory<SocketStream> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SocketStream);
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_SOCKET_STREAM_H_
