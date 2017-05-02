// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_CLIENT_H_
#define MIDIS_CLIENT_H_

#include <memory>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/memory/weak_ptr.h>
#include <brillo/message_loops/message_loop.h>

#include "midis/device.h"
#include "midis/device_tracker.h"
#include "midis/messages.h"

namespace midis {

class Client : public DeviceTracker::Observer {
 public:
  ~Client();
  static std::unique_ptr<Client> Create(base::ScopedFD fd,
                                        DeviceTracker* device_tracker);

 private:
  Client(base::ScopedFD fd, DeviceTracker* device_tracker);
  // Start monitoring the client socket fd for messages from the client.
  bool StartMonitoring();
  // Stop the task which was watching the client socket df.
  void StopMonitoring();
  // Main function to handle all messages sent by the client.
  // Messages will be of the following format:
  //
  // |<--- 4 bytes --->|<----  4 bytes  ---->|<---- payload_size bytes --->|
  // |  message type   |  size of payload    |      message payload        |
  void HandleClientMessages();

  // Return the list of all devices available to the client.
  // The message format will be the same as that used by Client messages.
  void SendDevicesList();

  // Prepare the payload for devices information.
  // Payload structure:
  //
  // |<-- 1 byte -->|<-- sizeof(struct MidisDeviceInfo) bytes -->|....
  // |  num entries |             device1_info                   |  device2_info
  // |....
  uint32_t PrepareDeviceListPayload(uint8_t* payload_buf, size_t buf_len);

  // This function is a DeviceTracker::Observer override.
  void OnDeviceAddedOrRemoved(const struct MidisDeviceInfo* dev_info,
                              bool added) override;

  base::ScopedFD client_fd_;
  brillo::MessageLoop::TaskId msg_taskid_;
  // The DeviceTracker can be guaranteed to exist for the lifetime of the
  // service. As such, it is safe to maintain this pointer as a means to make
  // updates and derive information regarding devices.
  DeviceTracker* device_tracker_;
  base::WeakPtrFactory<Client> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Client);
};

}  // namespace midis

#endif  // MIDIS_CLIENT_H_
