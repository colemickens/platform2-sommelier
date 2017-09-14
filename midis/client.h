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
#include <mojo/public/cpp/bindings/binding.h>

#include "midis/device.h"
#include "midis/device_tracker.h"
#include "midis/libmidis/clientlib.h"
#include "mojo/midis.mojom.h"

namespace midis {

class Client : public DeviceTracker::Observer, public arc::mojom::MidisServer {
 public:
  using ClientDeletionCallback = base::Callback<void(uint32_t)>;
  ~Client() override;

  // Legacy Create() function to create a new client.
  // Still kept here while we are in the process of transitioning to Mojo
  // clients.
  // TODO(pmalani): Remove once Mojo server and client interfaces are fully
  // implemented.
  static std::unique_ptr<Client> Create(base::ScopedFD fd,
                                        DeviceTracker* device_tracker,
                                        uint32_t client_id,
                                        ClientDeletionCallback del_callback);

  // Similar to the original Create() call, but for Mojo clients.
  static std::unique_ptr<Client> CreateMojo(
      DeviceTracker* device_tracker,
      uint32_t client_id,
      ClientDeletionCallback del_callback,
      arc::mojom::MidisServerRequest request,
      arc::mojom::MidisClientPtr client_ptr);

  void NotifyDeviceAddedOrRemoved(const Device& dev, bool added);

 private:
  Client(base::ScopedFD fd,
         DeviceTracker* device_tracker,
         uint32_t client_id,
         ClientDeletionCallback del_cb,
         arc::mojom::MidisServerRequest request,
         arc::mojom::MidisClientPtr client_ptr);

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

  // This function is a DeviceTracker::Observer override.
  void OnDeviceAddedOrRemoved(const Device& dev, bool added) override;

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

  // arc::mojom::MidisServer
  void ListDevices(const ListDevicesCallback& callback) override;

  base::ScopedFD client_fd_;
  brillo::MessageLoop::TaskId msg_taskid_;
  // The DeviceTracker can be guaranteed to exist for the lifetime of the
  // service. As such, it is safe to maintain this pointer as a means to make
  // updates and derive information regarding devices.
  DeviceTracker* device_tracker_;
  uint32_t client_id_;
  base::Callback<void(uint32_t)> del_cb_;

  // Handle to the Mojo client interface. This is used to send necessary
  // information to the clients when required.
  arc::mojom::MidisClientPtr client_ptr_;
  mojo::Binding<arc::mojom::MidisServer> binding_;

  base::WeakPtrFactory<Client> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Client);
};

}  // namespace midis

#endif  // MIDIS_CLIENT_H_
