// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_STREAM_BASE_H_
#define ARC_VM_VSOCK_PROXY_STREAM_BASE_H_

#include <stdint.h>

namespace arc_proxy {
class Data;
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

  // Reads the message from the file descriptor. Returns true on success,
  // and stores the read message in the |message|. Otherwise false.
  virtual bool Read(arc_proxy::VSockMessage* message) = 0;

  // Writes the serialized |data| to the file descriptor.
  // Returns true iff the whole message is written.
  virtual bool Write(arc_proxy::Data* data) = 0;

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
