// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_PIPE_STREAM_H_
#define ARC_VM_VSOCK_PROXY_PIPE_STREAM_H_

#include "arc/vm/vsock_proxy/stream_base.h"

#include <base/files/scoped_file.h>
#include <base/macros.h>

namespace arc {

// Wrapper of pipe file descriptor to support reading and writing
// Message protocol buffer.
class PipeStream : public StreamBase {
 public:
  explicit PipeStream(base::ScopedFD pipe_fd);
  ~PipeStream() override;

  // StreamBase overrides:
  bool Read(arc_proxy::VSockMessage* message) override;
  bool Write(arc_proxy::Data* data) override;
  bool Pread(uint64_t count,
             uint64_t offset,
             arc_proxy::PreadResponse* response) override;
  bool Fstat(arc_proxy::FstatResponse* response) override;

 private:
  base::ScopedFD pipe_fd_;

  DISALLOW_COPY_AND_ASSIGN(PipeStream);
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_PIPE_STREAM_H_
