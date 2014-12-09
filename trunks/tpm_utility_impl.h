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
#include <gtest/gtest_prod.h>

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
  TPM_RC Clear() override;
  void Shutdown() override;
  TPM_RC InitializeTpm() override;
  TPM_RC TakeOwnership(const std::string& owner_password,
                       const std::string& endorsement_password,
                       const std::string& lockout_password) override;
  TPM_RC StirRandom(const std::string& entropy_data) override;
  TPM_RC GenerateRandom(size_t num_bytes,
                        std::string* random_data) override;
  TPM_RC ExtendPCR(int pcr_index, const std::string& extend_data) override;
  TPM_RC ReadPCR(int pcr_index, std::string* pcr_value) override;
  TPM_RC AsymmetricEncrypt(TPM_HANDLE key_handle,
                           TPM_ALG_ID scheme,
                           TPM_ALG_ID hash_alg,
                           const std::string& plaintext,
                           std::string* ciphertext) override;
  TPM_RC AsymmetricDecrypt(TPM_HANDLE key_handle,
                           TPM_ALG_ID scheme,
                           TPM_ALG_ID hash_alg,
                           const std::string& password,
                           const std::string& ciphertext,
                           std::string* plaintext) override;
  TPM_RC Sign(TPM_HANDLE key_handle,
              TPM_ALG_ID scheme,
              TPM_ALG_ID hash_alg,
              const std::string& password,
              const std::string& digest,
              std::string* signature) override;
  TPM_RC Verify(TPM_HANDLE key_handle,
                TPM_ALG_ID scheme,
                TPM_ALG_ID hash_alg,
                const std::string& digest,
                const std::string& signature) override;
  TPM_RC CreateRSAKey(AsymmetricKeyUsage key_type,
                      const std::string& password,
                      TPM_HANDLE* key_handle) override;

 protected:
  FRIEND_TEST(TpmUtilityTest, RootKeysSuccess);
  FRIEND_TEST(TpmUtilityTest, RootKeysHandleConsistency);
  FRIEND_TEST(TpmUtilityTest, RootKeysCreateFailure);
  FRIEND_TEST(TpmUtilityTest, RootKeysPersistFailure);
  FRIEND_TEST(TpmUtilityTest, SaltingKeySuccess);
  FRIEND_TEST(TpmUtilityTest, SaltingKeyConsistency);
  FRIEND_TEST(TpmUtilityTest, SaltingKeyCreateFailure);
  FRIEND_TEST(TpmUtilityTest, SaltingKeyLoadFailure);
  FRIEND_TEST(TpmUtilityTest, SaltingKeyPersistFailure);
  FRIEND_TEST(TpmUtilityTest, RootKeysSuccess);

 private:
  const TrunksFactory& factory_;
  scoped_ptr<AuthorizationSession> session_;

  // Synchronously derives storage root keys for RSA and ECC and persists the
  // keys in the TPM. This operation must be authorized by the |owner_password|
  // and, on success, KRSAStorageRootKey and kECCStorageRootKey can be used
  // with an empty authorization value until the TPM is cleared.
  TPM_RC CreateStorageRootKeys(const std::string& owner_password);

  // This method creates an RSA decryption key to be used for salting sessions.
  // This method also makes the salting key permanent under the storage
  // hierarchy.
  TPM_RC CreateSaltingKey(const std::string& owner_password);

  // This method returns a partially filled TPMT_PUBLIC strucutre,
  // which can then be modified by other methods to create the public
  // template for a key. It takes a valid |key_type| tp construct the
  // parameters.
  TPMT_PUBLIC CreateDefaultPublicArea(TPM_ALG_ID key_alg);

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

  // This function sets |name| to the name of the object referenced by
  // |handle|. This function only works on Transient and Permanent objects.
  TPM_RC GetKeyName(TPM_HANDLE handle, std::string* name);

  // This function returns the public area of a handle in the tpm.
  TPM_RC GetKeyPublicArea(TPM_HANDLE handle, TPM2B_PUBLIC* public_data);

  DISALLOW_COPY_AND_ASSIGN(TpmUtilityImpl);
};

}  // namespace trunks

#endif  // TRUNKS_TPM_UTILITY_IMPL_H_
