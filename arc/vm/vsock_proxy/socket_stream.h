// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_SOCKET_STREAM_H_
#define ARC_VM_VSOCK_PROXY_SOCKET_STREAM_H_

#include <base/files/scoped_file.h>
#include <base/macros.h>

#include "arc/vm/vsock_proxy/stream_base.h"

namespace arc {

class VSockProxy;

// Wrapper of socket file descriptor to support reading and writing
// Message protocol buffer.
class SocketStream : public StreamBase {
 public:
  // Instantiates SocketStream based on the given |socket_fd|.
  // |proxy| must not be null. If a new file descriptor is passed from
  // local socket, or contained in |message| or write(), it will be
  // registered to |proxy|.
  SocketStream(base::ScopedFD socket_fd, VSockProxy* proxy);
  ~SocketStream() override;

  // StreamBase overrides:
  bool Read(arc_proxy::VSockMessage* message) override;
  bool Write(arc_proxy::Data* data) override;
  bool Pread(uint64_t count,
             uint64_t offset,
             arc_proxy::PreadResponse* response) override;
  bool Fstat(arc_proxy::FstatResponse* response) override;

 private:
  base::ScopedFD socket_fd_;
  VSockProxy* const proxy_;  // Owned by ServerProxy.

  DISALLOW_COPY_AND_ASSIGN(SocketStream);
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_SOCKET_STREAM_H_
