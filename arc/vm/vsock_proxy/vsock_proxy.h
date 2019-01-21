// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_VSOCK_PROXY_H_
#define ARC_VM_VSOCK_PROXY_VSOCK_PROXY_H_

#include <stdint.h>

#include <map>
#include <memory>

#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "arc/vm/vsock_proxy/message.pb.h"
#include "arc/vm/vsock_proxy/vsock_stream.h"

namespace arc {

class StreamBase;

// Proxies between local file descriptors and given VSOCK socket by Message
// protocol.
class VSockProxy {
 public:
  // Represents whether this proxy is server (host) side one, or client (guest)
  // side one.
  enum class Type {
    SERVER = 1,
    CLIENT = 2,
  };

  VSockProxy(Type type, base::ScopedFD vsock);
  ~VSockProxy();

  // Registers the |fd| whose type is |fd_type| to watch.
  // Internally, this creates Stream object to read/write Message protocol
  // buffer.
  // If |handle| is the value corresponding to the file descriptor on
  // messages on VSOCK. If 0 is set, this internally generates the handle.
  // Returns handle or 0 on error.
  uint64_t RegisterFileDescriptor(base::ScopedFD fd,
                                  arc_proxy::FileDescriptor::Type fd_type,
                                  uint64_t handle);

  // Unregisters the |fd|. Internally, this destroys the corresponding stream
  // object.
  void UnregisterFileDescriptor(int fd);

 private:
  // Callback called when VSOCK gets ready to read.
  // Reads Message from VSOCK file descriptor, and dispatches it to the
  // corresponding local file descriptor.
  void OnVSockReadReady();

  // Callback called when local file descriptor gets ready to read.
  // Reads Message from |fd|, and forwards to VSOCK file descriptor.
  void OnLocalFileDesciptorReadReady(int fd);

  VSockStream vsock_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> vsock_controller_;

  // Map from a raw file descriptor to corresponding info.
  // Note that the file descriptor should be owned by the stream instance.
  // Erasing the entry from this map should close the file descriptor
  // automatically.
  struct FileDescriptorInfo {
    // 64-bit handle representation in Message proto.
    uint64_t handle;

    // Stream instane to read/write Message.
    std::unique_ptr<StreamBase> stream;

    // Controller of FileDescriptorWatcher. Destroying this will
    // stop watching.
    std::unique_ptr<base::FileDescriptorWatcher::Controller> controller;
  };
  std::map<int, FileDescriptorInfo> fd_map_;

  // Map from handle in the Message into a raw file descriptor.
  std::map<uint64_t, int> handle_map_;

  // File descriptor can be created in either side (guest or host), and it is
  // necessary that the handler is unique across guest and host.
  // In host side, the value is started from 2, since 1 is reserved for the
  // /run/chrome/arc_bridge.sock connection.
  // In guest side, the value is started from 1000000000000000001ULL, which
  // is an arbitrally huge value, in order to avoid conflict.
  // TODO(hidehiko,yusukes,keiichiw): Fix the protocol.
  uint64_t next_handle_;

  // WeakPtrFactory needs to be declared as the member of the class, so that
  // on destruction, any pending Callbacks bound to WeakPtr are cancelled
  // first.
  base::WeakPtrFactory<VSockProxy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VSockProxy);
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_VSOCK_PROXY_H_
