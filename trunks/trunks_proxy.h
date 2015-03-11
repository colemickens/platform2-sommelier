// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TRUNKS_PROXY_H_
#define TRUNKS_TRUNKS_PROXY_H_

#include "trunks/command_transceiver.h"

#include <string>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "trunks/trunks_export.h"

namespace trunks {

// TrunksProxy is a CommandTransceiver implementation that forwards all commands
// to the trunksd D-Bus daemon. See TrunksService for details on how the
// commands are handled once they reach trunksd.
class TRUNKS_EXPORT TrunksProxy: public CommandTransceiver {
 public:
  TrunksProxy();
  virtual ~TrunksProxy();

  // Initializes the D-Bus client. Returns true on success.
  bool Init();

  // CommandTransceiver methods.
  void SendCommand(const std::string& command,
                   const ResponseCallback& callback) override;
  std::string SendCommandAndWait(const std::string& command) override;

 private:
  // A callback for asynchronous D-Bus calls.
  void OnResponse(const ResponseCallback& callback,
                  dbus::Response* response);

  // Extracts and returns response data from a D-Bus response. If an error
  // occurs a well-formed error response will be returned.
  std::string GetResponseData(dbus::Response* response);

  scoped_ptr<dbus::MethodCall> CreateSendCommandMethodCall(
      const std::string& command);

  base::WeakPtr<TrunksProxy> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* object_;

  // Declared last so weak pointers are invalidated first on destruction.
  base::WeakPtrFactory<TrunksProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TrunksProxy);
};

}  // namespace trunks

#endif  // TRUNKS_TRUNKS_PROXY_H_
