// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TPM_UTILITY_IMPL_H_
#define TRUNKS_TPM_UTILITY_IMPL_H_

#include "trunks/tpm_utility.h"

#include <string>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/chromeos_export.h>

#include "trunks/null_authorization_delegate.h"

namespace trunks {

class AuthorizationDelegate;
class AuthorizationSession;
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
  TPM_RC ExtendPCR(int pcr_index, const std::string& extend_data) override;
  TPM_RC ReadPCR(int pcr_index, std::string* pcr_value) override;
  TPM_RC TakeOwnership(const std::string& owner_password,
                       const std::string& endorsement_password,
                       const std::string& lockout_password) override;
  TPM_RC CreateStorageRootKeys(const std::string& owner_password) override;

 private:
  const TrunksFactory& factory_;
  scoped_ptr<AuthorizationSession> session_;

  // If session_ has not been initialized, creates an unbound and salted
  // authorization session with encryption enabled and assigns it to session_.
  // If session_ has already been initialized, this method has no effect. Call
  // this method successfully before accessing session_.
  TPM_RC InitializeSession();

  // Sets TPM |hierarchy| authorization to |password| using |authorization|.
  TPM_RC SetHierarchyAuthorization(TPMI_RH_HIERARCHY_AUTH hierarchy,
                                   const std::string& password,
                                   AuthorizationDelegate* authorization);

  // Disables the TPM platform hierarchy until the next startup. This requires
  // platform |authorization|.
  TPM_RC DisablePlatformHierarchy(AuthorizationDelegate* authorization);

  DISALLOW_COPY_AND_ASSIGN(TpmUtilityImpl);
};

}  // namespace trunks

#endif  // TRUNKS_TPM_UTILITY_IMPL_H_
