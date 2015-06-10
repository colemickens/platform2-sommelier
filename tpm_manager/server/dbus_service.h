// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_SERVER_DBUS_SERVICE_H_
#define TPM_MANAGER_SERVER_DBUS_SERVICE_H_

#include <memory>

#include <chromeos/dbus/dbus_method_response.h>
#include <chromeos/dbus/dbus_object.h>
#include <dbus/bus.h>

#include "tpm_manager/common/tpm_manager_interface.h"

namespace tpm_manager {

using CompletionAction =
    chromeos::dbus_utils::AsyncEventSequencer::CompletionAction;

// Handles D-Bus communtion with the TpmManager daemon.
class DBusService {
 public:
  // Does not take ownership of |service|. |service| must remain valid for the
  // lifetime of this instance.
  DBusService(const scoped_refptr<dbus::Bus>& bus,
              TpmManagerInterface* service);
  virtual ~DBusService() = default;

  // Connects to D-Bus system bus and exports TpmManager methods.
  void Register(const CompletionAction& callback);

  void set_service(TpmManagerInterface* service) {
    service_ = service;
  }

 private:
  friend class DBusServiceTest;

  // Handles the GetTpmStatus D-Bus call.
  void HandleGetTpmStatus(
      std::unique_ptr<chromeos::dbus_utils::DBusMethodResponse<
          const GetTpmStatusReply&>> response,
      const GetTpmStatusRequest& request);

  // Handles the TakeOwnership D-Bus call.
  void HandleTakeOwnership(
      std::unique_ptr<chromeos::dbus_utils::DBusMethodResponse<
          const TakeOwnershipReply&>> response,
      const TakeOwnershipRequest& request);

  chromeos::dbus_utils::DBusObject dbus_object_;
  TpmManagerInterface* service_;
  DISALLOW_COPY_AND_ASSIGN(DBusService);
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_DBUS_SERVICE_H_
