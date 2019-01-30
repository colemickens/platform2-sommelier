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

namespace base {
class FilePath;
}

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
  int64_t RegisterFileDescriptor(base::ScopedFD fd,
                                 arc_proxy::FileDescriptor::Type fd_type,
                                 int64_t handle);

  // Unregisters the |fd|. Internally, this destroys the corresponding stream
  // object.
  void UnregisterFileDescriptor(int fd);

  // Requests to connect(2) to a unix domain socket at |path| in the other
  // side.
  // |callback| will be called with errno, and the connected handle iff
  // succeeded.
  using ConnectCallback = base::OnceCallback<void(int, int64_t)>;
  void Connect(const base::FilePath& path, ConnectCallback callback);

 private:
  // Callback called when VSOCK gets ready to read.
  // Reads Message from VSOCK file descriptor, and dispatches it to the
  // corresponding local file descriptor.
  void OnVSockReadReady();

  // Handlers for each command.
  // TODO(crbug.com/842960): Use pass-by-value when protobuf is upreved enough
  // to support rvalues. (At least, 3.5, or maybe 3.6).
  void OnClose(arc_proxy::Close* close);
  void OnData(arc_proxy::Data* data);
  void OnConnectRequest(arc_proxy::ConnectRequest* request);
  void OnConnectResponse(arc_proxy::ConnectResponse* response);

  // Callback called when local file descriptor gets ready to read.
  // Reads Message from |fd|, and forwards to VSOCK file descriptor.
  void OnLocalFileDesciptorReadReady(int fd);

  const Type type_;

  VSockStream vsock_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> vsock_controller_;

  // Map from a raw file descriptor to corresponding info.
  // Note that the file descriptor should be owned by the stream instance.
  // Erasing the entry from this map should close the file descriptor
  // automatically.
  struct FileDescriptorInfo {
    // 64-bit handle representation in Message proto.
    int64_t handle;

    // Stream instane to read/write Message.
    std::unique_ptr<StreamBase> stream;

    // Controller of FileDescriptorWatcher. Destroying this will
    // stop watching.
    std::unique_ptr<base::FileDescriptorWatcher::Controller> controller;
  };
  std::map<int, FileDescriptorInfo> fd_map_;

  // Map from handle in the Message into a raw file descriptor.
  std::map<int64_t, int> handle_map_;

  // For handle and cookie generation rules, please find the comment in
  // message.proto.
  int64_t next_handle_;
  int64_t next_cookie_;

  // Map from cookie to its pending callback.
  std::map<int64_t, ConnectCallback> pending_connect_;

  // WeakPtrFactory needs to be declared as the member of the class, so that
  // on destruction, any pending Callbacks bound to WeakPtr are cancelled
  // first.
  base::WeakPtrFactory<VSockProxy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VSockProxy);
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_VSOCK_PROXY_H_
