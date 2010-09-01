// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Tpm

#include "tpm.h"

#include <base/file_util.h>
#include <base/platform_thread.h>
#include <base/time.h>
#include <openssl/rsa.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>

#include "crypto.h"
#include "mount.h"
#include "platform.h"

namespace cryptohome {

const unsigned char kDefaultSrkAuth[] = { };
const int kDefaultTpmRsaKeyBits = 2048;
const int kDefaultDiscardableWrapPasswordLength = 32;
const char kDefaultCryptohomeKeyFile[] = "/home/.shadow/cryptohome.key";
const TSS_UUID kCryptohomeWellKnownUuid = {0x0203040b, 0, 0, 0, 0,
                                           {0, 9, 8, 1, 0, 3}};

Tpm::Tpm()
    : rsa_key_bits_(kDefaultTpmRsaKeyBits),
      srk_auth_(kDefaultSrkAuth, sizeof(kDefaultSrkAuth)),
      crypto_(NULL),
      context_handle_(0),
      key_handle_(0),
      key_file_(kDefaultCryptohomeKeyFile) {
}

Tpm::~Tpm() {
  Disconnect();
}

bool Tpm::Init(Crypto* crypto, bool open_key) {
  crypto_ = crypto;

  if (open_key && key_handle_ == 0) {
    Tpm::TpmRetryAction retry_action;
    return Connect(&retry_action);
  }

  return true;
}

bool Tpm::Connect(TpmRetryAction* retry_action) {
  *retry_action = Tpm::RetryNone;
  if (key_handle_ == 0) {
    TSS_RESULT result;
    TSS_HCONTEXT context_handle;
    if (!OpenAndConnectTpm(&context_handle, &result)) {
      context_handle_ = 0;
      key_handle_ = 0;
      *retry_action = HandleError(result);
      return false;
    }

    TSS_HKEY key_handle;
    if (!LoadOrCreateCryptohomeKey(context_handle, false, &key_handle,
                                   &result)) {
      context_handle_ = 0;
      key_handle_ = 0;
      *retry_action = HandleError(result);
      Tspi_Context_Close(context_handle);
      return false;
    }

    key_handle_ = key_handle;
    context_handle_ = context_handle;
  }

  return true;
}

bool Tpm::IsConnected() {
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

void Tpm::GetStatus(bool check_crypto, Tpm::TpmStatus* status) {
  memset(status, 0, sizeof(Tpm::TpmStatus));
  status->ThisInstanceHasContext = (context_handle_ != 0);
  status->ThisInstanceHasKeyHandle = (key_handle_ != 0);
  TSS_HCONTEXT context_handle = 0;
  do {
    // Check if we can connect
    TSS_RESULT result;
    if (!OpenAndConnectTpm(&context_handle, &result)) {
      status->LastTpmError = result;
      break;
    }
    status->CanConnect = true;

    // Check the Storage Root Key
    TSS_HKEY srk_handle;
    if (!LoadSrk(context_handle, &srk_handle, &result)) {
      status->LastTpmError = result;
      break;
    }
    status->CanLoadSrk = true;

    // Check the SRK public key
    unsigned int size_n;
    BYTE *public_srk;
    if ((result = Tspi_Key_GetPubKey(srk_handle, &size_n, &public_srk))) {
      Tspi_Context_CloseObject(context_handle, srk_handle);
      status->LastTpmError = result;
      break;
    }
    Tspi_Context_FreeMemory(context_handle, public_srk);
    Tspi_Context_CloseObject(context_handle, srk_handle);
    status->CanLoadSrkPublicKey = true;

    // Check the Cryptohome key
    TSS_HKEY key_handle;
    if (!LoadCryptohomeKey(context_handle, &key_handle, &result)) {
      status->LastTpmError = result;
      break;
    }
    status->HasCryptohomeKey = true;

    if (check_crypto) {
      // Check encryption (we don't care about the contents, just whether or not
      // there was an error)
      SecureBlob data(16);
      SecureBlob password(16);
      SecureBlob salt(8);
      SecureBlob data_out(16);
      memset(data.data(), 'A', data.size());
      memset(password.data(), 'B', password.size());
      memset(salt.data(), 'C', salt.size());
      memset(data_out.data(), 'D', data_out.size());
      if (!EncryptBlob(context_handle, key_handle, data, password,
                       13, salt, &data_out, &result)) {
        Tspi_Context_CloseObject(context_handle, key_handle);
        status->LastTpmError = result;
        break;
      }
      status->CanEncrypt = true;

      // Check decryption (we don't care about the contents, just whether or not
      // there was an error)
      if (!DecryptBlob(context_handle, key_handle, data_out, password,
                       13, salt, &data, &result)) {
        Tspi_Context_CloseObject(context_handle, key_handle);
        status->LastTpmError = result;
        break;
      }
      status->CanDecrypt = true;
    }
    Tspi_Context_CloseObject(context_handle, key_handle);
  } while (false);

  if (context_handle) {
    Tspi_Context_Close(context_handle);
  }
}

bool Tpm::CreateCryptohomeKey(TSS_HCONTEXT context_handle, bool create_in_tpm,
                              TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  // Load the Storage Root Key
  TSS_HKEY srk_handle;
  if (!LoadSrk(context_handle, &srk_handle, result)) {
    return false;
  }

  // Make sure we can get the public key for the SRK.  If not, then the TPM
  // is not available.
  unsigned int size_n;
  BYTE *public_srk;
  if ((*result = Tspi_Key_GetPubKey(srk_handle, &size_n, &public_srk))) {
    Tspi_Context_CloseObject(context_handle, srk_handle);
    return false;
  }
  Tspi_Context_FreeMemory(context_handle, public_srk);

  // Create the key object
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
        Tspi_Context_CloseObject(context_handle, srk_handle);
        return false;
    }
  }
  TSS_HKEY local_key_handle;
  if ((*result = Tspi_Context_CreateObject(context_handle,
                                           TSS_OBJECT_TYPE_RSAKEY,
                                           init_flags, &local_key_handle))) {
    LOG(ERROR) << "Error calling Tspi_Context_CreateObject: " << *result;
    Tspi_Context_CloseObject(context_handle, srk_handle);
    return false;
  }

  // Set the attributes
  UINT32 sig_scheme = TSS_SS_RSASSAPKCS1V15_DER;
  if ((*result = Tspi_SetAttribUint32(local_key_handle, TSS_TSPATTRIB_KEY_INFO,
                                      TSS_TSPATTRIB_KEYINFO_SIGSCHEME,
                                      sig_scheme))) {
    LOG(ERROR) << "Error calling Tspi_SetAttribUint32: " << *result;
    Tspi_Context_CloseObject(context_handle, srk_handle);
    Tspi_Context_CloseObject(context_handle, local_key_handle);
    return false;
  }

  UINT32 enc_scheme = TSS_ES_RSAESPKCSV15;
  if ((*result = Tspi_SetAttribUint32(local_key_handle, TSS_TSPATTRIB_KEY_INFO,
                                      TSS_TSPATTRIB_KEYINFO_ENCSCHEME,
                                      enc_scheme))) {
    LOG(ERROR) << "Error calling Tspi_SetAttribUint32: " << *result;
    Tspi_Context_CloseObject(context_handle, srk_handle);
    Tspi_Context_CloseObject(context_handle, local_key_handle);
    return false;
  }

  // Create a new system-wide key for cryptohome
  if (create_in_tpm) {
    if ((*result = Tspi_Key_CreateKey(local_key_handle, srk_handle, 0))) {
      LOG(ERROR) << "Error calling Tspi_Key_CreateKey: " << *result;
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }
  } else {
    TSS_HPOLICY policy_handle;
    if ((*result = Tspi_Context_CreateObject(context_handle,
                                             TSS_OBJECT_TYPE_POLICY,
                                             TSS_POLICY_MIGRATION,
                                             &policy_handle))) {
      LOG(ERROR) << "Error creating policy object: " << *result;
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
    if ((*result = Tspi_Policy_SetSecret(policy_handle, TSS_SECRET_MODE_PLAIN,
                       migration_password.size(),
                       static_cast<BYTE*>(migration_password.data())))) {
      LOG(ERROR) << "Error setting migration policy password: " << *result;
      Tspi_Context_CloseObject(context_handle, policy_handle);
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }

    if ((*result = Tspi_Policy_AssignToObject(policy_handle,
                                              local_key_handle))) {
      LOG(ERROR) << "Error assigning migration policy: " << *result;
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

    if ((*result = Tspi_SetAttribData(local_key_handle,
                                      TSS_TSPATTRIB_RSAKEY_INFO,
                                      TSS_TSPATTRIB_KEYINFO_RSA_MODULUS,
                                      n.size(),
                                      static_cast<BYTE *>(n.data())))) {
      LOG(ERROR) << "Error setting RSA modulus: " << *result;
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }

    if ((*result = Tspi_SetAttribData(local_key_handle, TSS_TSPATTRIB_KEY_BLOB,
                                      TSS_TSPATTRIB_KEYBLOB_PRIVATE_KEY,
                                      p.size(),
                                      static_cast<BYTE *>(p.data())))) {
      LOG(ERROR) << "Error setting private key: " << *result;
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }

    if ((*result = Tspi_Key_WrapKey(local_key_handle, srk_handle, 0))) {
      LOG(ERROR) << "Error wrapping RSA key: " << *result;
      Tspi_Context_CloseObject(context_handle, srk_handle);
      Tspi_Context_CloseObject(context_handle, local_key_handle);
      return false;
    }
  }

  if (!SaveCryptohomeKey(context_handle, local_key_handle, result)) {
    LOG(ERROR) << "Couldn't save cryptohome key";
    Tspi_Context_CloseObject(context_handle, srk_handle);
    Tspi_Context_CloseObject(context_handle, local_key_handle);
    return false;
  }

  Tspi_Context_CloseObject(context_handle, srk_handle);
  Tspi_Context_CloseObject(context_handle, local_key_handle);

  return true;
}

bool Tpm::LoadCryptohomeKey(TSS_HCONTEXT context_handle,
                            TSS_HKEY* key_handle, TSS_RESULT* result) {
  // Load the Storage Root Key
  TSS_HKEY srk_handle;
  if (!LoadSrk(context_handle, &srk_handle, result)) {
    return false;
  }

  // Make sure we can get the public key for the SRK.  If not, then the TPM
  // is not available.
  unsigned int size_n;
  BYTE *public_srk;
  if ((*result = Tspi_Key_GetPubKey(srk_handle, &size_n, &public_srk))) {
    Tspi_Context_CloseObject(context_handle, srk_handle);
    return false;
  }
  Tspi_Context_FreeMemory(context_handle, public_srk);

  // First, try loading the key from the key file
  SecureBlob raw_key;
  if (Mount::LoadFileBytes(FilePath(key_file_), &raw_key)) {
    if ((*result = Tspi_Context_LoadKeyByBlob(context_handle,
        srk_handle,
        raw_key.size(),
        const_cast<BYTE*>(static_cast<const BYTE*>(raw_key.const_data())),
        key_handle))) {
      // If the error is expected to be transient, return now.
      if (IsTransient(*result)) {
        Tspi_Context_CloseObject(context_handle, srk_handle);
        return false;
      }
    } else {
      Tspi_Context_CloseObject(context_handle, srk_handle);
      return true;
    }
  }

  // Then try loading the key by the UUID (this is a legacy upgrade path)
  if ((*result = Tspi_Context_LoadKeyByUUID(context_handle,
                                            TSS_PS_TYPE_SYSTEM,
                                            kCryptohomeWellKnownUuid,
                                            key_handle))) {
    // If the error is expected to be transient, return now.
    if (IsTransient(*result)) {
      Tspi_Context_CloseObject(context_handle, srk_handle);
      return false;
    }
  } else {
    Tspi_Context_CloseObject(context_handle, srk_handle);
    // Save the cryptohome key to the well-known location
    if (!SaveCryptohomeKey(context_handle, *key_handle, result)) {
      LOG(ERROR) << "Couldn't save cryptohome key";
      return false;
    }
    return true;
  }

  Tspi_Context_CloseObject(context_handle, srk_handle);
  return false;
}

bool Tpm::LoadOrCreateCryptohomeKey(TSS_HCONTEXT context_handle,
                                    bool create_in_tpm,
                                    TSS_HKEY* key_handle,
                                    TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  // Try to load the cryptohome key.
  if (LoadCryptohomeKey(context_handle, key_handle, result)) {
    return true;
  }

  // If the error is expected to be transient, return now.
  if (IsTransient(*result)) {
    return false;
  }

  // Otherwise, the key couldn't be loaded, and it wasn't due to a transient
  // error, so we must create the key.
  if (CreateCryptohomeKey(context_handle, create_in_tpm, result)) {
    return true;
  }

  // If the error is expected to be transient, return now.
  if (IsTransient(*result)) {
    return false;
  }

  if (LoadCryptohomeKey(context_handle, key_handle, result)) {
    return true;
  }

  // Don't check the retry status, since we are returning false here anyway
  return false;
}

bool Tpm::IsTransient(TSS_RESULT result) {
  bool transient = false;
  switch (ERROR_CODE(result)) {
    case TSS_E_COMM_FAILURE:
    case TSS_E_INVALID_HANDLE:
    case TPM_E_DEFEND_LOCK_RUNNING:
      transient = true;
      break;
    default:
      break;
  }
  return transient;
}

Tpm::TpmRetryAction Tpm::HandleError(TSS_RESULT result) {
  Tpm::TpmRetryAction status = Tpm::RetryNone;
  switch (ERROR_CODE(result)) {
    case TSS_E_COMM_FAILURE:
      LOG(ERROR) << "Communications failure with the TPM.";
      Disconnect();
      status = Tpm::RetryCommFailure;
      break;
    case TSS_E_INVALID_HANDLE:
      LOG(ERROR) << "Invalid handle to the TPM.";
      Disconnect();
      status = Tpm::RetryCommFailure;
      break;
    case TPM_E_DEFEND_LOCK_RUNNING:
      LOG(ERROR) << "The TPM is defending itself against possible dictionary "
                 << "attacks.";
      status = Tpm::RetryDefendLock;
      break;
    default:
      break;
  }
  return status;
}

bool Tpm::SaveCryptohomeKey(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                            TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  SecureBlob raw_key;
  if (!GetKeyBlob(context_handle, key_handle, &raw_key, result)) {
    LOG(ERROR) << "Error getting key blob";
    return false;
  }
  Platform platform;
  int previous_mask = platform.SetMask(cryptohome::kDefaultUmask);
  unsigned int data_written = file_util::WriteFile(
      FilePath(key_file_),
      static_cast<const char*>(raw_key.const_data()),
      raw_key.size());
  platform.SetMask(previous_mask);
  if (data_written != raw_key.size()) {
    LOG(ERROR) << "Error writing key file";
    return false;
  }
  return true;
}

int Tpm::GetMaxRsaKeyCount() {
  if (context_handle_ == 0) {
    return -1;
  }

  return GetMaxRsaKeyCountForContext(context_handle_);
}

int Tpm::GetMaxRsaKeyCountForContext(TSS_HCONTEXT context_handle) {
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

bool Tpm::OpenAndConnectTpm(TSS_HCONTEXT* context_handle, TSS_RESULT* result) {
  TSS_HCONTEXT local_context_handle;
  if ((*result = Tspi_Context_Create(&local_context_handle))) {
    LOG(ERROR) << "Error calling Tspi_Context_Create: " << *result;
    return false;
  }

  for (int i = 0; i < 5; i++) {
    if ((*result = Tspi_Context_Connect(local_context_handle, NULL))) {
      // If there was a communications failure, try sleeping a bit here--it may
      // be that tcsd is still starting
      if (ERROR_CODE(*result) == TSS_E_COMM_FAILURE) {
        PlatformThread::Sleep(100);
      } else {
        LOG(ERROR) << "Error calling Tspi_Context_Connect: " << *result;
        Tspi_Context_Close(local_context_handle);
        return false;
      }
    } else {
      break;
    }
  }

  if (*result) {
    LOG(ERROR) << "Error calling Tspi_Context_Connect: " << *result;
    Tspi_Context_Close(local_context_handle);
    return false;
  }

  *context_handle = local_context_handle;
  return true;
}

bool Tpm::Encrypt(const chromeos::Blob& data, const chromeos::Blob& password,
                  int password_rounds, const chromeos::Blob& salt,
                  SecureBlob* data_out, Tpm::TpmRetryAction* retry_action) {
  *retry_action = Tpm::RetryNone;
  if (!IsConnected()) {
    if (!Connect(retry_action)) {
      return false;
    }
  }

  TSS_RESULT result = TSS_SUCCESS;
  if (!EncryptBlob(context_handle_, key_handle_, data, password,
                   password_rounds, salt, data_out, &result)) {
    *retry_action = HandleError(result);
    return false;
  }
  return true;
}

bool Tpm::Decrypt(const chromeos::Blob& data, const chromeos::Blob& password,
                  int password_rounds, const chromeos::Blob& salt,
                  SecureBlob* data_out, Tpm::TpmRetryAction* retry_action) {
  *retry_action = Tpm::RetryNone;
  if (!IsConnected()) {
    if (!Connect(retry_action)) {
      return false;
    }
  }

  TSS_RESULT result = TSS_SUCCESS;
  if (!DecryptBlob(context_handle_, key_handle_, data, password,
                   password_rounds, salt, data_out, &result)) {
    *retry_action = HandleError(result);
    return false;
  }
  return true;
}

bool Tpm::GetKey(SecureBlob* blob, Tpm::TpmRetryAction* retry_action) {
  *retry_action = Tpm::RetryNone;
  if (!IsConnected()) {
    if (!Connect(retry_action)) {
      return false;
    }
  }

  TSS_RESULT result = TSS_SUCCESS;
  if (!GetKeyBlob(context_handle_, key_handle_, blob, &result)) {
    *retry_action = HandleError(result);
    return false;
  }
  return true;
}

bool Tpm::GetPublicKey(SecureBlob* blob, Tpm::TpmRetryAction* retry_action) {
  *retry_action = Tpm::RetryNone;
  if (!IsConnected()) {
    if (!Connect(retry_action)) {
      return false;
    }
  }

  TSS_RESULT result = TSS_SUCCESS;
  if (!GetPublicKeyBlob(context_handle_, key_handle_, blob, &result)) {
    *retry_action = HandleError(result);
    return false;
  }

  return true;
}

bool Tpm::LoadKey(const SecureBlob& blob, Tpm::TpmRetryAction* retry_action) {
  *retry_action = Tpm::RetryNone;
  if (!IsConnected()) {
    if (!Connect(retry_action)) {
      return false;
    }
  }

  TSS_HKEY local_key_handle;
  TSS_RESULT result = TSS_SUCCESS;
  if (!LoadKeyBlob(context_handle_, blob, &local_key_handle, &result)) {
    *retry_action = HandleError(result);
    return false;
  }

  key_handle_ = local_key_handle;
  return true;
}

// Begin private methods
bool Tpm::EncryptBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                      const chromeos::Blob& data,
                      const chromeos::Blob& password, int password_rounds,
                      const chromeos::Blob& salt, SecureBlob* data_out,
                      TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  TSS_FLAG init_flags = TSS_ENCDATA_SEAL;
  TSS_HKEY enc_handle;
  if ((*result = Tspi_Context_CreateObject(context_handle,
                                          TSS_OBJECT_TYPE_ENCDATA,
                                          init_flags, &enc_handle))) {
    LOG(ERROR) << "Error calling Tspi_Context_CreateObject: " << *result;
    return false;
  }

  // TODO(fes): Check RSA key modulus size, return an error or block input
  if ((*result = Tspi_Data_Bind(enc_handle, key_handle, data.size(),
      const_cast<BYTE *>(&data[0])))) {
    LOG(ERROR) << "Error calling Tspi_Data_Bind: " << *result;
    Tspi_Context_CloseObject(context_handle, enc_handle);
    return false;
  }

  unsigned char* enc_data = NULL;
  UINT32 enc_data_length = 0;
  if ((*result = Tspi_GetAttribData(enc_handle, TSS_TSPATTRIB_ENCDATA_BLOB,
                                   TSS_TSPATTRIB_ENCDATABLOB_BLOB,
                                   &enc_data_length, &enc_data))) {
    LOG(ERROR) << "Error calling Tspi_GetAttribData: " << *result;
    Tspi_Context_CloseObject(context_handle, enc_handle);
    return false;
  }

  SecureBlob local_data(enc_data_length);
  memcpy(local_data.data(), enc_data, enc_data_length);
  Tspi_Context_FreeMemory(context_handle, enc_data);
  Tspi_Context_CloseObject(context_handle, enc_handle);

  SecureBlob aes_key;
  SecureBlob iv;
  if (!crypto_->PasskeyToAesKey(password,
                                salt,
                                password_rounds,
                                &aes_key,
                                &iv)) {
    LOG(ERROR) << "Failure converting passkey to key";
    return false;
  }

  unsigned int aes_block_size = crypto_->GetAesBlockSize();
  unsigned int offset = local_data.size() - aes_block_size;
  if (offset < 0) {
    LOG(ERROR) << "Encrypted data is too small.";
    return false;
  }

  SecureBlob passkey_part;
  if (!crypto_->WrapAesSpecifyBlockMode(local_data, offset, aes_block_size,
                                        aes_key, iv, Crypto::PADDING_NONE,
                                        Crypto::ECB, &passkey_part)) {
    LOG(ERROR) << "AES encryption failed.";
    return false;
  }
  PLOG_IF(FATAL, passkey_part.size() != aes_block_size)
      << "Output block size error: " << passkey_part.size() << ", expected: "
      << aes_block_size;
  memcpy(&local_data[offset], passkey_part.data(), passkey_part.size());

  data_out->swap(local_data);
  return true;
}

bool Tpm::DecryptBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                      const chromeos::Blob& data,
                      const chromeos::Blob& password, int password_rounds,
                      const chromeos::Blob& salt, SecureBlob* data_out,
                      TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  SecureBlob aes_key;
  SecureBlob iv;
  if (!crypto_->PasskeyToAesKey(password,
                                salt,
                                password_rounds,
                                &aes_key,
                                &iv)) {
    LOG(ERROR) << "Failure converting passkey to key";
    return false;
  }

  unsigned int aes_block_size = crypto_->GetAesBlockSize();
  unsigned int offset = data.size() - aes_block_size;
  if (offset < 0) {
    LOG(ERROR) << "Input data is too small.";
    return false;
  }

  SecureBlob passkey_part;
  if (!crypto_->UnwrapAesSpecifyBlockMode(data, offset, aes_block_size, aes_key,
                                          iv, Crypto::PADDING_NONE, Crypto::ECB,
                                          &passkey_part)) {
    LOG(ERROR) << "AES decryption failed.";
    return false;
  }
  PLOG_IF(FATAL, passkey_part.size() != aes_block_size)
      << "Output block size error: " << passkey_part.size() << ", expected: "
      << aes_block_size;
  SecureBlob local_data(data.begin(), data.end());
  memcpy(&local_data[offset], passkey_part.data(), passkey_part.size());

  TSS_FLAG init_flags = TSS_ENCDATA_SEAL;
  TSS_HKEY enc_handle;
  if ((*result = Tspi_Context_CreateObject(context_handle,
                                          TSS_OBJECT_TYPE_ENCDATA,
                                          init_flags, &enc_handle))) {
    LOG(ERROR) << "Error calling Tspi_Context_CreateObject: " << *result;
    return false;
  }

  if ((*result = Tspi_SetAttribData(enc_handle,
      TSS_TSPATTRIB_ENCDATA_BLOB, TSS_TSPATTRIB_ENCDATABLOB_BLOB,
      local_data.size(),
      static_cast<BYTE *>(const_cast<void*>(local_data.const_data()))))) {
    LOG(ERROR) << "Error calling Tspi_SetAttribData: " << *result;
    Tspi_Context_CloseObject(context_handle, enc_handle);
    return false;
  }

  unsigned char* dec_data = NULL;
  UINT32 dec_data_length = 0;
  if ((*result = Tspi_Data_Unbind(enc_handle, key_handle, &dec_data_length,
                                 &dec_data))) {
    LOG(ERROR) << "Error calling Tspi_Data_Unbind: " << *result;
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
                     SecureBlob* data_out, TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  BYTE* blob;
  UINT32 blob_size;
  if ((*result = Tspi_GetAttribData(key_handle, TSS_TSPATTRIB_KEY_BLOB,
                                   TSS_TSPATTRIB_KEYBLOB_BLOB,
                                   &blob_size, &blob))) {
    LOG(ERROR) << "Couldn't get key blob: " << *result;
    return false;
  }

  SecureBlob local_data(blob_size);
  memcpy(local_data.data(), blob, blob_size);
  chromeos::SecureMemset(blob, 0, blob_size);
  Tspi_Context_FreeMemory(context_handle, blob);
  data_out->swap(local_data);
  return true;
}

bool Tpm::GetPublicKeyBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                           SecureBlob* data_out, TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  BYTE *blob;
  UINT32 blob_size;
  if ((*result = Tspi_Key_GetPubKey(key_handle, &blob_size, &blob))) {
    LOG(ERROR) << "Error calling Tspi_Key_GetPubKey: " << *result;
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
                      TSS_HKEY* key_handle, TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  TSS_HKEY srk_handle;
  if (!LoadSrk(context_handle, &srk_handle, result)) {
    return false;
  }

  TSS_HKEY local_key_handle = 0;
  if ((*result = Tspi_Context_LoadKeyByBlob(context_handle,
      srk_handle,
      blob.size(),
      const_cast<BYTE*>(static_cast<const BYTE*>(blob.const_data())),
      &local_key_handle))) {
    LOG(ERROR) << "Error calling Tspi_Context_LoadKeyByBlob: " << *result;
    Tspi_Context_CloseObject(context_handle, srk_handle);
    return false;
  }

  Tspi_Context_CloseObject(context_handle, srk_handle);

  unsigned int size_n;
  BYTE *public_key;
  if ((*result = Tspi_Key_GetPubKey(local_key_handle, &size_n, &public_key))) {
    LOG(ERROR) << "Error calling Tspi_Key_GetPubKey: " << *result;
    Tspi_Context_CloseObject(context_handle, local_key_handle);
    return false;
  }
  Tspi_Context_FreeMemory(context_handle, public_key);

  *key_handle = local_key_handle;
  return true;
}

bool Tpm::LoadSrk(TSS_HCONTEXT context_handle, TSS_HKEY* srk_handle,
                  TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  // Load the Storage Root Key
  TSS_UUID SRK_UUID = TSS_UUID_SRK;
  TSS_HKEY local_srk_handle;
  if ((*result = Tspi_Context_LoadKeyByUUID(context_handle, TSS_PS_TYPE_SYSTEM,
                                           SRK_UUID, &local_srk_handle))) {
    return false;
  }

  // Check if the SRK wants a password
  UINT32 srk_authusage;
  if ((*result = Tspi_GetAttribUint32(local_srk_handle, TSS_TSPATTRIB_KEY_INFO,
                                     TSS_TSPATTRIB_KEYINFO_AUTHUSAGE,
                                     &srk_authusage))) {
    Tspi_Context_CloseObject(context_handle, local_srk_handle);
    return false;
  }

  // Give it the password if needed
  if (srk_authusage) {
    TSS_HPOLICY srk_usage_policy;
    if ((*result = Tspi_GetPolicyObject(local_srk_handle, TSS_POLICY_USAGE,
                                       &srk_usage_policy))) {
      Tspi_Context_CloseObject(context_handle, local_srk_handle);
      return false;
    }

    if ((*result = Tspi_Policy_SetSecret(srk_usage_policy,
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
