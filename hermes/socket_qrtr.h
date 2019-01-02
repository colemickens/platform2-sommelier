// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_SOCKET_QRTR_H_
#define HERMES_SOCKET_QRTR_H_

#include <base/files/scoped_file.h>
#include <base/message_loop/message_loop.h>

#include "hermes/socket_interface.h"

namespace hermes {

class SocketQrtr : public SocketInterface,
                   public base::MessageLoopForIO::Watcher {
 public:
  struct PacketMetadata {
    uint32_t port;
    uint32_t node;
  };

  SocketQrtr();

  void SetDataAvailableCallback(DataAvailableCallback cb) override;

  bool Open() override;
  void Close() override;
  bool IsValid() const override { return socket_.is_valid(); }
  Type GetType() const override { return Type::kQrtr; }

  bool StartService(uint32_t service, uint16_t version_major,
                    uint16_t version_minor) override;
  bool StopService(uint32_t service, uint16_t version_major,
                   uint16_t version_minor) override;

  // If the metadata ptr is not null, it must point to a
  // SocketQrtr::PacketMetadata instance.
  int Recv(void* buf, size_t size, void* metadata) override;
  int Send(const void* data, size_t size) override;

 private:
  // base::MesageLoopForIO::Watcher methods.
  void OnFileCanReadWithoutBlocking(int socket) override;
  void OnFileCanWriteWithoutBlocking(int socket) override;

 private:
  uint32_t node_;
  uint32_t port_;
  base::ScopedFD socket_;

  DataAvailableCallback cb_;
  // FileDescriptorWatcher to watch QRTR socket for |CallWhenDataAvailable|.
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;
};

}  // namespace hermes

#endif  // HERMES_SOCKET_QRTR_H_
