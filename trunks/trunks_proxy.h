// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TRUNKS_PROXY_H_
#define TRUNKS_TRUNKS_PROXY_H_

#include <string>

#include <base/callback.h>
#include <chromeos/chromeos_export.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "trunks/command_transceiver.h"

namespace trunks {

typedef base::Callback<void(const std::string& response)> SendCommandCallback;

// TrunksProxy is a fully async implementation of the CommandTransceiver
// interface. Services that want to talk to the TPM through trunksd can
// get an instance of this singleton proxy. Then they can use the SendCommand
// method to send TPM commands to trunksd to forward to the TPM handle.
class CHROMEOS_EXPORT TrunksProxy: public CommandTransceiver {
 public:
  TrunksProxy();
  virtual ~TrunksProxy();
  // This method Initializes the dbus client in TrunksProxy. Returns true on
  // success and false on failure.
  virtual bool Init();
  // This method forwards |command| to trunksd via dbus, and calls |callback|
  // when the TPM is done processing |command|.
  virtual void SendCommand(const std::string& command,
                           const SendCommandCallback& callback);

 private:
  static void OnResponse(const SendCommandCallback& callback,
                         dbus::Response* response);

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* object_;

  DISALLOW_COPY_AND_ASSIGN(TrunksProxy);
};

}  // namespace trunks

#endif  // TRUNKS_TRUNKS_PROXY_H_
