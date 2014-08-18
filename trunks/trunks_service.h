// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TRUNKS_SERVICE_H_
#define TRUNKS_TRUNKS_SERVICE_H_

#include <base/memory/scoped_ptr.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

#include <string>

#include "trunks/tpm_handle.h"

namespace trunks {

// TrunksService is the implementation of the trunks Dbus Daemon, Trunksd
// Trunksd posesses the sole handle to "/dev/tpm0" and uses it to send
// commands and receive replies from the TPM.
class TrunksService {
 public:
  TrunksService();
  virtual ~TrunksService();
  // Initializes trunks daemon, sets up its dbus interface,
  // and exports methods.
  virtual void Init(TpmHandle* tpm);

 private:
  // This method handles calls to the Dbus exported method |SendCommand|
  // It is the sole means of communication with the TPM in trunks.
  virtual void HandleSendCommand(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);
  // This method sets up the minijail sandbox. It sets up the seccomp
  // filters and drops the Trunks Daemon user down from |root| into
  // |trunks|.
  virtual void InitMinijailSandbox();
  // This method sets up trunksd as a dbus service, and exports the
  // |HandleSendCommand| method via dbus.
  virtual void InitDbusService();

  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* trunks_dbus_object_;
  trunks::TpmHandle* tpm_;

  DISALLOW_COPY_AND_ASSIGN(TrunksService);
};

}  // namespace trunks


#endif  // TRUNKS_TRUNKS_SERVICE_H_
