// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_SERVER_INTERFACE_H_
#define WEBSERVER_WEBSERVD_SERVER_INTERFACE_H_

#include <base/macros.h>

namespace webservd {

class ProtocolHandler;

// An abstract interface to expose Server object to IPC transport layer such as
// D-Bus.
class ServerInterface {
 public:
  ServerInterface() = default;

  // Called by ProtocolHandler to notify the server that a new protocol handler
  // appears online or goes offline.
  virtual void ProtocolHandlerStarted(ProtocolHandler* handler) = 0;
  virtual void ProtocolHandlerStopped(ProtocolHandler* handler) = 0;

  // Helper method to obtain the Debug-Request flag. This flag controls
  // additional logging from libmicrohttpd as well as more debug info sent
  // to browser in case of an error response.
  virtual bool UseDebugInfo() const = 0;

 protected:
  // This interface should not be used to control the life-time of the class
  // that derives from this interface. This is especially important when a mock
  // server class is used. Since the life-time of the mock must be controlled
  // by the test itself, we can't let some business logic suddenly delete
  // the instance of this interface.
  // So, just declare the destructor as protected, so nobody can just call
  // delete on a pointer to ServerInterface.
  ~ServerInterface() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerInterface);
};

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_SERVER_INTERFACE_H_
