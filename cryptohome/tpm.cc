// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Tpm

#include "tpm.h"

#include <base/platform_thread.h>
#include <base/time.h>
#include <openssl/rsa.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>

#include "crypto.h"

namespace cryptohome {

const unsigned char kDefaultSrkAuth[] = { };
const TSS_UUID kCryptohomeWellKnownUuid = {0x0203040b, 0, 0, 0, 0,
                                            {0, 9, 8, 1, 0, 3}};
const int kDefaultTpmRsaKeyBits = 2048;
const int kDefaultDiscardableWrapPasswordLength = 32;

Tpm::Tpm()
    : rsa_key_bits_(kDefaultTpmRsaKeyBits),
      srk_auth_(kDefaultSrkAuth, sizeof(kDefaultSrkAuth)),
      crypto_(NULL),
      context_handle_(0),
      key_handle_(0) {
  set_well_known_uuid(kCryptohomeWellKnownUuid);
}

Tpm::~Tpm() {
  Disconnect();
}

bool Tpm::Init(Crypto* crypto, bool open_key) {
  crypto_ = crypto;

  if (open_key && key_handle_ == 0) {
    return Connect();
  }

  return true;
}

bool Tpm::Connect() {
  if (key_handle_ == 0) {
    TSS_HCONTEXT context_handle;
    if (!OpenAndConnectTpm(&context_handle)) {
      return false;
    }

    TSS_HKEY key_handle;
    if (!LoadOrCreateCryptohomeKey(context_handle, well_known_uuid_, false,
                                   true, &key_handle)) {
      Tspi_Context_Close(context_handle);
      return false;
    }

    key_handle_ = key_handle;
    context_handle_ = context_handle;
  }

  return true;
}

bool Tpm::IsConnected() const {
  return (key_handle_ != 0);
}

void Tpm::Disconnect() {
  if (key_handle_) {
    Tspi_Context_CloseObject(context_handle_, key_handle_);
    key_handle_ = 0;
  }
  if (context_handle_) {
    Tspi_Context_Close(context_handle_);
    context_handle_ = 0;
  }
}

bool Tpm::RemoveCryptohomeKey(TSS_HCONTEXT context_handle,
                              const TSS_UUID& key_uuid) const {
  TSS_RESULT result;
  TSS_HKEY key_handle;
  if ((result = Tspi_Context_UnregisterKey(context_handle, TSS_PS_TYPE_SYSTEM,
                                           key_uuid, &key_handle))) {
    LOG(ERROR) << "Error calling Tspi_UnregisterKey: " << result;
    return false;
  }
  Tspi_Context_CloseObject(context_handle, key_handle);
  return true;
}

bool Tpm::LoadOrCreateCryptohomeKey(TSS_HCONTEXT context_handle,
                                    const TSS_UUID& key_uuid,
                                    bool create_in_tpm, bool register_key,
                                    TSS_HKEY* key_handle) const {
  TSS_RESULT result;

  // First try loading the key by the UUID
  if ((result = Tspi_Context_LoadKeyByUUID(context_handle, TSS_PS_TYPE_SYSTEM,
                                           key_uuid, key_handle))
      == TSS_SUCCESS) {
    return true;
  }

  // Otherwise, we need to create the key.  First, create an object.
  TSS_FLAG init_flags = TSS_KEY_TYPE_LEGACY | TSS_KEY_VOLATILE;
  if (!create_in_tpm) {
    init_flags |= TSS_KEY_MIGRATABLE;
    switch(rsa_key_bits_) {
      case 2048:
        init_flags |= TSS_KEY_SIZE_2048;
        break;
      case 1024:
        init_flags |= TSS_KEY_SIZE_1024;
        break;
      case 512:
        init_flags |= TSS_KEY_SIZE_512;
        break;
      default:
        LOG(INFO) << "Key size is unknown.";
        return false;
    }
  }
  TSS_HKEY local_key_handle;
  if ((result = Tspi_Context_CreateObject(context_handle,
                                TSS_OBJECT_TYPE_RSAKEY,
                                init_flags, &local_key_handle))) {
    LOG(ERROR) << "Error calling Tspi_Context_CreateObject";
    return false;
  }

  // Set the attributes
  UINT32 sig_scheme = TSS_SS_RSASSAPKCS1V15_DER;
  if ((result = Tspi_SetAttribUint32(local_key_handle, TSS_TSPATTRIB_KEY_INFO,
                           TSS_TSPATTRIB_KEYINFO_SIGSCHEME,
                           sig_scheme))) {
    LOG(ERROR) << "Error calling Tspi_SetAttribUint32";
    Tspi_Context_CloseObject(context_handle, local_key_handle);
    return false;
  }

  UINT32 enc_scheme = TSS_ES_RSAESPKCSV15;
  if ((result = Tspi_SetAttribUint32(local_key_handle, TSS_TSPATTRIB_KEY_INFO,
                           TSS_TSPATTRIB_KEYINFO_ENCSCHEME,
                           enc_scheme))) {
    LOG(ERROR) << "Error calling Tspi_SetAttribUint32";
    Tspi_Context_CloseObject(context_handle, local_key_handle);
    return false;
  }

  // Load the Storage Root Key
  TSS_HKEY srk_handle;
  if (!LoadSrk(context_handle, &srk_handle)) {
    Tspi_Context_CloseObject(context_handle, local_key_handle);
    return false;
  }

  // Create a new system-wide key for cryptohome
  if (create_in_tpm) {
    if ((result = Tspi_Key_CreateKey(local_key_handle, srk_handle, 0))) {
      LOG(ERROR) << "Error calling Tspi_Key_CreateKey: " << result;
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }
  } else {
    unsigned int size_n;
    BYTE *public_srk;
    if ((result = Tspi_Key_GetPubKey(srk_handle, &size_n, &public_srk))) {
      LOG(ERROR) << "Error calling Tspi_Key_GetPubKey";
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }
    Tspi_Context_FreeMemory(context_handle, public_srk);

    TSS_HPOLICY policy_handle;
    if ((result = Tspi_Context_CreateObject(context_handle,
                                            TSS_OBJECT_TYPE_POLICY,
                                            TSS_POLICY_MIGRATION,
                                            &policy_handle))) {
      LOG(ERROR) << "Error creating policy object: " << result;
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }

    // Set a random migration policy password, and discard it.  The key will not
    // be migrated, but to create the key outside of the TPM, we have to do it
    // this way.
    SecureBlob migration_password(kDefaultDiscardableWrapPasswordLength);
    crypto_->GetSecureRandom(
        static_cast<unsigned char*>(migration_password.data()),
        migration_password.size());
    if ((result = Tspi_Policy_SetSecret(policy_handle, TSS_SECRET_MODE_PLAIN,
        migration_password.size(),
        static_cast<BYTE*>(migration_password.data())))) {
      LOG(ERROR) << "Error setting migration policy password: " << result;
      Tspi_Context_CloseObject(context_handle, policy_handle);
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }

    if ((result = Tspi_Policy_AssignToObject(policy_handle,
                                             local_key_handle))) {
      LOG(ERROR) << "Error assigning migration policy: " << result;
      // TODO(fes): Close policy handle
      Tspi_Context_CloseObject(context_handle, policy_handle);
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }

    SecureBlob n;
    SecureBlob p;
    if (!crypto_->CreateRsaKey(rsa_key_bits_, &n, &p)) {
      LOG(ERROR) << "Error creating RSA key";
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }

    if ((result = Tspi_SetAttribData(local_key_handle,
                                     TSS_TSPATTRIB_RSAKEY_INFO,
                                     TSS_TSPATTRIB_KEYINFO_RSA_MODULUS,
                                     n.size(),
                                     static_cast<BYTE *>(n.data())))) {
      LOG(ERROR) << "Error setting RSA modulus";
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }

    if ((result = Tspi_SetAttribData(local_key_handle, TSS_TSPATTRIB_KEY_BLOB,
                                     TSS_TSPATTRIB_KEYBLOB_PRIVATE_KEY,
                                     p.size(),
                                     static_cast<BYTE *>(p.data())))) {
      LOG(ERROR) << "Error setting private key";
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }

    if ((result = Tspi_Key_WrapKey(local_key_handle, srk_handle, 0))) {
      LOG(ERROR) << "Error wrapping RSA key: " << result;
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }
  }

  if (register_key) {
    // Register the key so we can look it up by UUID
    TSS_UUID SRK_UUID = TSS_UUID_SRK;
    if ((result = Tspi_Context_RegisterKey(context_handle, local_key_handle,
                                           TSS_PS_TYPE_SYSTEM, key_uuid,
                                           TSS_PS_TYPE_SYSTEM, SRK_UUID))) {
      LOG(ERROR) << "Error calling Tspi_RegisterKey: " << result;
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }
  }

  if ((result = Tspi_Key_LoadKey(local_key_handle, srk_handle))) {
    LOG(ERROR) << "Error calling Tspi_Key_LoadKey: " << result;
    Tspi_Context_CloseObject(context_handle, srk_handle);
    Tspi_Context_CloseObject(context_handle, local_key_handle);
    return false;
  }

  Tspi_Context_CloseObject(context_handle, srk_handle);

  *key_handle = local_key_handle;
  return true;
}

int Tpm::GetMaxRsaKeyCount() const {
  if (context_handle_ == 0) {
    return -1;
  }

  return GetMaxRsaKeyCountForContext(context_handle_);
}

int Tpm::GetMaxRsaKeyCountForContext(TSS_HCONTEXT context_handle) const {
  int count = -1;
  TSS_RESULT result;
  TSS_HTPM tpm_handle;
  if ((result = Tspi_Context_GetTpmObject(context_handle, &tpm_handle))) {
    LOG(ERROR) << "Error calling Tspi_Context_GetTpmObject: " << result;
    return count;
  }

  UINT32 cap_length = 0;
  BYTE* cap = NULL;
  UINT32 subcap = TSS_TPMCAP_PROP_MAXKEYS;
  if ((result = Tspi_TPM_GetCapability(tpm_handle, TSS_TPMCAP_PROPERTY,
                                       sizeof(subcap),
                                       reinterpret_cast<BYTE*>(&subcap),
                                       &cap_length, &cap))) {
    LOG(ERROR) << "Error calling Tspi_TPM_GetCapability: " << result;
    return count;
  }
  if (cap_length == sizeof(int)) {
    count = *(reinterpret_cast<int*>(cap));
  }
  Tspi_Context_FreeMemory(context_handle, cap);
  return count;
}

bool Tpm::OpenAndConnectTpm(TSS_HCONTEXT* context_handle) const {
  TSS_RESULT result;
  TSS_HCONTEXT local_context_handle;
  if ((result = Tspi_Context_Create(&local_context_handle))) {
    LOG(ERROR) << "Error calling Tspi_Context_Create";
    return false;
  }

  for (int i = 0; i < 5; i++) {
    if ((result = Tspi_Context_Connect(local_context_handle, NULL))) {
      if (result == TSS_E_COMM_FAILURE) {
        PlatformThread::Sleep(100);
      } else {
        LOG(ERROR) << "Error calling Tspi_Context_Connect: " << result;
        Tspi_Context_Close(local_context_handle);
        return false;
      }
    } else {
      break;
    }
  }

  if (result) {
    LOG(ERROR) << "Error calling Tspi_Context_Connect: " << result;
    Tspi_Context_Close(local_context_handle);
    return false;
  }

  *context_handle = local_context_handle;
  return true;
}

bool Tpm::Encrypt(const chromeos::Blob& data, const chromeos::Blob& password,
                  const chromeos::Blob& salt, SecureBlob* data_out) const {
  if (key_handle_ == 0 || context_handle_ == 0) {
    return false;
  }

  return EncryptBlob(context_handle_, key_handle_, data, password, salt,
                     data_out);
}

bool Tpm::Decrypt(const chromeos::Blob& data, const chromeos::Blob& password,
                  const chromeos::Blob& salt, SecureBlob* data_out) const {
  if (key_handle_ == 0 || context_handle_ == 0) {
    return false;
  }

  return DecryptBlob(context_handle_, key_handle_, data, password, salt,
                     data_out);
}

bool Tpm::GetKey(SecureBlob* blob) const {
  if (key_handle_ == 0 || context_handle_ == 0) {
    return false;
  }

  return GetKeyBlob(context_handle_, key_handle_, blob);
}

bool Tpm::LoadKey(const SecureBlob& blob) {
  if (context_handle_ == 0) {
    return false;
  }

  TSS_HKEY local_key_handle;

  if (!LoadKeyBlob(context_handle_, blob, &local_key_handle)) {
    return false;
  }

  TSS_HKEY srk_handle;
  if (!LoadSrk(context_handle_, &srk_handle)) {
    Tspi_Context_CloseObject(context_handle_, local_key_handle);
    return false;
  }

  TSS_RESULT result;
  if ((result = Tspi_Key_LoadKey(local_key_handle, srk_handle))) {
    LOG(ERROR) << "Error calling Tspi_Key_LoadKey: " << result;
    Tspi_Context_CloseObject(context_handle_, local_key_handle);
    Tspi_Context_CloseObject(context_handle_, srk_handle);
    return false;
  }
  Tspi_Context_CloseObject(context_handle_, srk_handle);

  if (key_handle_ != 0) {
    Tspi_Context_CloseObject(context_handle_, key_handle_);
  }
  key_handle_ = local_key_handle;

  return true;
}

bool Tpm::EncryptBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                      const chromeos::Blob& data,
                      const chromeos::Blob& password,
                      const chromeos::Blob& salt, SecureBlob* data_out) const {
  TSS_RESULT result;

  TSS_FLAG init_flags = TSS_ENCDATA_SEAL;
  TSS_HKEY enc_handle;
  if ((result = Tspi_Context_CreateObject(context_handle,
                                          TSS_OBJECT_TYPE_ENCDATA,
                                          init_flags, &enc_handle))) {
    LOG(ERROR) << "Error calling Tspi_Context_CreateObject";
    return false;
  }

  // TODO(fes): Check RSA key modulus size, return an error or block input
  if ((result = Tspi_Data_Bind(enc_handle, key_handle, data.size(),
      const_cast<BYTE *>(&data[0])))) {
    LOG(ERROR) << "Error calling Tspi_Data_Bind: " << result;
    Tspi_Context_CloseObject(context_handle, enc_handle);
    return false;
  }

  unsigned char* enc_data = NULL;
  UINT32 enc_data_length = 0;
  if ((result = Tspi_GetAttribData(enc_handle, TSS_TSPATTRIB_ENCDATA_BLOB,
                                   TSS_TSPATTRIB_ENCDATABLOB_BLOB,
                                   &enc_data_length, &enc_data))) {
    LOG(ERROR) << "Error calling Tspi_GetAttribData";
    Tspi_Context_CloseObject(context_handle, enc_handle);
    return false;
  }

  SecureBlob local_data(enc_data_length);
  memcpy(local_data.data(), enc_data, enc_data_length);
  Tspi_Context_FreeMemory(context_handle, enc_data);
  Tspi_Context_CloseObject(context_handle, enc_handle);

  SecureBlob aes_key;
  SecureBlob iv;
  if (!crypto_->PasskeyToAesKey(password, salt, &aes_key, &iv)) {
    LOG(ERROR) << "Failure converting passkey to key";
    return false;
  }

  unsigned int aes_block_size = crypto_->GetAesBlockSize();
  unsigned int to_wrap = local_data.size()
      - (local_data.size() % aes_block_size);

  SecureBlob local_data_out;
  if (!crypto_->WrapAes(local_data, 0, to_wrap, aes_key, iv,
                        Crypto::PADDING_NONE, &local_data_out)) {
    LOG(ERROR) << "AES encryption failed.";
    return false;
  }
  if (local_data.size() > to_wrap) {
    int additional = local_data.size() - to_wrap;
    local_data_out.resize(local_data_out.size() + additional);
    memcpy(&local_data_out[to_wrap], &local_data[to_wrap], additional);
  }

  data_out->swap(local_data_out);
  return true;
}

bool Tpm::DecryptBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                      const chromeos::Blob& data,
                      const chromeos::Blob& password,
                      const chromeos::Blob& salt, SecureBlob* data_out) const {
  SecureBlob aes_key;
  SecureBlob iv;
  if (!crypto_->PasskeyToAesKey(password, salt, &aes_key, &iv)) {
    LOG(ERROR) << "Failure converting passkey to key";
    return false;
  }

  unsigned int aes_block_size = crypto_->GetAesBlockSize();
  unsigned int to_unwrap = data.size() - (data.size() % aes_block_size);

  SecureBlob local_data;
  if (!crypto_->UnwrapAes(data, 0, to_unwrap, aes_key, iv, Crypto::PADDING_NONE,
                          &local_data)) {
    LOG(ERROR) << "AES decryption failed.";
    return false;
  }
  if (data.size() > to_unwrap) {
    local_data.resize(local_data.size() + (data.size() - to_unwrap));
    memcpy(&local_data[to_unwrap], &data[to_unwrap], (data.size() - to_unwrap));
  }

  TSS_RESULT result;
  TSS_FLAG init_flags = TSS_ENCDATA_SEAL;
  TSS_HKEY enc_handle;
  if ((result = Tspi_Context_CreateObject(context_handle,
                                          TSS_OBJECT_TYPE_ENCDATA,
                                          init_flags, &enc_handle))) {
    LOG(ERROR) << "Error calling Tspi_Context_CreateObject";
    return false;
  }

  if ((result = Tspi_SetAttribData(enc_handle,
      TSS_TSPATTRIB_ENCDATA_BLOB, TSS_TSPATTRIB_ENCDATABLOB_BLOB,
      local_data.size(),
      static_cast<BYTE *>(const_cast<void*>(local_data.const_data()))))) {
    LOG(ERROR) << "Error calling Tspi_SetAttribData";
    Tspi_Context_CloseObject(context_handle, enc_handle);
    return false;
  }

  unsigned char* dec_data = NULL;
  UINT32 dec_data_length = 0;
  if ((result = Tspi_Data_Unbind(enc_handle, key_handle, &dec_data_length,
                                 &dec_data))) {
    LOG(ERROR) << "Error calling Tspi_Data_Unbind: " << result;
    Tspi_Context_CloseObject(context_handle, enc_handle);
    return false;
  }
  data_out->resize(dec_data_length);
  memcpy(data_out->data(), dec_data, dec_data_length);
  chromeos::SecureMemset(dec_data, 0, dec_data_length);
  Tspi_Context_FreeMemory(context_handle, dec_data);

  Tspi_Context_CloseObject(context_handle, enc_handle);

  return true;
}

bool Tpm::GetKeyBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                     SecureBlob* data_out) const {
  TSS_RESULT result;
  BYTE* blob;
  UINT32 blob_size;
  if ((result = Tspi_GetAttribData(key_handle, TSS_TSPATTRIB_KEY_BLOB,
                                   TSS_TSPATTRIB_KEYBLOB_BLOB,
                                   &blob_size, &blob))) {
    LOG(ERROR) << "Couldn't get key blob";
    return false;
  }

  SecureBlob local_data(blob_size);
  memcpy(local_data.data(), blob, blob_size);
  chromeos::SecureMemset(blob, 0, blob_size);
  Tspi_Context_FreeMemory(context_handle, blob);
  data_out->swap(local_data);
  return true;
}

bool Tpm::LoadKeyBlob(TSS_HCONTEXT context_handle, const SecureBlob& blob,
                      TSS_HKEY* key_handle) const {
  TSS_RESULT result;
  TSS_HKEY local_key_handle;
  TSS_FLAG init_flags = TSS_KEY_TYPE_LEGACY | TSS_KEY_VOLATILE;
  if ((result = Tspi_Context_CreateObject(context_handle,
                                TSS_OBJECT_TYPE_RSAKEY,
                                init_flags, &local_key_handle))) {
    LOG(ERROR) << "Error calling Tspi_Context_CreateObject";
    return false;
  }

  // Set the attributes
  UINT32 sig_scheme = TSS_SS_RSASSAPKCS1V15_DER;
  if ((result = Tspi_SetAttribUint32(local_key_handle, TSS_TSPATTRIB_KEY_INFO,
                           TSS_TSPATTRIB_KEYINFO_SIGSCHEME,
                           sig_scheme))) {
    LOG(ERROR) << "Error calling Tspi_SetAttribUint32";
    Tspi_Context_CloseObject(context_handle, local_key_handle);
    return false;
  }

  UINT32 enc_scheme = TSS_ES_RSAESPKCSV15;
  if ((result = Tspi_SetAttribUint32(local_key_handle, TSS_TSPATTRIB_KEY_INFO,
                           TSS_TSPATTRIB_KEYINFO_ENCSCHEME,
                           enc_scheme))) {
    LOG(ERROR) << "Error calling Tspi_SetAttribUint32";
    Tspi_Context_CloseObject(context_handle, local_key_handle);
    return false;
  }

  if ((result = Tspi_SetAttribData(local_key_handle, TSS_TSPATTRIB_KEY_BLOB,
      TSS_TSPATTRIB_KEYBLOB_BLOB,
      blob.size(),
      static_cast<BYTE*>(const_cast<void*>(blob.const_data()))))) {
    LOG(ERROR) << "Couldn't set key blob";
    Tspi_Context_CloseObject(context_handle, local_key_handle);
    return false;
  }

  *key_handle = local_key_handle;
  return true;
}

bool Tpm::LoadSrk(TSS_HCONTEXT context_handle, TSS_HKEY* srk_handle) const {
  // Load the Storage Root Key
  TSS_RESULT result;
  TSS_UUID SRK_UUID = TSS_UUID_SRK;
  TSS_HKEY local_srk_handle;
  if ((result = Tspi_Context_LoadKeyByUUID(context_handle, TSS_PS_TYPE_SYSTEM,
                                           SRK_UUID, &local_srk_handle))) {
    return false;
  }

  // Check if the SRK wants a password
  UINT32 srk_authusage;
  if ((result = Tspi_GetAttribUint32(local_srk_handle, TSS_TSPATTRIB_KEY_INFO,
                                     TSS_TSPATTRIB_KEYINFO_AUTHUSAGE,
                                     &srk_authusage))) {
    Tspi_Context_CloseObject(context_handle, local_srk_handle);
    return false;
  }

  // Give it the password if needed
  if (srk_authusage) {
    TSS_HPOLICY srk_usage_policy;
    if ((result = Tspi_GetPolicyObject(local_srk_handle, TSS_POLICY_USAGE,
                                       &srk_usage_policy))) {
      Tspi_Context_CloseObject(context_handle, local_srk_handle);
      return false;
    }

    if ((result = Tspi_Policy_SetSecret(srk_usage_policy,
        TSS_SECRET_MODE_PLAIN,
        srk_auth_.size(),
        const_cast<BYTE *>(static_cast<const BYTE *>(
            srk_auth_.const_data()))))) {
      Tspi_Context_CloseObject(context_handle, local_srk_handle);
      return false;
    }
  }

  *srk_handle = local_srk_handle;
  return true;
}

} // namespace cryptohome
