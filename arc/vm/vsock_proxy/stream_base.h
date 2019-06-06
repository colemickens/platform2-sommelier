// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_STREAM_BASE_H_
#define ARC_VM_VSOCK_PROXY_STREAM_BASE_H_

#include <stdint.h>
#include <string>
#include <vector>

#include <base/files/scoped_file.h>

namespace arc_proxy {
class FstatResponse;
class PreadResponse;
class VSockMessage;
}  // namespace arc_proxy

namespace arc {

// Interface to wrap file descriptor to support reading and writing Message
// protocol buffer.
class StreamBase {
 public:
  virtual ~StreamBase() = default;

  // Reads the message from the file descriptor.
  // Returns a struct of error_code, where it is 0 on succeess or errno, blob
  // and attached fds if available.
  struct ReadResult {
    int error_code;
    std::string blob;
    std::vector<base::ScopedFD> fds;
  };
  virtual ReadResult Read() = 0;

  // Writes the given blob and file descriptors to the wrapped file descriptor.
  // Returns true iff the whole message is written.
  virtual bool Write(std::string blob, std::vector<base::ScopedFD> fds) = 0;

  // Reads |count| bytes from the stream starting at |offset|.
  // Returns whether pread() is supported or not.
  // If supported, the result will be constructed in |response|.
  // Note: Internally, it is expected to call pread(2). Even if pread(2) fails,
  // this method will still return true (because pread is supported), then.
  virtual bool Pread(uint64_t count,
                     uint64_t offset,
                     arc_proxy::PreadResponse* response) = 0;

  // Fills the file descriptor's stat attribute to the |response|.
  // Returns whether fstat(2) is supported or not.
  // Internally, it is expected to call fstat(2). Even if fstat(2) fails,
  // this method will still return true (because fstat is supported), then.
  virtual bool Fstat(arc_proxy::FstatResponse* response) = 0;
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_STREAM_BASE_H_
