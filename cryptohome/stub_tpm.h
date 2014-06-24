// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdio.h>
#include <unistd.h>

#include "tpm.h"

namespace cryptohome {

class StubTpm : public Tpm {
 public:
  StubTpm() { }
  virtual ~StubTpm() { }

  // See tpm.h for comments
  virtual bool Connect(TpmRetryAction* retry_action) { return false; }
  virtual bool IsConnected() { return false; }
  virtual bool IsEnabled() const { return false; }
  virtual bool IsOwned() const { return false; }
  virtual bool ReadNvram(uint32_t index,
                         chromeos::SecureBlob* blob) { return false; }
  virtual bool IsNvramDefined(uint32_t index) { return false; }
  virtual bool IsNvramLocked(uint32_t index) { return false; }
  virtual unsigned int GetNvramSize(uint32_t index) { return 0; }
  virtual void Disconnect() { }
  virtual bool GetOwnerPassword(
    chromeos::Blob* owner_password) { return false; }
  virtual bool Encrypt(const chromeos::SecureBlob& plaintext,
                       const chromeos::SecureBlob& key,
                       chromeos::SecureBlob* ciphertext,
                       TpmRetryAction* retry_action) { return false; }
  virtual bool Decrypt(const chromeos::SecureBlob& ciphertext,
                       const chromeos::SecureBlob& key,
                       chromeos::SecureBlob* plaintext,
                       TpmRetryAction* retry_action) { return false; }
  bool IsBeingOwned() const { return false; }
  bool InitializeTpm(bool* OUT_took_ownership) { return false; }
  virtual bool GetRandomData(size_t length,
                             chromeos::Blob* data) { return false; }
  virtual bool DefineLockOnceNvram(uint32_t index,
                                   size_t length) { return false; }
  virtual bool DestroyNvram(uint32_t index) { return false; }
  virtual bool WriteNvram(uint32_t index,
                          const chromeos::SecureBlob& blob) { return false; }
  virtual bool GetEndorsementPublicKey(chromeos::SecureBlob* ek_public_key)
    { return false; }
  virtual bool GetEndorsementCredential(chromeos::SecureBlob* credential)
    { return false; }
  virtual bool MakeIdentity(chromeos::SecureBlob* identity_public_key_der,
                            chromeos::SecureBlob* identity_public_key,
                            chromeos::SecureBlob* identity_key_blob,
                            chromeos::SecureBlob* identity_binding,
                            chromeos::SecureBlob* identity_label,
                            chromeos::SecureBlob* pca_public_key,
                            chromeos::SecureBlob* endorsement_credential,
                            chromeos::SecureBlob* platform_credential,
                            chromeos::SecureBlob* conformance_credential)
    { return false; }
  virtual bool QuotePCR0(const chromeos::SecureBlob& identity_key_blob,
                         const chromeos::SecureBlob& external_data,
                         chromeos::SecureBlob* pcr_value,
                         chromeos::SecureBlob* quoted_data,
                         chromeos::SecureBlob* quote) { return false; }
  virtual bool SealToPCR0(const chromeos::Blob& value,
                          chromeos::Blob* sealed_value) { return false; }
  virtual bool Unseal(const chromeos::Blob& sealed_value,
                      chromeos::Blob* value) { return false; }
  virtual bool CreateCertifiedKey(
      const chromeos::SecureBlob& identity_key_blob,
      const chromeos::SecureBlob& external_data,
      chromeos::SecureBlob* certified_public_key,
      chromeos::SecureBlob* certified_public_key_der,
      chromeos::SecureBlob* certified_key_blob,
      chromeos::SecureBlob* certified_key_info,
      chromeos::SecureBlob* certified_key_proof) { return false; }
  virtual bool CreateDelegate(const chromeos::SecureBlob& identity_key_blob,
                              chromeos::SecureBlob* delegate_blob,
                              chromeos::SecureBlob* delegate_secret)
    { return false; }
  virtual bool ActivateIdentity(const chromeos::SecureBlob& delegate_blob,
                                const chromeos::SecureBlob& delegate_secret,
                                const chromeos::SecureBlob& identity_key_blob,
                                const chromeos::SecureBlob& encrypted_asym_ca,
                                const chromeos::SecureBlob& encrypted_sym_ca,
                                chromeos::SecureBlob* identity_credential)
    { return false; }
  virtual bool TssCompatibleEncrypt(const chromeos::SecureBlob& key,
                                    const chromeos::SecureBlob& input,
                                    chromeos::SecureBlob* output)
    { return false; }
  virtual bool TpmCompatibleOAEPEncrypt(RSA* key,
                                        const chromeos::SecureBlob& input,
                                        chromeos::SecureBlob* output)
    { return false; }
};

}  // namespace cryptohome
