// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TPM_UTILITY_IMPL_H_
#define TRUNKS_TPM_UTILITY_IMPL_H_

#include "trunks/tpm_utility.h"

#include <string>

#include <base/macros.h>
#include <chromeos/chromeos_export.h>

#include "trunks/null_authorization_delegate.h"

namespace trunks {

class AuthorizationDelegate;
class TrunksFactory;

// A default implementation of TpmUtility.
class CHROMEOS_EXPORT TpmUtilityImpl : public TpmUtility {
 public:
  explicit TpmUtilityImpl(const TrunksFactory& factory);
  virtual ~TpmUtilityImpl();

  // TpmUtility methods.
  TPM_RC Startup() override;
  TPM_RC InitializeTpm() override;
  TPM_RC StirRandom(const std::string& entropy_data) override;
  TPM_RC GenerateRandom(int num_bytes,
                        std::string* random_data) override;

 private:
  const TrunksFactory& factory_;

  // Sets TPM platformAuth to |password|.
  TPM_RC SetPlatformAuthorization(const std::string& password);

  // Disables the TPM platform hierarchy until the next startup. This requires
  // platform |authorization|.
  TPM_RC DisablePlatformHierarchy(AuthorizationDelegate* authorization);

  DISALLOW_COPY_AND_ASSIGN(TpmUtilityImpl);
};

}  // namespace trunks

#endif  // TRUNKS_TPM_UTILITY_IMPL_H_
