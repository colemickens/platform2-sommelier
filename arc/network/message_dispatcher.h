// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_MESSAGE_DISPATCHER_H_
#define ARC_NETWORK_MESSAGE_DISPATCHER_H_

#include <memory>
#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>

#include "arc/network/ipc.pb.h"

namespace arc_networkd {

// Helper message processor
class MessageDispatcher : public base::MessageLoopForIO::Watcher {
 public:
  explicit MessageDispatcher(base::ScopedFD fd);

  void RegisterFailureHandler(const base::Callback<void()>& handler);

  void RegisterGuestMessageHandler(
      const base::Callback<void(const GuestMessage&)>& handler);

  void RegisterDeviceMessageHandler(
      const base::Callback<void(const DeviceMessage&)>& handler);

 private:
  // Overrides MessageLoopForIO callbacks for new data on |control_fd_|.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override {}

 private:
  base::ScopedFD fd_;
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;
  base::Callback<void()> failure_handler_;
  base::Callback<void(const GuestMessage&)> guest_handler_;
  base::Callback<void(const DeviceMessage&)> device_handler_;
  IpHelperMessage msg_;

  base::WeakPtrFactory<MessageDispatcher> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MessageDispatcher);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MESSAGE_DISPATCHER_H_
