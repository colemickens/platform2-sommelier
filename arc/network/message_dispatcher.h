// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_MESSAGE_DISPATCHER_H_
#define ARC_NETWORK_MESSAGE_DISPATCHER_H_

#include <memory>
#include <string>

#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "arc/network/ipc.pb.h"

namespace arc_networkd {

// Helper message processor
class MessageDispatcher {
 public:
  explicit MessageDispatcher(base::ScopedFD fd);

  void RegisterFailureHandler(const base::Callback<void()>& handler);

  void RegisterGuestMessageHandler(
      const base::Callback<void(const GuestMessage&)>& handler);

  void RegisterDeviceMessageHandler(
      const base::Callback<void(const DeviceMessage&)>& handler);

 private:
  // Overrides MessageLoopForIO callbacks for new data on |control_fd_|.
  void OnFileCanReadWithoutBlocking();

  base::ScopedFD fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watcher_;
  base::Callback<void()> failure_handler_;
  base::Callback<void(const GuestMessage&)> guest_handler_;
  base::Callback<void(const DeviceMessage&)> device_handler_;

  IpHelperMessage msg_;

  base::WeakPtrFactory<MessageDispatcher> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MessageDispatcher);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MESSAGE_DISPATCHER_H_
