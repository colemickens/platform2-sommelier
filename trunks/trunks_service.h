//
// Copyright (C) 2014 The Android Open Source Project
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

#ifndef TRUNKS_TRUNKS_SERVICE_H_
#define TRUNKS_TRUNKS_SERVICE_H_

#include <string>

#include <base/memory/weak_ptr.h>
#include <chromeos/dbus/dbus_method_response.h>
#include <chromeos/dbus/dbus_object.h>

#include "trunks/dbus_interface.pb.h"
#include "trunks/tpm_handle.h"

namespace trunks {

using CompletionAction =
    chromeos::dbus_utils::AsyncEventSequencer::CompletionAction;

// TrunksService registers for and handles all incoming D-Bus messages for the
// trunksd system daemon.
class TrunksService {
 public:
  // The |transceiver| will be the target of all incoming TPM commands.
  TrunksService(const scoped_refptr<dbus::Bus>& bus,
                         CommandTransceiver* transceiver);
  virtual ~TrunksService() = default;

  // Connects to D-Bus system bus and exports Trunks methods.
  void Register(const CompletionAction& callback);

 private:
  // Handles calls to the 'SendCommand' method.
  void HandleSendCommand(
      std::unique_ptr<chromeos::dbus_utils::DBusMethodResponse<
          const SendCommandResponse&>> response_sender,
      const SendCommandRequest& request);

  base::WeakPtr<TrunksService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  chromeos::dbus_utils::DBusObject trunks_dbus_object_;
  CommandTransceiver* transceiver_;

  // Declared last so weak pointers are invalidated first on destruction.
  base::WeakPtrFactory<TrunksService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TrunksService);
};

}  // namespace trunks


#endif  // TRUNKS_TRUNKS_SERVICE_H_
