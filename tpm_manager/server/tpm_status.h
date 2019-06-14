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

#ifndef TPM_MANAGER_SERVER_TPM_STATUS_H_
#define TPM_MANAGER_SERVER_TPM_STATUS_H_

#include <stdint.h>

#include <vector>

namespace tpm_manager {

// TpmStatus is an interface class that reports status information for some kind
// of TPM device.
class TpmStatus {
 public:
  enum TpmOwnershipStatus {
    // TPM is not owned. The owner password is empty.
    kTpmUnowned = 0,

    // TPM is pre-owned. The owner password is set to a well-known password, but
    // TPM initialization is not completed yet.
    kTpmPreOwned,

    // TPM initialization is completed. The owner password is set to a randomly-
    // generated password.
    kTpmOwned,
  };

  TpmStatus() = default;
  virtual ~TpmStatus() = default;

  // Returns true iff the TPM is enabled.
  virtual bool IsTpmEnabled() = 0;

  // Gets current TPM ownership status and stores it in |status|. The status
  // will be kTpmOwned iff the entire TPM initialization process has finished,
  // including all the password set up.
  //
  // Sends out a signal to the dbus if the TPM state is changed to owned from a
  // different state.
  //
  // Returns whether the operation is successful or not.
  virtual bool CheckAndNotifyIfTpmOwned(TpmOwnershipStatus* status) = 0;

  // Reports the current state of the TPM dictionary attack logic.
  virtual bool GetDictionaryAttackInfo(uint32_t* counter,
                                       uint32_t* threshold,
                                       bool* lockout,
                                       uint32_t* seconds_remaining) = 0;

  // Get TPM hardware and software version information.
  virtual bool GetVersionInfo(uint32_t* family,
                              uint64_t* spec_level,
                              uint32_t* manufacturer,
                              uint32_t* tpm_model,
                              uint64_t* firmware_version,
                              std::vector<uint8_t>* vendor_specific) = 0;

  // We cache the state whether current TPM owner password is the default
  // password to avoid unnecessary TPM queries. Marking the cache dirty will
  // force a new TPM query and cache update in the next time
  // TestTpmWithDefaultOwnerPassword() is called.
  //
  // NOTE: This method should be used by TPM 1.2 only.
  virtual void MarkOwnerPasswordStateDirty() = 0;

  // This method return true iff the default password is the current owner
  // password in the TPM. This method can also return false if there was an
  // error communicating with the TPM.
  //
  // NOTE: This method should be used by TPM 1.2 only.
  virtual bool TestTpmWithDefaultOwnerPassword() = 0;
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_TPM_STATUS_H_
