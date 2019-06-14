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

#ifndef TPM_MANAGER_SERVER_TPM_STATUS_IMPL_H_
#define TPM_MANAGER_SERVER_TPM_STATUS_IMPL_H_

#include "tpm_manager/server/tpm_status.h"

#include <memory>
#include <vector>

#include <base/macros.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>  // NOLINT(build/include_alpha)

#include <tpm_manager/server/dbus_service.h>
#include <tpm_manager/server/tpm_connection.h>

namespace tpm_manager {

class TpmStatusImpl : public TpmStatus {
 public:
  // |ownership_taken_callback| must stay alive during the entire lifetime of
  // the TpmStatus object.
  explicit TpmStatusImpl(
      const OwnershipTakenCallBack& ownership_taken_callback);
  ~TpmStatusImpl() override = default;

  // TpmState methods.
  bool IsTpmEnabled() override;
  bool CheckAndNotifyIfTpmOwned(TpmOwnershipStatus* status) override;
  bool GetDictionaryAttackInfo(uint32_t* counter,
                               uint32_t* threshold,
                               bool* lockout,
                               uint32_t* seconds_remaining) override;
  bool GetVersionInfo(uint32_t* family,
                      uint64_t* spec_level,
                      uint32_t* manufacturer,
                      uint32_t* tpm_model,
                      uint64_t* firmware_version,
                      std::vector<uint8_t>* vendor_specific) override;

  bool TestTpmWithDefaultOwnerPassword() override;

  inline void MarkOwnerPasswordStateDirty() override {
    is_owner_password_state_dirty_ = true;
  }

 private:
  // This method refreshes the |is_owned_| and |is_enabled_| status of the
  // Tpm. It can be called multiple times.
  void RefreshOwnedEnabledInfo();
  // This method wraps calls to Tspi_TPM_GetCapability. |data| is set to
  // the raw capability data. If the optional out argument |tpm_result| is
  // provided, it is set to the result of the |Tspi_TPM_GetCapability| call.
  bool GetCapability(uint32_t capability,
                     uint32_t sub_capability,
                     std::vector<uint8_t>* data,
                     TSS_RESULT* tpm_result);

  TpmConnection tpm_connection_;
  bool is_enabled_{false};

  // Whether the TPM ownership has been taken with the default owner password.
  // Note that a true value doesn't necessary mean the entire TPM initialization
  // process has finished.
  bool is_owned_{false};

  // Whether the TPM is fully initialized.
  TpmOwnershipStatus ownership_status_{kTpmUnowned};

  bool is_enable_initialized_{false};

  // Callback function called after TPM ownership is taken.
  OwnershipTakenCallBack ownership_taken_callback_;

  // Whether we should query the TPM again with the default password or use the
  // cached result in is_owner_password_default_. We should query the TPM for
  // the first time TestTpmWithDefaultOwnerPassword is called.
  bool is_owner_password_state_dirty_ = true;

  // Whether current owner password in the TPM is the default one.
  bool is_owner_password_default_ = false;

  DISALLOW_COPY_AND_ASSIGN(TpmStatusImpl);
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_TPM_STATUS_IMPL_H_
