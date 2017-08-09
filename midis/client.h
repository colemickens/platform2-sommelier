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
#include "midis/libmidis/clientlib.h"

namespace midis {

class Client : public DeviceTracker::Observer {
 public:
  using ClientDeletionCallback = base::Callback<void(uint32_t)>;
  ~Client();
  static std::unique_ptr<Client> Create(base::ScopedFD fd,
                                        DeviceTracker* device_tracker,
                                        uint32_t client_id,
                                        ClientDeletionCallback del_callback);
  void NotifyDeviceAddedOrRemoved(struct MidisDeviceInfo* dev_info, bool added);

 private:
  Client(base::ScopedFD fd, DeviceTracker* device_tracker, uint32_t client_id,
         ClientDeletionCallback del_cb);
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

  void HandleCloseDeviceMessage();

  // On receipt of a REQUEST_PORT message header, this function contacts
  // the requisite device, obtains an FD for the relevant subdevice, and
  // returns the FD.
  // The FD is returned via the cmsg mechanism. The buffer will contain a
  // struct MidisRequestPort with information regarding the port requested.
  // The cmsg header will contain information regarding the information
  // about the FD to be sent over.
  //
  // Just as the sendmsg() protocol is used by the server to send the FD, so too
  // the client must use the recvmsg() protocol to retrieve said FD.
  void AddClientToPort();

  void TriggerClientDeletion();

  base::ScopedFD client_fd_;
  brillo::MessageLoop::TaskId msg_taskid_;
  // The DeviceTracker can be guaranteed to exist for the lifetime of the
  // service. As such, it is safe to maintain this pointer as a means to make
  // updates and derive information regarding devices.
  DeviceTracker* device_tracker_;
  uint32_t client_id_;
  base::Callback<void(uint32_t)> del_cb_;
  base::WeakPtrFactory<Client> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Client);
};

}  // namespace midis

#endif  // MIDIS_CLIENT_H_
