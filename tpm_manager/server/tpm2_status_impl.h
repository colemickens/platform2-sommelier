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

#ifndef TPM_MANAGER_SERVER_TPM2_STATUS_IMPL_H_
#define TPM_MANAGER_SERVER_TPM2_STATUS_IMPL_H_

#include "tpm_manager/server/tpm_status.h"

#include <memory>
#include <vector>

#include <base/logging.h>
#include <base/macros.h>
#include <tpm_manager/server/dbus_service.h>
#include <trunks/tpm_state.h>
#include <trunks/trunks_factory.h>

namespace tpm_manager {

class Tpm2StatusImpl : public TpmStatus {
 public:
  // Does not take ownership of |factory|.
  //
  // |ownership_taken_callback| must stay alive during the entire lifetime of
  // the TpmStatus object.
  explicit Tpm2StatusImpl(
      const trunks::TrunksFactory& factory,
      const OwnershipTakenCallBack& ownership_taken_callback);
  ~Tpm2StatusImpl() override = default;

  // TpmStatus methods.
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

  inline void MarkOwnerPasswordStateDirty() override {
    LOG(ERROR) << __func__ << ": Not implemented";
  }

  bool TestTpmWithDefaultOwnerPassword() override {
    LOG(ERROR) << __func__ << ": Not implemented";
    return false;
  }

 private:
  // Refreshes the Tpm state information. Can be called as many times as needed
  // to refresh the cached information in this class. Return true if the
  // refresh operation succeeded.
  bool Refresh();

  bool initialized_{false};
  TpmOwnershipStatus ownership_status_{kTpmUnowned};
  const trunks::TrunksFactory& trunks_factory_;
  std::unique_ptr<trunks::TpmState> trunks_tpm_state_;

  // Callback function called after TPM ownership is taken.
  OwnershipTakenCallBack ownership_taken_callback_;

  DISALLOW_COPY_AND_ASSIGN(Tpm2StatusImpl);
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_TPM2_STATUS_IMPL_H_
