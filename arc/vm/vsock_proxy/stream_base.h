// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_STREAM_BASE_H_
#define ARC_VM_VSOCK_PROXY_STREAM_BASE_H_

#include <base/optional.h>

#include "arc/vm/vsock_proxy/message.pb.h"

namespace arc {

// Interface to wrap file descriptor to support reading and writing Message
// protocol buffer.
class StreamBase {
 public:
  virtual ~StreamBase() = default;

  // Reads the message from the file descriptor. Returns true on success,
  // and stores the read message in the |message|. Otherwise false.
  virtual bool Read(arc_proxy::Message* message) = 0;

  // Writes the serialized |message| to the file descriptor.
  // Returns true iff the whole message is written.
  virtual bool Write(const arc_proxy::Message& message) = 0;
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_STREAM_BASE_H_
