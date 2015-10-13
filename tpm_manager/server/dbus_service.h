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

#ifndef TPM_MANAGER_SERVER_DBUS_SERVICE_H_
#define TPM_MANAGER_SERVER_DBUS_SERVICE_H_

#include <memory>

#include <brillo/dbus/dbus_method_response.h>
#include <brillo/dbus/dbus_object.h>
#include <dbus/bus.h>

#include "tpm_manager/common/tpm_manager_interface.h"

namespace tpm_manager {

using brillo::dbus_utils::DBusMethodResponse;
using CompletionAction =
    brillo::dbus_utils::AsyncEventSequencer::CompletionAction;

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

 private:
  friend class DBusServiceTest;

  template<typename RequestProtobufType,
           typename ReplyProtobufType>
  using HandlerFunction = void(TpmManagerInterface::*)(
      const RequestProtobufType&,
      const base::Callback<void(const ReplyProtobufType&)>&);

  // Template to handle D-Bus calls.
  template<typename RequestProtobufType,
           typename ReplyProtobufType,
           DBusService::HandlerFunction<RequestProtobufType,
                                        ReplyProtobufType> func>
  void HandleDBusMethod(
      std::unique_ptr<DBusMethodResponse<const ReplyProtobufType&>> response,
      const RequestProtobufType& request);

  brillo::dbus_utils::DBusObject dbus_object_;
  TpmManagerInterface* service_;
  DISALLOW_COPY_AND_ASSIGN(DBusService);
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_DBUS_SERVICE_H_
