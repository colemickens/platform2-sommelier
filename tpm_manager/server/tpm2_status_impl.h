// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_SERVER_TPM2_STATUS_IMPL_H_
#define TPM_MANAGER_SERVER_TPM2_STATUS_IMPL_H_

#include "tpm_manager/server/tpm_status.h"

#include <memory>

#include <base/macros.h>
#include <trunks/tpm_state.h>
#include <trunks/trunks_factory.h>

namespace tpm_manager {

class Tpm2StatusImpl : public TpmStatus {
 public:
  Tpm2StatusImpl();
  // Does not take ownership of |factory|.
  explicit Tpm2StatusImpl(trunks::TrunksFactory* factory);
  ~Tpm2StatusImpl() override = default;

  // TpmState methods.
  bool IsTpmEnabled() override;
  bool IsTpmOwned() override;
  bool GetDictionaryAttackInfo(int* counter,
                               int* threshold,
                               bool* lockout,
                               int* seconds_remaining) override;

 private:
  // Refreshes the Tpm state information. Can be called as many times as needed
  // to refresh the cached information in this class. Return true if the
  // refresh operation succeeded.
  bool Refresh();

  bool initialized_{false};
  bool is_owned_{false};
  std::unique_ptr<trunks::TrunksFactory> default_trunks_factory_;
  trunks::TrunksFactory* trunks_factory_;
  scoped_ptr<trunks::TpmState> trunks_tpm_state_;

  DISALLOW_COPY_AND_ASSIGN(Tpm2StatusImpl);
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_TPM2_STATUS_IMPL_H_
