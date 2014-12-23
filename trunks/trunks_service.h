// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TRUNKS_SERVICE_H_
#define TRUNKS_TRUNKS_SERVICE_H_

#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

#include <string>

#include "trunks/tpm_handle.h"

namespace trunks {

// TrunksService registers for and handles all incoming D-Bus messages for the
// trunksd system daemon.
class TrunksService {
 public:
  // The |transceiver| will be the target of all incoming TPM commands.
  explicit TrunksService(CommandTransceiver* transceiver);
  virtual ~TrunksService();

  // Registers for all trunksd D-Bus messages.
  void Init();

 private:
  // Handles calls to the 'SendCommand' method.
  void HandleSendCommand(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // A response callback for 'SendCommand'.
  void OnResponse(dbus::ExportedObject::ResponseSender response_sender,
                  scoped_ptr<dbus::Response> dbus_response,
                  const std::string& response_from_tpm);

  // This method sets up trunksd as a dbus service, and exports the
  // |HandleSendCommand| method via dbus.
  void InitDBusService();

  base::WeakPtr<TrunksService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* trunks_dbus_object_;
  CommandTransceiver* transceiver_;

  // Declared last so weak pointers are invalidated first on destruction.
  base::WeakPtrFactory<TrunksService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TrunksService);
};

}  // namespace trunks


#endif  // TRUNKS_TRUNKS_SERVICE_H_
