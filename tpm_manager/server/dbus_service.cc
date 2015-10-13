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

#include <brillo/bind_lambda.h>
#include <dbus/bus.h>
#include <dbus/object_path.h>

#include "tpm_manager/common/dbus_interface.h"

namespace tpm_manager {

DBusService::DBusService(const scoped_refptr<dbus::Bus>& bus,
                         TpmManagerInterface* service)
    : dbus_object_(nullptr, bus, dbus::ObjectPath(kTpmManagerServicePath)),
      service_(service) {}

void DBusService::Register(const CompletionAction& callback) {
  brillo::dbus_utils::DBusInterface* dbus_interface =
      dbus_object_.AddOrGetInterface(kTpmManagerInterface);

  dbus_interface->AddMethodHandler(
      kGetTpmStatus,
      base::Unretained(this),
      &DBusService::HandleDBusMethod<
          GetTpmStatusRequest,
          GetTpmStatusReply,
          &TpmManagerInterface::GetTpmStatus>);

  dbus_interface->AddMethodHandler(
      kTakeOwnership,
      base::Unretained(this),
      &DBusService::HandleDBusMethod<
          TakeOwnershipRequest,
          TakeOwnershipReply,
          &TpmManagerInterface::TakeOwnership>);

  dbus_interface->AddMethodHandler(
      kDefineNvram,
      base::Unretained(this),
      &DBusService::HandleDBusMethod<
          DefineNvramRequest,
          DefineNvramReply,
          &TpmManagerInterface::DefineNvram>);

  dbus_interface->AddMethodHandler(
      kDestroyNvram,
      base::Unretained(this),
      &DBusService::HandleDBusMethod<
          DestroyNvramRequest,
          DestroyNvramReply,
          &TpmManagerInterface::DestroyNvram>);

  dbus_interface->AddMethodHandler(
      kWriteNvram,
      base::Unretained(this),
      &DBusService::HandleDBusMethod<
          WriteNvramRequest,
          WriteNvramReply,
          &TpmManagerInterface::WriteNvram>);

  dbus_interface->AddMethodHandler(
      kReadNvram,
      base::Unretained(this),
      &DBusService::HandleDBusMethod<
          ReadNvramRequest,
          ReadNvramReply,
          &TpmManagerInterface::ReadNvram>);

  dbus_interface->AddMethodHandler(
      kIsNvramDefined,
      base::Unretained(this),
      &DBusService::HandleDBusMethod<
          IsNvramDefinedRequest,
          IsNvramDefinedReply,
          &TpmManagerInterface::IsNvramDefined>);

  dbus_interface->AddMethodHandler(
      kIsNvramLocked,
      base::Unretained(this),
      &DBusService::HandleDBusMethod<
          IsNvramLockedRequest,
          IsNvramLockedReply,
          &TpmManagerInterface::IsNvramLocked>);

  dbus_interface->AddMethodHandler(
      kGetNvramSize,
      base::Unretained(this),
      &DBusService::HandleDBusMethod<
          GetNvramSizeRequest,
          GetNvramSizeReply,
          &TpmManagerInterface::GetNvramSize>);

  dbus_object_.RegisterAsync(callback);
}

template<typename RequestProtobufType,
         typename ReplyProtobufType,
         DBusService::HandlerFunction<RequestProtobufType,
                                      ReplyProtobufType> func>
void DBusService::HandleDBusMethod(
    std::unique_ptr<DBusMethodResponse<const ReplyProtobufType&>> response,
    const RequestProtobufType& request) {
  // Convert |response| to a shared_ptr so |service_| can safely copy the
  // callback.
  using SharedResponsePointer = std::shared_ptr<
      DBusMethodResponse<const ReplyProtobufType&>>;
  // A callback that sends off the reply protobuf.
  auto callback = [](const SharedResponsePointer& response,
                     const ReplyProtobufType& reply) {
    response->Return(reply);
  };
  (service_->*func)(
      request,
      base::Bind(callback, SharedResponsePointer(std::move(response))));
}

}  // namespace tpm_manager
