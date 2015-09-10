//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "tpm_manager/server/dbus_service.h"

#include <memory>
#include <string>

#include <chromeos/bind_lambda.h>
#include <dbus/bus.h>
#include <dbus/object_path.h>

#include "tpm_manager/common/dbus_interface.h"

using chromeos::dbus_utils::DBusMethodResponse;

namespace tpm_manager {

DBusService::DBusService(const scoped_refptr<dbus::Bus>& bus,
                         TpmManagerInterface* service)
    : dbus_object_(nullptr, bus, dbus::ObjectPath(kTpmManagerServicePath)),
      service_(service) {}

void DBusService::Register(const CompletionAction& callback) {
  chromeos::dbus_utils::DBusInterface* dbus_interface =
      dbus_object_.AddOrGetInterface(kTpmManagerInterface);

  dbus_interface->AddMethodHandler(kGetTpmStatus, base::Unretained(this),
                                   &DBusService::HandleGetTpmStatus);
  dbus_interface->AddMethodHandler(kTakeOwnership, base::Unretained(this),
                                   &DBusService::HandleTakeOwnership);
  dbus_object_.RegisterAsync(callback);
}

void DBusService::HandleGetTpmStatus(
    std::unique_ptr<DBusMethodResponse<const GetTpmStatusReply&>> response,
    const GetTpmStatusRequest& request) {
  // Convert |response| to a shared_ptr so |service_| can safely copy the
  // callback.
  using SharedResponsePointer = std::shared_ptr<
      DBusMethodResponse<const GetTpmStatusReply&>>;
  // A callback that sends off the reply protobuf.
  auto callback = [](const SharedResponsePointer& response,
                     const GetTpmStatusReply& reply) {
    response->Return(reply);
  };
  service_->GetTpmStatus(
      request,
      base::Bind(callback, SharedResponsePointer(std::move(response))));
}

void DBusService::HandleTakeOwnership(
    std::unique_ptr<DBusMethodResponse<const TakeOwnershipReply&>> response,
    const TakeOwnershipRequest& request) {
  // Convert |response| to a shared_ptr so |service_| can safely copy the
  // callback.
  using SharedResponsePointer = std::shared_ptr<
      DBusMethodResponse<const TakeOwnershipReply&>>;
  // A callback that sends off the reply protobuf.
  auto callback = [](const SharedResponsePointer& response,
                     const TakeOwnershipReply& reply) {
    response->Return(reply);
  };
  service_->TakeOwnership(
      request,
      base::Bind(callback, SharedResponsePointer(std::move(response))));
}

}  // namespace tpm_manager
