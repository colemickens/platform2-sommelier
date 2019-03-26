// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_TPM_NEW_IMPL_H_
#define CRYPTOHOME_TPM_NEW_IMPL_H_

#include <tpm_manager/client/tpm_manager_utility.h>

#include "cryptohome/tpm_impl.h"

namespace cryptohome {

// |TpmNewImpl| is derived from |TpmImpl| and refines a set of interfaces with
// the data coming from |tpm_managerd|. In particular, the logic which should
// belong to only |tpm_managerd| (e.g. the ownership operation, owner password,
// etc.) are overwritted in this class and the corresponding setters should take
// no effect. Once |ServiceMonolithic| is obsoleted, the implementation of this
// class should be backported to |TpmImpl| and this class should be removed at
// the same time.
class TpmNewImpl : public TpmImpl {
 public:
  TpmNewImpl() = default;
  virtual ~TpmNewImpl() = default;
  bool GetOwnerPassword(brillo::SecureBlob* owner_password) override;
  void SetOwnerPassword(const brillo::SecureBlob& owner_password) override;
  bool IsEnabled() override;
  void SetIsEnabled(bool enabled) override;
  bool IsOwned() override;
  void SetIsOwned(bool owned) override;
  bool TakeOwnership(int max_timeout_tries,
                     const brillo::SecureBlob& owner_password) override;

 private:
  // Initializes |tpm_manager_utility_|; returns |true| iff successful.
  bool InitializeTpmManagerUtility();

  // Calls |TpmManagerUtility::GetTpmStatus| and stores the result into
  // |is_enabled_|, |is_owned_|, and |last_tpm_manager_data_| for later use.
  bool CacheTpmManagerStatus();

  //  wrapped tpm_manager proxy to get information from |tpm_manager|.
  tpm_manager::TpmManagerUtility tpm_manager_utility_;

  // give |TpmNewImpl| a new set of members of status from tpm manager so we can
  // touch the already working code as little as possible. Otherwise need to
  // move data members in |TpmImpl| to |protected| field.
  bool is_enabled_{false};
  bool is_owned_{false};

  tpm_manager::LocalData last_tpm_manager_data_;

  DISALLOW_COPY_AND_ASSIGN(TpmNewImpl);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_TPM_NEW_IMPL_H_
