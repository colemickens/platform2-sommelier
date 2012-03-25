// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_OPENVPN_MANAGEMENT_SERVER_
#define SHILL_OPENVPN_MANAGEMENT_SERVER_

#include <base/basictypes.h>
#include <base/cancelable_callback.h>
#include <base/memory/weak_ptr.h>

namespace shill {

class EventDispatcher;
class InputData;
class IOHandler;
class OpenVPNDriver;
class Sockets;

class OpenVPNManagementServer {
 public:
  OpenVPNManagementServer(OpenVPNDriver *driver);
  virtual ~OpenVPNManagementServer();

  // Returns true on success, false on failure.
  bool Start(EventDispatcher *dispatcher, Sockets *sockets);

  void Stop();

 private:
  // IO handler callbacks.
  void OnReady(int fd);
  void OnInput(InputData *data);

  void Send(const std::string &data);
  void SendState(const std::string &state);

  void ProcessMessage(const std::string &message);

  OpenVPNDriver *driver_;
  base::WeakPtrFactory<OpenVPNManagementServer> weak_ptr_factory_;
  base::Callback<void(int)> ready_callback_;
  base::Callback<void(InputData *)> input_callback_;

  Sockets *sockets_;
  int socket_;
  scoped_ptr<IOHandler> ready_handler_;
  EventDispatcher *dispatcher_;
  int connected_socket_;
  scoped_ptr<IOHandler> input_handler_;

  DISALLOW_COPY_AND_ASSIGN(OpenVPNManagementServer);
};

}  // namespace shill

#endif  // SHILL_OPENVPN_MANAGEMENT_SERVER_
