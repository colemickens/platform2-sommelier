// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Tpm

#include "tpm.h"

#include <arpa/inet.h>
#include <base/file_util.h>
#include <base/memory/scoped_ptr.h>
#include <base/threading/platform_thread.h>
#include <base/time.h>
#include <base/values.h>
#include <openssl/rsa.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <trousers/scoped_tss_type.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>

#include "cryptolib.h"
#include "platform.h"
#include "tpm_init.h"

using base::PlatformThread;
using chromeos::SecureBlob;
using trousers::ScopedTssContext;
using trousers::ScopedTssKey;
using trousers::ScopedTssMemory;
using trousers::ScopedTssNvStore;
using trousers::ScopedTssPcrs;
using trousers::ScopedTssPolicy;

namespace cryptohome {

#define TPM_LOG(severity, result) \
  LOG(severity) << "TPM error 0x" << std::hex << result \
                << " (" << Trspi_Error_String(result) << "): "

const unsigned char kDefaultSrkAuth[] = { };
const unsigned int kDefaultTpmRsaKeyBits = 2048;
const unsigned int kDefaultTpmRsaKeyFlag = TSS_KEY_SIZE_2048;
const unsigned int kDefaultDiscardableWrapPasswordLength = 32;
const char kDefaultCryptohomeKeyFile[] = "/home/.shadow/cryptohome.key";
const TSS_UUID kCryptohomeWellKnownUuid = {0x0203040b, 0, 0, 0, 0,
                                           {0, 9, 8, 1, 0, 3}};
const char* kWellKnownSrkTmp = "1234567890";
const int kOwnerPasswordLength = 12;
const int kMaxTimeoutRetries = 5;
const char* kTpmCheckEnabledFile = "/sys/class/misc/tpm0/device/enabled";
const char* kTpmCheckOwnedFile = "/sys/class/misc/tpm0/device/owned";
const char* kTpmOwnedFileOld = "/var/lib/.tpm_owned";
const char* kTpmStatusFileOld = "/var/lib/.tpm_status";
const char* kTpmOwnedFile = "/mnt/stateful_partition/.tpm_owned";
const char* kTpmStatusFile = "/mnt/stateful_partition/.tpm_status";
const char* kOpenCryptokiPath = "/var/lib/opencryptoki";
const unsigned int kTpmConnectRetries = 10;
const unsigned int kTpmConnectIntervalMs = 100;
const char kTpmWellKnownPassword[] = TSS_WELL_KNOWN_SECRET;
const char kTpmOwnedWithWellKnown = 'W';
const char kTpmOwnedWithRandom = 'R';
const unsigned int kTpmBootPCR = 0;
const unsigned int kTpmPCRLocality = 1;

Tpm* Tpm::singleton_ = NULL;
base::Lock Tpm::singleton_lock_;

Tpm* Tpm::GetSingleton() {
  // TODO(fes): Replace with a better atomic operation
  singleton_lock_.Acquire();
  if (!singleton_)
    singleton_ = new Tpm();
  singleton_lock_.Release();
  return singleton_;
}

Tpm::Tpm()
    : initialized_(false),
      srk_auth_(kDefaultSrkAuth, sizeof(kDefaultSrkAuth)),
      context_handle_(0),
      key_handle_(0),
      owner_password_(),
      password_sync_lock_(),
      is_disabled_(true),
      is_owned_(false),
      is_being_owned_(false),
      platform_(NULL) {
}

Tpm::~Tpm() {
  Disconnect();
}

bool Tpm::Init(Platform* platform, bool open_key) {
  if (initialized_) {
    return false;
  }
  initialized_ = true;
  DCHECK(platform);
  platform_ = platform;

  // Migrate any old status files from old location to new location.
  if (!file_util::PathExists(FilePath(kTpmOwnedFile)) &&
      file_util::PathExists(FilePath(kTpmOwnedFileOld))) {
    file_util::Move(FilePath(kTpmOwnedFileOld), FilePath(kTpmOwnedFile));
  }
  if (!file_util::PathExists(FilePath(kTpmStatusFile)) &&
      file_util::PathExists(FilePath(kTpmStatusFileOld))) {
    file_util::Move(FilePath(kTpmStatusFileOld), FilePath(kTpmStatusFile));
  }

  // Checking disabled and owned either via sysfs or via TSS calls will block if
  // ownership is being taken by another thread or process.  So for this to work
  // well, Tpm::Init() needs to be called before InitializeTpm() is called.  At
  // that point, the public API for Tpm only checks these booleans, so other
  // threads can check without being blocked.  InitializeTpm() will reset the
  // is_owned_ bit on success.
  bool successful_check = false;
  if (file_util::PathExists(FilePath(kTpmCheckEnabledFile))) {
    is_disabled_ = IsDisabledCheckViaSysfs();
    is_owned_ = IsOwnedCheckViaSysfs();
    successful_check = true;
  } else {
    TSS_HCONTEXT context_handle;
    TSS_RESULT result;
    if (OpenAndConnectTpm(&context_handle, &result)) {
      bool enabled = false;
      bool owned = false;
      IsEnabledOwnedCheckViaContext(context_handle, &enabled, &owned);
      DisconnectContext(context_handle);
      is_disabled_ = !enabled;
      is_owned_ = owned;
      successful_check = true;
    }
  }
  if (successful_check && !is_owned_) {
    file_util::Delete(FilePath(kOpenCryptokiPath), true);
    file_util::Delete(FilePath(kTpmOwnedFile), false);
    file_util::Delete(FilePath(kTpmStatusFile), false);
  }
  TpmStatus tpm_status;
  if (LoadTpmStatus(&tpm_status)) {
    if (tpm_status.has_owner_password()) {
      SecureBlob local_owner_password;
      if (LoadOwnerPassword(tpm_status, &local_owner_password)) {
        password_sync_lock_.Acquire();
        owner_password_.assign(local_owner_password.begin(),
                               local_owner_password.end());
        password_sync_lock_.Release();
      }
    }
  }

  if (open_key && key_handle_ == 0) {
    Tpm::TpmRetryAction retry_action;
    return Connect(&retry_action);
  }

  return true;
}

bool Tpm::Connect(TpmRetryAction* retry_action) {
  // TODO(fes): Check the status of enabled, owned, being_owned first.
  *retry_action = Tpm::RetryNone;
  if (key_handle_ == 0) {
    TSS_RESULT result;
    TSS_HCONTEXT context_handle;
    if (!OpenAndConnectTpm(&context_handle, &result)) {
      context_handle_ = 0;
      key_handle_ = 0;
      *retry_action = HandleError(result);
      TPM_LOG(ERROR, result) << "Error connecting to the TPM";
      return false;
    }

    TSS_HKEY key_handle;
    if (!LoadOrCreateCryptohomeKey(context_handle, false, &key_handle,
                                   &result)) {
      context_handle_ = 0;
      key_handle_ = 0;
      *retry_action = HandleError(result);
      Tspi_Context_Close(context_handle);
      TPM_LOG(ERROR, result) << "Error loading the cryptohome TPM key";
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

TSS_HCONTEXT Tpm::ConnectContext() {
  TSS_RESULT result;
  TSS_HCONTEXT context_handle = 0;
  if (!OpenAndConnectTpm(&context_handle, &result)) {
    return 0;
  }

  return context_handle;
}

void Tpm::DisconnectContext(TSS_HCONTEXT context_handle) {
  if (context_handle) {
    Tspi_Context_Close(context_handle);
  }
}

bool Tpm::ConnectContextAsOwner(TSS_HCONTEXT* context, TSS_HTPM* tpm) {
  *context = 0;
  *tpm = 0;
  if (owner_password_.size() == 0) {
    LOG(ERROR) << "ConnectContextAsOwner requires an owner password";
    return false;
  }

  if (!is_owned_ || is_being_owned_) {
    LOG(ERROR) << "ConnectContextAsOwner: TPM is unowned or still being owned";
    return false;
  }

  if ((*context = ConnectContext()) == 0) {
    LOG(ERROR) << "ConnectContextAsOwner: Could not open the TPM";
    return false;
  }

  if (!GetTpmWithAuth(*context, owner_password_, tpm)) {
    LOG(ERROR) << "ConnectContextAsOwner: failed to authorize as the owner";
    Tspi_Context_Close(*context);
    *context = 0;
    *tpm = 0;
    return false;
  }
  return true;
}

bool Tpm::ConnectContextAsUser(TSS_HCONTEXT* context, TSS_HTPM* tpm) {
  *context = 0;
  *tpm = 0;
  if ((*context = ConnectContext()) == 0) {
    LOG(ERROR) << "ConnectContextAsUser: Could not open the TPM";
    return false;
  }

  if (!GetTpm(*context, tpm)) {
    LOG(ERROR) << "ConnectContextAsUser: failed to get a TPM object";
    Tspi_Context_Close(*context);
    *context = 0;
    *tpm = 0;
    return false;
  }
  return true;
}

void Tpm::GetStatus(bool check_crypto, Tpm::TpmStatusInfo* status) {
  memset(status, 0, sizeof(Tpm::TpmStatusInfo));
  status->ThisInstanceHasContext = (context_handle_ != 0);
  status->ThisInstanceHasKeyHandle = (key_handle_ != 0);
  ScopedTssContext context_handle;
  // Check if we can connect
  TSS_RESULT result;
  if (!OpenAndConnectTpm(context_handle.ptr(), &result)) {
    status->LastTpmError = result;
    return;
  }
  status->CanConnect = true;

  // Check the Storage Root Key
  ScopedTssKey srk_handle(context_handle);
  if (!LoadSrk(context_handle, srk_handle.ptr(), &result)) {
    status->LastTpmError = result;
    return;
  }
  status->CanLoadSrk = true;

  // Check the SRK public key
  unsigned int size_n;
  ScopedTssMemory public_srk(context_handle);
  if (TPM_ERROR(result = Tspi_Key_GetPubKey(srk_handle, &size_n,
                                            public_srk.ptr()))) {
    status->LastTpmError = result;
    return;
  }
  status->CanLoadSrkPublicKey = true;

  // Check the Cryptohome key
  ScopedTssKey key_handle(context_handle);
  if (!LoadCryptohomeKey(context_handle, key_handle.ptr(), &result)) {
    status->LastTpmError = result;
    return;
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
      status->LastTpmError = result;
      return;
    }
    status->CanEncrypt = true;

    // Check decryption (we don't care about the contents, just whether or not
    // there was an error)
    if (!DecryptBlob(context_handle, key_handle, data_out, password,
                     13, salt, &data, &result)) {
      status->LastTpmError = result;
      return;
    }
    status->CanDecrypt = true;
  }
}

bool Tpm::CreateCryptohomeKey(TSS_HCONTEXT context_handle, bool create_in_tpm,
                              TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  // Load the Storage Root Key
  ScopedTssKey srk_handle(context_handle);
  if (!LoadSrk(context_handle, srk_handle.ptr(), result)) {
    TPM_LOG(INFO, *result) << "CreateCryptohomeKey: Cannot load SRK";
    return false;
  }

  // Make sure we can get the public key for the SRK.  If not, then the TPM
  // is not available.
  unsigned int size_n;
  ScopedTssMemory public_srk(context_handle);
  if (TPM_ERROR(*result = Tspi_Key_GetPubKey(srk_handle, &size_n,
                                             public_srk.ptr()))) {
    TPM_LOG(INFO, *result) << "CreateCryptohomeKey: Cannot load SRK pub key";
    return false;
  }

  // Create the key object
  TSS_FLAG init_flags = TSS_KEY_TYPE_LEGACY | TSS_KEY_VOLATILE;
  if (!create_in_tpm) {
    init_flags |= TSS_KEY_MIGRATABLE;
    init_flags |= kDefaultTpmRsaKeyFlag;
  }
  ScopedTssKey local_key_handle(context_handle);
  if (TPM_ERROR(*result = Tspi_Context_CreateObject(context_handle,
                                                    TSS_OBJECT_TYPE_RSAKEY,
                                                    init_flags,
                                                    local_key_handle.ptr()))) {
    TPM_LOG(ERROR, *result) << "Error calling Tspi_Context_CreateObject";
    return false;
  }

  // Set the attributes
  UINT32 sig_scheme = TSS_SS_RSASSAPKCS1V15_DER;
  if (TPM_ERROR(*result = Tspi_SetAttribUint32(local_key_handle,
                                               TSS_TSPATTRIB_KEY_INFO,
                                               TSS_TSPATTRIB_KEYINFO_SIGSCHEME,
                                               sig_scheme))) {
    TPM_LOG(ERROR, *result) << "Error calling Tspi_SetAttribUint32";
    return false;
  }

  UINT32 enc_scheme = TSS_ES_RSAESPKCSV15;
  if (TPM_ERROR(*result = Tspi_SetAttribUint32(local_key_handle,
                                               TSS_TSPATTRIB_KEY_INFO,
                                               TSS_TSPATTRIB_KEYINFO_ENCSCHEME,
                                               enc_scheme))) {
    TPM_LOG(ERROR, *result) << "Error calling Tspi_SetAttribUint32";
    return false;
  }

  // Create a new system-wide key for cryptohome
  if (create_in_tpm) {
    if (TPM_ERROR(*result = Tspi_Key_CreateKey(local_key_handle,
                                               srk_handle,
                                               0))) {
      TPM_LOG(ERROR, *result) << "Error calling Tspi_Key_CreateKey";
      return false;
    }
  } else {
    ScopedTssPolicy policy_handle(context_handle);
    if (TPM_ERROR(*result = Tspi_Context_CreateObject(context_handle,
                                                      TSS_OBJECT_TYPE_POLICY,
                                                      TSS_POLICY_MIGRATION,
                                                      policy_handle.ptr()))) {
      TPM_LOG(ERROR, *result) << "Error creating policy object";
      return false;
    }

    // Set a random migration policy password, and discard it.  The key will not
    // be migrated, but to create the key outside of the TPM, we have to do it
    // this way.
    SecureBlob migration_password(kDefaultDiscardableWrapPasswordLength);
    CryptoLib::GetSecureRandom(
        static_cast<unsigned char*>(migration_password.data()),
        migration_password.size());
    if (TPM_ERROR(*result = Tspi_Policy_SetSecret(policy_handle,
                            TSS_SECRET_MODE_PLAIN,
                            migration_password.size(),
                            static_cast<BYTE*>(migration_password.data())))) {
      TPM_LOG(ERROR, *result) << "Error setting migration policy password";
      return false;
    }

    if (TPM_ERROR(*result = Tspi_Policy_AssignToObject(policy_handle,
                                                       local_key_handle))) {
      TPM_LOG(ERROR, *result) << "Error assigning migration policy";
      return false;
    }

    SecureBlob n;
    SecureBlob p;
    if (!CryptoLib::CreateRsaKey(kDefaultTpmRsaKeyBits, &n, &p)) {
      LOG(ERROR) << "Error creating RSA key";
      return false;
    }

    if (TPM_ERROR(*result = Tspi_SetAttribData(local_key_handle,
                                             TSS_TSPATTRIB_RSAKEY_INFO,
                                             TSS_TSPATTRIB_KEYINFO_RSA_MODULUS,
                                             n.size(),
                                             static_cast<BYTE *>(n.data())))) {
      TPM_LOG(ERROR, *result) << "Error setting RSA modulus";
      return false;
    }

    if (TPM_ERROR(*result = Tspi_SetAttribData(local_key_handle,
                                             TSS_TSPATTRIB_KEY_BLOB,
                                             TSS_TSPATTRIB_KEYBLOB_PRIVATE_KEY,
                                             p.size(),
                                             static_cast<BYTE *>(p.data())))) {
      TPM_LOG(ERROR, *result) << "Error setting private key";
      return false;
    }

    if (TPM_ERROR(*result = Tspi_Key_WrapKey(local_key_handle,
                                             srk_handle,
                                             0))) {
      TPM_LOG(ERROR, *result) << "Error wrapping RSA key";
      return false;
    }
  }

  if (!SaveCryptohomeKey(context_handle, local_key_handle, result)) {
    LOG(ERROR) << "Couldn't save cryptohome key";
    return false;
  }

  LOG(INFO) << "Created new cryptohome key.";
  return true;
}

bool Tpm::LoadCryptohomeKey(TSS_HCONTEXT context_handle,
                            TSS_HKEY* key_handle, TSS_RESULT* result) {
  // Load the Storage Root Key
  ScopedTssKey srk_handle(context_handle);
  if (!LoadSrk(context_handle, srk_handle.ptr(), result)) {
    TPM_LOG(INFO, *result) << "LoadCryptohomeKey: Cannot load SRK";
    return false;
  }

  // Make sure we can get the public key for the SRK.  If not, then the TPM
  // is not available.
  unsigned int size_n;
  ScopedTssMemory public_srk(context_handle);
  if (TPM_ERROR(*result = Tspi_Key_GetPubKey(srk_handle, &size_n,
                                             public_srk.ptr()))) {
    TPM_LOG(INFO, *result) << "LoadCryptohomeKey: Cannot load SRK public key";
    return false;
  }

  // First, try loading the key from the key file
  SecureBlob raw_key;
  if (platform_->ReadFile(kDefaultCryptohomeKeyFile, &raw_key)) {
    if (TPM_ERROR(*result = Tspi_Context_LoadKeyByBlob(
                                    context_handle,
                                    srk_handle,
                                    raw_key.size(),
                                    const_cast<BYTE*>(static_cast<const BYTE*>(
                                      raw_key.const_data())),
                                    key_handle))) {
      // If the error is expected to be transient, return now.
      if (IsTransient(*result)) {
        TPM_LOG(INFO, *result) << "LoadCryptohomeKey: Cannot load key " \
                                << "from blob";
        return false;
      }
    } else {
      SecureBlob pub_key;
      // Make sure that we can get the public key
      if (GetPublicKeyBlob(context_handle, *key_handle, &pub_key, result)) {
        return true;
      }
      // Otherwise, close the key and fall through
      Tspi_Context_CloseObject(context_handle, *key_handle);
      if (IsTransient(*result)) {
        TPM_LOG(INFO, *result) << "LoadCryptohomeKey: closed object";
        return false;
      }
    }
  }

  // Then try loading the key by the UUID (this is a legacy upgrade path)
  if (TPM_ERROR(*result = Tspi_Context_LoadKeyByUUID(context_handle,
                                                     TSS_PS_TYPE_SYSTEM,
                                                     kCryptohomeWellKnownUuid,
                                                     key_handle))) {
    // If the error is expected to be transient, return now.
    if (IsTransient(*result)) {
      TPM_LOG(INFO, *result) << "LoadCryptohomeKey: failed LoadKeyByUUID";
      return false;
    }
  } else {
    SecureBlob pub_key;
    // Make sure that we can get the public key
    if (GetPublicKeyBlob(context_handle, *key_handle, &pub_key, result)) {
      // Save the cryptohome key to the well-known location
      if (!SaveCryptohomeKey(context_handle, *key_handle, result)) {
        LOG(ERROR) << "Couldn't save cryptohome key";
        Tspi_Context_CloseObject(context_handle, *key_handle);
        return false;
      }
      return true;
    }
    Tspi_Context_CloseObject(context_handle, *key_handle);
  }

  TPM_LOG(INFO, *result) << "LoadCryptohomeKey: could not load key";
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
    TPM_LOG(INFO, *result) << "Transient failure loading cryptohome key";
    return false;
  }

  // Otherwise, the key couldn't be loaded, and it wasn't due to a transient
  // error, so we must create the key.
  if (CreateCryptohomeKey(context_handle, create_in_tpm, result)) {
    if (LoadCryptohomeKey(context_handle, key_handle, result)) {
      return true;
    }
  }

  // Don't check the retry status, since we are returning false here anyway
  TPM_LOG(INFO, *result) << "Unable to create or load cryptohome key";
  return false;
}

bool Tpm::IsTransient(TSS_RESULT result) {
  bool transient = false;
  switch (ERROR_CODE(result)) {
    case ERROR_CODE(TSS_E_COMM_FAILURE):
    case ERROR_CODE(TSS_E_INVALID_HANDLE):
    case ERROR_CODE(TPM_E_DEFEND_LOCK_RUNNING):
    // TODO(fes): We're considering this a transient failure for now
    case ERROR_CODE(TCS_E_KM_LOADFAILED):
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
    case ERROR_CODE(TSS_E_COMM_FAILURE):
      LOG(ERROR) << "Communications failure with the TPM.";
      Disconnect();
      status = Tpm::RetryCommFailure;
      break;
    case ERROR_CODE(TSS_E_INVALID_HANDLE):
      LOG(ERROR) << "Invalid handle to the TPM.";
      Disconnect();
      status = Tpm::RetryCommFailure;
      break;
    // TODO(fes): We're considering this a communication failure for now.
    case ERROR_CODE(TCS_E_KM_LOADFAILED):
      LOG(ERROR) << "Key load failed; problem with parent key authorization.";
      Disconnect();
      status = Tpm::RetryCommFailure;
      break;
    case ERROR_CODE(TPM_E_DEFEND_LOCK_RUNNING):
      LOG(ERROR) << "The TPM is defending itself against possible dictionary "
                 << "attacks.";
      status = Tpm::RetryDefendLock;
      break;
      // This error occurs on bad password
    case ERROR_CODE(TPM_E_DECRYPT_ERROR):
      status = Tpm::RetryNone;
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
      FilePath(kDefaultCryptohomeKeyFile),
      static_cast<const char*>(raw_key.const_data()),
      raw_key.size());
  platform.SetMask(previous_mask);
  if (data_written != raw_key.size()) {
    LOG(ERROR) << "Error writing key file.  Wrote: " << data_written
               << ", expected: " << raw_key.size();
    return false;
  }
  return true;
}

bool Tpm::OpenAndConnectTpm(TSS_HCONTEXT* context_handle, TSS_RESULT* result) {
  TSS_RESULT local_result;
  ScopedTssContext local_context_handle;
  if (TPM_ERROR(local_result = Tspi_Context_Create(
                                                local_context_handle.ptr()))) {
    TPM_LOG(ERROR, local_result) << "Error calling Tspi_Context_Create";
    if (result)
      *result = local_result;
    return false;
  }

  for (unsigned int i = 0; i < kTpmConnectRetries; i++) {
    if (TPM_ERROR(local_result = Tspi_Context_Connect(local_context_handle,
                                                      NULL))) {
      // If there was a communications failure, try sleeping a bit here--it may
      // be that tcsd is still starting
      if (ERROR_CODE(local_result) == TSS_E_COMM_FAILURE) {
        PlatformThread::Sleep(kTpmConnectIntervalMs);
      } else {
        TPM_LOG(ERROR, local_result) << "Error calling Tspi_Context_Connect";
        if (result)
          *result = local_result;
        return false;
      }
    } else {
      break;
    }
  }

  if (TPM_ERROR(local_result)) {
    TPM_LOG(ERROR, local_result) << "Error calling Tspi_Context_Connect";
    if (result)
      *result = local_result;
    return false;
  }

  *context_handle = local_context_handle.release();
  if (result)
    *result = local_result;
  return (*context_handle != 0);
}

bool Tpm::Encrypt(const chromeos::Blob& data, const chromeos::Blob& password,
                  unsigned int password_rounds, const chromeos::Blob& salt,
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
                  unsigned int password_rounds, const chromeos::Blob& salt,
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

Tpm::TpmRetryAction Tpm::GetPublicKeyHash(SecureBlob* hash) {
  TpmRetryAction ret = Tpm::RetryNone;
  if (!IsConnected() && !Connect(&ret))
    return ret;

  TSS_RESULT result = TSS_SUCCESS;
  SecureBlob pubkey;
  if (!GetPublicKeyBlob(context_handle_, key_handle_, &pubkey, &result)) {
    return HandleError(result);
  }

  *hash = CryptoLib::Sha1(pubkey);
  return RetryNone;
}

// Begin private methods
bool Tpm::EncryptBlob(TSS_HCONTEXT context_handle,
                      TSS_HKEY key_handle,
                      const chromeos::Blob& data,
                      const chromeos::Blob& password,
                      unsigned int password_rounds,
                      const chromeos::Blob& salt,
                      SecureBlob* data_out,
                      TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  TSS_FLAG init_flags = TSS_ENCDATA_SEAL;
  ScopedTssKey enc_handle(context_handle);
  if (TPM_ERROR(*result = Tspi_Context_CreateObject(context_handle,
                                                    TSS_OBJECT_TYPE_ENCDATA,
                                                    init_flags,
                                                    enc_handle.ptr()))) {
    TPM_LOG(ERROR, *result) << "Error calling Tspi_Context_CreateObject";
    return false;
  }

  // TODO(fes): Check RSA key modulus size, return an error or block input

  if (TPM_ERROR(*result = Tspi_Data_Bind(enc_handle, key_handle, data.size(),
                                         const_cast<BYTE *>(&data[0])))) {
    TPM_LOG(ERROR, *result) << "Error calling Tspi_Data_Bind";
    return false;
  }

  UINT32 enc_data_length = 0;
  ScopedTssMemory enc_data(context_handle);
  if (TPM_ERROR(*result = Tspi_GetAttribData(enc_handle,
                                             TSS_TSPATTRIB_ENCDATA_BLOB,
                                             TSS_TSPATTRIB_ENCDATABLOB_BLOB,
                                             &enc_data_length,
                                             enc_data.ptr()))) {
    TPM_LOG(ERROR, *result) << "Error calling Tspi_GetAttribData";
    return false;
  }

  SecureBlob local_data(enc_data_length);
  memcpy(local_data.data(), enc_data.value(), enc_data_length);
  // We're done with enc_* so let's free it now.
  enc_data.reset();
  enc_handle.reset();

  SecureBlob aes_key;
  SecureBlob iv;
  if (!CryptoLib::PasskeyToAesKey(password,
                                salt,
                                password_rounds,
                                &aes_key,
                                &iv)) {
    LOG(ERROR) << "Failure converting passkey to key";
    return false;
  }

  unsigned int aes_block_size = CryptoLib::GetAesBlockSize();
  if (aes_block_size > local_data.size()) {
    LOG(ERROR) << "Encrypted data is too small.";
    return false;
  }
  unsigned int offset = local_data.size() - aes_block_size;

  SecureBlob passkey_part;
  if (!CryptoLib::AesEncryptSpecifyBlockMode(local_data, offset, aes_block_size,
                                             aes_key, iv,
                                             CryptoLib::kPaddingNone,
                                             CryptoLib::kEcb, &passkey_part)) {
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

bool Tpm::DecryptBlob(TSS_HCONTEXT context_handle,
                      TSS_HKEY key_handle,
                      const chromeos::Blob& data,
                      const chromeos::Blob& password,
                      unsigned int password_rounds,
                      const chromeos::Blob& salt,
                      SecureBlob* data_out,
                      TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  SecureBlob aes_key;
  SecureBlob iv;
  if (!CryptoLib::PasskeyToAesKey(password,
                                  salt,
                                  password_rounds,
                                  &aes_key,
                                  &iv)) {
    LOG(ERROR) << "Failure converting passkey to key";
    return false;
  }

  unsigned int aes_block_size = CryptoLib::GetAesBlockSize();
  if (aes_block_size > data.size()) {
    LOG(ERROR) << "Input data is too small.";
    return false;
  }
  unsigned int offset = data.size() - aes_block_size;

  SecureBlob passkey_part;
  if (!CryptoLib::AesDecryptSpecifyBlockMode(data, offset, aes_block_size,
                                           aes_key, iv, CryptoLib::kPaddingNone,
                                           CryptoLib::kEcb, &passkey_part)) {
    LOG(ERROR) << "AES decryption failed.";
    return false;
  }
  PLOG_IF(FATAL, passkey_part.size() != aes_block_size)
      << "Output block size error: " << passkey_part.size() << ", expected: "
      << aes_block_size;
  SecureBlob local_data(data.begin(), data.end());
  memcpy(&local_data[offset], passkey_part.data(), passkey_part.size());

  TSS_FLAG init_flags = TSS_ENCDATA_SEAL;
  ScopedTssKey enc_handle(context_handle);
  if (TPM_ERROR(*result = Tspi_Context_CreateObject(context_handle,
                                                    TSS_OBJECT_TYPE_ENCDATA,
                                                    init_flags,
                                                    enc_handle.ptr()))) {
    TPM_LOG(ERROR, *result) << "Error calling Tspi_Context_CreateObject";
    return false;
  }

  if (TPM_ERROR(*result = Tspi_SetAttribData(enc_handle,
                                             TSS_TSPATTRIB_ENCDATA_BLOB,
                                             TSS_TSPATTRIB_ENCDATABLOB_BLOB,
                                             local_data.size(),
                                             static_cast<BYTE *>(
                                               const_cast<void*>(
                                                 local_data.const_data()))))) {
    TPM_LOG(ERROR, *result) << "Error calling Tspi_SetAttribData";
    return false;
  }

  ScopedTssMemory dec_data(context_handle);
  UINT32 dec_data_length = 0;
  if (TPM_ERROR(*result = Tspi_Data_Unbind(enc_handle, key_handle,
                                           &dec_data_length, dec_data.ptr()))) {
    TPM_LOG(ERROR, *result) << "Error calling Tspi_Data_Unbind";
    return false;
  }

  data_out->resize(dec_data_length);
  memcpy(data_out->data(), dec_data.value(), dec_data_length);
  chromeos::SecureMemset(dec_data.value(), 0, dec_data_length);

  return true;
}

bool Tpm::GetKeyBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                     SecureBlob* data_out, TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  ScopedTssMemory blob(context_handle);
  UINT32 blob_size;
  if (TPM_ERROR(*result = Tspi_GetAttribData(key_handle, TSS_TSPATTRIB_KEY_BLOB,
                                   TSS_TSPATTRIB_KEYBLOB_BLOB,
                                   &blob_size, blob.ptr()))) {
    TPM_LOG(ERROR, *result) << "Couldn't get key blob";
    return false;
  }

  SecureBlob local_data(blob_size);
  memcpy(local_data.data(), blob.value(), blob_size);
  chromeos::SecureMemset(blob.value(), 0, blob_size);
  data_out->swap(local_data);
  return true;
}

bool Tpm::GetPublicKeyBlob(TSS_HCONTEXT context_handle, TSS_HKEY key_handle,
                           SecureBlob* data_out, TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  ScopedTssMemory blob(context_handle);
  UINT32 blob_size;
  if (TPM_ERROR(*result = Tspi_Key_GetPubKey(key_handle, &blob_size,
                                             blob.ptr()))) {
    TPM_LOG(ERROR, *result) << "Error calling Tspi_Key_GetPubKey";
    return false;
  }

  SecureBlob local_data(blob_size);
  memcpy(local_data.data(), blob.value(), blob_size);
  chromeos::SecureMemset(blob.value(), 0, blob_size);
  data_out->swap(local_data);
  return true;
}

bool Tpm::LoadSrk(TSS_HCONTEXT context_handle, TSS_HKEY* srk_handle,
                  TSS_RESULT* result) {
  *result = TSS_SUCCESS;

  // Load the Storage Root Key
  TSS_UUID SRK_UUID = TSS_UUID_SRK;
  ScopedTssKey local_srk_handle(context_handle);
  if (TPM_ERROR(*result = Tspi_Context_LoadKeyByUUID(context_handle,
                                                     TSS_PS_TYPE_SYSTEM,
                                                     SRK_UUID,
                                                     local_srk_handle.ptr()))) {
    return false;
  }

  // Check if the SRK wants a password
  UINT32 srk_authusage;
  if (TPM_ERROR(*result = Tspi_GetAttribUint32(local_srk_handle,
                                               TSS_TSPATTRIB_KEY_INFO,
                                               TSS_TSPATTRIB_KEYINFO_AUTHUSAGE,
                                               &srk_authusage))) {
    return false;
  }

  // Give it the password if needed
  if (srk_authusage) {
    TSS_HPOLICY srk_usage_policy;
    if (TPM_ERROR(*result = Tspi_GetPolicyObject(local_srk_handle,
                                                 TSS_POLICY_USAGE,
                                                 &srk_usage_policy))) {
      return false;
    }

    *result = Tspi_Policy_SetSecret(srk_usage_policy,
                                    TSS_SECRET_MODE_PLAIN,
                                    srk_auth_.size(),
                                    const_cast<BYTE *>(
                                      static_cast<const BYTE *>(
                                        srk_auth_.const_data())));
    if (TPM_ERROR(*result)) {
      return false;
    }
  }

  *srk_handle = local_srk_handle.release();
  return true;
}

bool Tpm::IsDisabledCheckViaSysfs() {
  std::string contents;
  if (!file_util::ReadFileToString(FilePath(kTpmCheckEnabledFile), &contents)) {
    return false;
  }
  if (contents.size() < 1) {
    return false;
  }
  return (contents[0] == '0');
}

bool Tpm::IsOwnedCheckViaSysfs() {
  std::string contents;
  if (!file_util::ReadFileToString(FilePath(kTpmCheckOwnedFile), &contents)) {
    return false;
  }
  if (contents.size() < 1) {
    return false;
  }
  return (contents[0] != '0');
}

void Tpm::IsEnabledOwnedCheckViaContext(TSS_HCONTEXT context_handle,
                                        bool* enabled, bool* owned) {
  *enabled = false;
  *owned = false;

  TSS_RESULT result;
  TSS_HTPM tpm_handle;
  if (!GetTpm(context_handle, &tpm_handle)) {
    return;
  }

  UINT32 sub_cap = TSS_TPMCAP_PROP_OWNER;
  UINT32 cap_length = 0;
  ScopedTssMemory cap(context_handle);
  if (TPM_ERROR(result = Tspi_TPM_GetCapability(tpm_handle, TSS_TPMCAP_PROPERTY,
                                       sizeof(sub_cap),
                                       reinterpret_cast<BYTE*>(&sub_cap),
                                       &cap_length, cap.ptr())) == 0) {
    if (cap_length >= (sizeof(TSS_BOOL))) {
      *enabled = true;
      *owned = ((*(reinterpret_cast<TSS_BOOL*>(cap.value()))) != 0);
    }
  } else if (ERROR_CODE(result) == TPM_E_DISABLED) {
    *enabled = false;
  }
}

bool Tpm::CreateEndorsementKey(TSS_HCONTEXT context_handle) {
  TSS_RESULT result;
  TSS_HTPM tpm_handle;
  if (!GetTpm(context_handle, &tpm_handle)) {
    return false;
  }

  ScopedTssKey local_key_handle(context_handle);
  TSS_FLAG init_flags = TSS_KEY_TYPE_LEGACY | TSS_KEY_SIZE_2048;
  if (TPM_ERROR(result = Tspi_Context_CreateObject(context_handle,
                                                   TSS_OBJECT_TYPE_RSAKEY,
                                                   init_flags,
                                                   local_key_handle.ptr()))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_Context_CreateObject";
    return false;
  }

  if (TPM_ERROR(result = Tspi_TPM_CreateEndorsementKey(tpm_handle,
                                                       local_key_handle,
                                                       NULL))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_TPM_CreateEndorsementKey";
    return false;
  }

  return true;
}

bool Tpm::IsEndorsementKeyAvailable(TSS_HCONTEXT context_handle) {
  TSS_RESULT result;
  TSS_HTPM tpm_handle;
  if (!GetTpm(context_handle, &tpm_handle)) {
    return false;
  }

  ScopedTssKey local_key_handle(context_handle);
  if (TPM_ERROR(result = Tspi_TPM_GetPubEndorsementKey(tpm_handle, false, NULL,
                                                    local_key_handle.ptr()))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_TPM_GetPubEndorsementKey";
    return false;
  }

  return true;
}

void Tpm::CreateOwnerPassword(SecureBlob* password) {
  // Generate a random owner password.  The default is a 12-character,
  // hex-encoded password created from 6 bytes of random data.
  SecureBlob random(kOwnerPasswordLength / 2);
  CryptoLib::GetSecureRandom(static_cast<unsigned char*>(random.data()),
                             random.size());
  SecureBlob tpm_password(kOwnerPasswordLength);
  CryptoLib::AsciiEncodeToBuffer(random,
                                 static_cast<char*>(tpm_password.data()),
                                 tpm_password.size());
  password->swap(tpm_password);
}

bool Tpm::TakeOwnership(TSS_HCONTEXT context_handle, int max_timeout_tries,
                        const SecureBlob& owner_password) {
  TSS_RESULT result;
  TSS_HTPM tpm_handle;
  if (!GetTpmWithAuth(context_handle, owner_password, &tpm_handle)) {
    return false;
  }

  ScopedTssKey srk_handle(context_handle);
  TSS_FLAG init_flags = TSS_KEY_TSP_SRK | TSS_KEY_AUTHORIZATION;
  if (TPM_ERROR(result = Tspi_Context_CreateObject(context_handle,
                                          TSS_OBJECT_TYPE_RSAKEY,
                                          init_flags, srk_handle.ptr()))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_Context_CreateObject";
    return false;
  }

  TSS_HPOLICY srk_usage_policy;
  if (TPM_ERROR(result = Tspi_GetPolicyObject(srk_handle, TSS_POLICY_USAGE,
                                              &srk_usage_policy))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_GetPolicyObject";
    return false;
  }

  if (TPM_ERROR(result = Tspi_Policy_SetSecret(srk_usage_policy,
      TSS_SECRET_MODE_PLAIN,
      strlen(kWellKnownSrkTmp),
      const_cast<BYTE *>(reinterpret_cast<const BYTE *>(kWellKnownSrkTmp))))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_Policy_SetSecret";
    return false;
  }

  int retry_count = 0;
  do {
    result = Tspi_TPM_TakeOwnership(tpm_handle, srk_handle, 0);
    retry_count++;
  } while (((result == TDDL_E_TIMEOUT) ||
            (result == (TSS_LAYER_TDDL | TDDL_E_TIMEOUT)) ||
            (result == (TSS_LAYER_TDDL | TDDL_E_IOERROR))) &&
           (retry_count < max_timeout_tries));

  if (result) {
    TPM_LOG(ERROR, result)
        << "Error calling Tspi_TPM_TakeOwnership, attempts: " << retry_count;
    return false;
  }

  return true;
}

bool Tpm::ZeroSrkPassword(TSS_HCONTEXT context_handle,
                          const SecureBlob& owner_password) {
  TSS_RESULT result;
  TSS_HTPM tpm_handle;
  if (!GetTpmWithAuth(context_handle, owner_password, &tpm_handle)) {
    return false;
  }

  ScopedTssKey srk_handle(context_handle);
  TSS_UUID SRK_UUID = TSS_UUID_SRK;
  if (TPM_ERROR(result = Tspi_Context_LoadKeyByUUID(context_handle,
                                                    TSS_PS_TYPE_SYSTEM,
                                                    SRK_UUID,
                                                    srk_handle.ptr()))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_Context_LoadKeyByUUID";
    return false;
  }

  ScopedTssPolicy policy_handle(context_handle);
  if (TPM_ERROR(result = Tspi_Context_CreateObject(context_handle,
                                                   TSS_OBJECT_TYPE_POLICY,
                                                   TSS_POLICY_USAGE,
                                                   policy_handle.ptr()))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_Context_CreateObject";
    return false;
  }

  BYTE new_password[0];
  if (TPM_ERROR(result = Tspi_Policy_SetSecret(policy_handle,
                                               TSS_SECRET_MODE_PLAIN,
                                               0,
                                               new_password))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_Policy_SetSecret";
    return false;
  }

  if (TPM_ERROR(result = Tspi_ChangeAuth(srk_handle,
                                         tpm_handle,
                                         policy_handle))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_ChangeAuth";
    return false;
  }

  return true;
}

bool Tpm::UnrestrictSrk(TSS_HCONTEXT context_handle,
                        const SecureBlob& owner_password) {
  TSS_RESULT result;
  TSS_HTPM tpm_handle;
  if (!GetTpmWithAuth(context_handle, owner_password, &tpm_handle)) {
    return false;
  }

  TSS_BOOL current_status = false;

  if (TPM_ERROR(result = Tspi_TPM_GetStatus(tpm_handle,
                                            TSS_TPMSTATUS_DISABLEPUBSRKREAD,
                                            &current_status))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_TPM_GetStatus";
    return false;
  }

  // If it is currently owner auth (true), set it to SRK auth
  if (current_status) {
    if (TPM_ERROR(result = Tspi_TPM_SetStatus(tpm_handle,
                                              TSS_TPMSTATUS_DISABLEPUBSRKREAD,
                                              false))) {
      TPM_LOG(ERROR, result) << "Error calling Tspi_TPM_SetStatus";
      return false;
    }
  }

  return true;
}

bool Tpm::ChangeOwnerPassword(TSS_HCONTEXT context_handle,
                              const SecureBlob& previous_owner_password,
                              const SecureBlob& owner_password) {
  TSS_RESULT result;
  TSS_HTPM tpm_handle;
  if (!GetTpmWithAuth(context_handle, previous_owner_password, &tpm_handle)) {
    return false;
  }

  ScopedTssPolicy policy_handle(context_handle);
  if (TPM_ERROR(result = Tspi_Context_CreateObject(context_handle,
                                                   TSS_OBJECT_TYPE_POLICY,
                                                   TSS_POLICY_USAGE,
                                                   policy_handle.ptr()))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_Context_CreateObject";
    return false;
  }

  if (TPM_ERROR(result = Tspi_Policy_SetSecret(policy_handle,
      TSS_SECRET_MODE_PLAIN,
      owner_password.size(),
      const_cast<BYTE *>(static_cast<const BYTE *>(
          owner_password.const_data()))))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_Policy_SetSecret";
    return false;
  }

  if (TPM_ERROR(result = Tspi_ChangeAuth(tpm_handle, 0, policy_handle))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_ChangeAuth";
    return false;
  }

  return true;
}

bool Tpm::LoadOwnerPassword(const TpmStatus& tpm_status,
                            chromeos::Blob* owner_password) {
  if (!(tpm_status.flags() & TpmStatus::OWNED_BY_THIS_INSTALL)) {
    return false;
  }
  if ((tpm_status.flags() & TpmStatus::USES_WELL_KNOWN_OWNER)) {
    SecureBlob default_owner_password(sizeof(kTpmWellKnownPassword));
    memcpy(default_owner_password.data(), kTpmWellKnownPassword,
           sizeof(kTpmWellKnownPassword));
    owner_password->swap(default_owner_password);
    return true;
  }
  if (!(tpm_status.flags() & TpmStatus::USES_RANDOM_OWNER) ||
      !tpm_status.has_owner_password()) {
    return false;
  }

  SecureBlob local_owner_password(tpm_status.owner_password().length());
  tpm_status.owner_password().copy(
      static_cast<char*>(local_owner_password.data()),
      tpm_status.owner_password().length(), 0);
  if (!Unseal(local_owner_password, owner_password)) {
    LOG(ERROR) << "Failed to unseal the owner password.";
    return false;
  }
  return true;
}

bool Tpm::StoreOwnerPassword(const chromeos::Blob& owner_password,
                             TpmStatus* tpm_status) {
  // Use PCR0 when sealing the data so that the owner password is only
  // available in the current boot mode.  This helps protect the password from
  // offline attacks until it has been presented and cleared.
  chromeos::SecureBlob sealed_password;
  if (!SealToPCR0(owner_password, &sealed_password)) {
    LOG(ERROR) << "StoreOwnerPassword: Failed to seal owner password.";
    return false;
  }
  tpm_status->set_owner_password(sealed_password.data(),
                                 sealed_password.size());
  return true;
}

bool Tpm::GetTpm(TSS_HCONTEXT context_handle, TSS_HTPM* tpm_handle) {
  TSS_RESULT result;
  TSS_HTPM local_tpm_handle;
  if (TPM_ERROR(result = Tspi_Context_GetTpmObject(context_handle,
                                                   &local_tpm_handle))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_Context_GetTpmObject";
    return false;
  }

  *tpm_handle = local_tpm_handle;
  return true;
}

bool Tpm::GetTpmWithAuth(TSS_HCONTEXT context_handle,
                         const SecureBlob& owner_password,
                         TSS_HTPM* tpm_handle) {
  TSS_RESULT result;
  TSS_HTPM local_tpm_handle;
  if (!GetTpm(context_handle, &local_tpm_handle)) {
    return false;
  }

  TSS_HPOLICY tpm_usage_policy;
  if (TPM_ERROR(result = Tspi_GetPolicyObject(local_tpm_handle,
                                              TSS_POLICY_USAGE,
                                              &tpm_usage_policy))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_GetPolicyObject";
    return false;
  }

  if (TPM_ERROR(result = Tspi_Policy_SetSecret(
                                  tpm_usage_policy,
                                  TSS_SECRET_MODE_PLAIN,
                                  owner_password.size(),
                                  const_cast<BYTE *>(static_cast<const BYTE *>(
                                    owner_password.const_data()))))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_Policy_SetSecret";
    return false;
  }

  *tpm_handle = local_tpm_handle;
  return true;
}

bool Tpm::TestTpmAuth(TSS_HTPM tpm_handle) {
  // Call Tspi_TPM_GetStatus to test the authentication
  TSS_RESULT result;
  TSS_BOOL current_status = false;
  if (TPM_ERROR(result = Tspi_TPM_GetStatus(tpm_handle,
                                            TSS_TPMSTATUS_DISABLED,
                                            &current_status))) {
    return false;
  }
  return true;
}

bool Tpm::GetOwnerPassword(chromeos::Blob* owner_password) {
  bool result = false;
  if (password_sync_lock_.Try()) {
    if (owner_password_.size() != 0) {
      owner_password->assign(owner_password_.begin(), owner_password_.end());
      result = true;
    }
    password_sync_lock_.Release();
  }
  return result;
}

bool Tpm::InitializeTpm(bool* OUT_took_ownership) {
  TpmStatus tpm_status;

  if (!LoadTpmStatus(&tpm_status)) {
    tpm_status.Clear();
    tpm_status.set_flags(TpmStatus::NONE);
  }

  if (OUT_took_ownership) {
    *OUT_took_ownership = false;
  }

  if (is_disabled_) {
    return false;
  }

  ScopedTssContext context_handle;
  if (!(*(context_handle.ptr()) = ConnectContext())) {
    LOG(ERROR) << "Failed to connect to TPM";
    return false;
  }

  SecureBlob default_owner_password(sizeof(kTpmWellKnownPassword));
  memcpy(default_owner_password.data(), kTpmWellKnownPassword,
         sizeof(kTpmWellKnownPassword));

  bool took_ownership = false;
  if (!is_owned_) {
    is_being_owned_ = true;
    file_util::Delete(FilePath(kOpenCryptokiPath), true);
    file_util::Delete(FilePath(kTpmOwnedFile), false);
    file_util::Delete(FilePath(kTpmStatusFile), false);

    if (!IsEndorsementKeyAvailable(context_handle)) {
      if (!CreateEndorsementKey(context_handle)) {
        LOG(ERROR) << "Failed to create endorsement key";
        is_being_owned_ = false;
        return false;
      }
    }

    if (!IsEndorsementKeyAvailable(context_handle)) {
      LOG(ERROR) << "Endorsement key is not available";
      is_being_owned_ = false;
      return false;
    }

    if (!TakeOwnership(context_handle, kMaxTimeoutRetries,
                       default_owner_password)) {
      LOG(ERROR) << "Take Ownership failed";
      is_being_owned_ = false;
      return false;
    }

    is_owned_ = true;
    took_ownership = true;

    tpm_status.set_flags(TpmStatus::OWNED_BY_THIS_INSTALL |
                         TpmStatus::USES_WELL_KNOWN_OWNER |
                         TpmStatus::INSTALL_ATTRIBUTES_NEEDS_OWNER |
                         TpmStatus::ATTESTATION_NEEDS_OWNER);
    tpm_status.clear_owner_password();
    StoreTpmStatus(tpm_status);
  }

  if (OUT_took_ownership) {
    *OUT_took_ownership = took_ownership;
  }

  // Ensure the SRK is available
  TSS_RESULT result;
  TSS_HKEY srk_handle;
  TSS_UUID SRK_UUID = TSS_UUID_SRK;
  if (TPM_ERROR(result = Tspi_Context_LoadKeyByUUID(context_handle,
                                                    TSS_PS_TYPE_SYSTEM,
                                                    SRK_UUID,
                                                    &srk_handle))) {
  } else {
    Tspi_Context_CloseObject(context_handle, srk_handle);
  }

  // If we can open the TPM with the default password, then we still need to
  // zero the SRK password and unrestrict it, then change the owner password.
  TSS_HTPM tpm_handle;
  if (!file_util::PathExists(FilePath(kTpmOwnedFile)) &&
      GetTpmWithAuth(context_handle, default_owner_password, &tpm_handle) &&
      TestTpmAuth(tpm_handle)) {
    if (!ZeroSrkPassword(context_handle, default_owner_password)) {
      LOG(ERROR) << "Couldn't zero SRK password";
      is_being_owned_ = false;
      return false;
    }

    if (!UnrestrictSrk(context_handle, default_owner_password)) {
      LOG(ERROR) << "Couldn't unrestrict the SRK";
      is_being_owned_ = false;
      return false;
    }

    SecureBlob owner_password;
    CreateOwnerPassword(&owner_password);

    tpm_status.set_flags(TpmStatus::OWNED_BY_THIS_INSTALL |
                         TpmStatus::USES_RANDOM_OWNER |
                         TpmStatus::INSTALL_ATTRIBUTES_NEEDS_OWNER |
                         TpmStatus::ATTESTATION_NEEDS_OWNER);
    if (!StoreOwnerPassword(owner_password, &tpm_status)) {
      tpm_status.clear_owner_password();
    }
    StoreTpmStatus(tpm_status);

    if ((result = ChangeOwnerPassword(context_handle, default_owner_password,
                                      owner_password))) {
      password_sync_lock_.Acquire();
      owner_password_.assign(owner_password.begin(), owner_password.end());
      password_sync_lock_.Release();
    }

    file_util::WriteFile(FilePath(kTpmOwnedFile), NULL, 0);
  } else {
    // If we fall through here, then the TPM owned file doesn't exist, but we
    // couldn't auth with the well-known password.  In this case, we must assume
    // that the TPM has already been owned and set to a random password, so
    // touch the TPM owned file.
    if (!file_util::PathExists(FilePath(kTpmOwnedFile))) {
      file_util::WriteFile(FilePath(kTpmOwnedFile), NULL, 0);
    }
  }

  is_being_owned_ = false;
  return true;
}

bool Tpm::GetRandomData(size_t length, chromeos::Blob* data) {
  ScopedTssContext context_handle;
  if ((*(context_handle.ptr()) = ConnectContext()) == 0) {
    LOG(ERROR) << "Could not open the TPM";
    return false;
  }

  TSS_HTPM tpm_handle;
  if (!GetTpm(context_handle, &tpm_handle)) {
    LOG(ERROR) << "Could not get a handle to the TPM.";
    return false;
  }

  TSS_RESULT result;
  SecureBlob random(length);
  ScopedTssMemory tpm_data(context_handle);
  result = Tspi_TPM_GetRandom(tpm_handle, random.size(), tpm_data.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not get random data from the TPM";
    return false;
  }
  memcpy(random.data(), tpm_data.value(), random.size());
  chromeos::SecureMemset(tpm_data.value(), 0, random.size());
  data->swap(random);
  return true;
}

bool Tpm::DestroyNvram(uint32_t index) {
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsOwner(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "Could not open the TPM";
    return false;
  }

  if (!IsNvramDefinedForContext(context_handle, tpm_handle, index)) {
    LOG(INFO) << "NVRAM index is already undefined.";
    return true;
  }

  // Create an NVRAM store object handle.
  TSS_RESULT result;
  ScopedTssNvStore nv_handle(context_handle);
  result = Tspi_Context_CreateObject(context_handle,
                                     TSS_OBJECT_TYPE_NV,
                                     0,
                                     nv_handle.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not acquire an NVRAM object handle";
    return false;
  }

  result = Tspi_SetAttribUint32(nv_handle, TSS_TSPATTRIB_NV_INDEX,
                                0, index);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not set index on NVRAM object: " << index;
    return false;
  }

  if ((result = Tspi_NV_ReleaseSpace(nv_handle))) {
    TPM_LOG(ERROR, result) << "Could not release NVRAM space: " << index;
    return false;
  }

  return true;
}

// TODO(wad) Make this a wrapper around a generic DefineNvram().
bool Tpm::DefineLockOnceNvram(uint32_t index, size_t length) {
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsOwner(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "DefineLockOnceNvram failed to acquire authorization.";
    return false;
  }
  // Create a PCR object handle.
  TSS_RESULT result;
  ScopedTssPcrs pcrs_handle(context_handle);
  if (TPM_ERROR(result = Tspi_Context_CreateObject(context_handle,
                                                   TSS_OBJECT_TYPE_PCRS,
                                                   TSS_PCRS_STRUCT_INFO_SHORT,
                                                   pcrs_handle.ptr()))) {
    TPM_LOG(ERROR, result) << "Could not acquire PCR object handle";
    return false;
  }
  // Read PCR0.
  UINT32 pcr_len;
  ScopedTssMemory pcr_value(context_handle);
  if (TPM_ERROR(result = Tspi_TPM_PcrRead(tpm_handle, kTpmBootPCR,
                                          &pcr_len, pcr_value.ptr()))) {
    TPM_LOG(ERROR, result) << "Could not read PCR0 value";
    return false;
  }
  // Include PCR0 value in PcrComposite.
  if (TPM_ERROR(result = Tspi_PcrComposite_SetPcrValue(pcrs_handle,
                                                       kTpmBootPCR,
                                                       pcr_len,
                                                       pcr_value.value()))) {
    TPM_LOG(ERROR, result) << "Could not set value for PCR0 in PCR handle";
    return false;
  }
  // Set locality.
  if (TPM_ERROR(result = Tspi_PcrComposite_SetPcrLocality(pcrs_handle,
                                                          kTpmPCRLocality))) {
    TPM_LOG(ERROR, result) << "Could not set locality for PCR0 in PCR handle";
    return false;
  }

  // Create an NVRAM store object handle.
  ScopedTssNvStore nv_handle(context_handle);
  result = Tspi_Context_CreateObject(context_handle,
                                     TSS_OBJECT_TYPE_NV,
                                     0,
                                     nv_handle.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not acquire an NVRAM object handle";
    return false;
  }

  result = Tspi_SetAttribUint32(nv_handle, TSS_TSPATTRIB_NV_INDEX,
                                0, index);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not set index on NVRAM object: " << index;
    return false;
  }

  result = Tspi_SetAttribUint32(nv_handle, TSS_TSPATTRIB_NV_DATASIZE,
                                0, length);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not set size on NVRAM object: " << length;
    return false;
  }

  // Do not restrict to owner auth for write - just ensure it is lockable.
  result = Tspi_SetAttribUint32(nv_handle, TSS_TSPATTRIB_NV_PERMISSIONS,
                                0, TPM_NV_PER_WRITEDEFINE);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not set PER_WRITEDEFINE on NVRAM object";
    return false;
  }

  if (TPM_ERROR(result = Tspi_NV_DefineSpace(nv_handle,
                                             pcrs_handle, pcrs_handle))) {
    TPM_LOG(ERROR, result) << "Could not define NVRAM space: " << index;
    return false;
  }

  return true;
}

bool Tpm::IsNvramDefined(uint32_t index) {
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsUser(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "Could not connect to the TPM";
    return false;
  }
  return IsNvramDefinedForContext(context_handle, tpm_handle, index);
}

bool Tpm::IsNvramDefinedForContext(TSS_HCONTEXT context_handle,
                                TSS_HTPM tpm_handle,
                                uint32_t index) {
  TSS_RESULT result;
  UINT32 nv_list_data_length = 0;
  ScopedTssMemory nv_list_data(context_handle);
  if (TPM_ERROR(result = Tspi_TPM_GetCapability(tpm_handle,
                                                TSS_TPMCAP_NV_LIST,
                                                0,
                                                NULL,
                                                &nv_list_data_length,
                                                nv_list_data.ptr()))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_TPM_GetCapability";
    return false;
  }

  // Walk the list and check if the index exists.
  UINT32* nv_list = reinterpret_cast<UINT32*>(nv_list_data.value());
  UINT32 nv_list_length = nv_list_data_length / sizeof(UINT32);
  index = htonl(index);  // TPM data is network byte order.
  for (UINT32 i = 0; i < nv_list_length; ++i) {
    // TODO(wad) add a NvramList method.
    if (index == nv_list[i])
      return true;
  }
  return false;
}

unsigned int Tpm::GetNvramSize(uint32_t index) {
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsUser(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "Could not connect to the TPM";
    return 0;
  }

  return GetNvramSizeForContext(context_handle, tpm_handle, index);
}

unsigned int Tpm::GetNvramSizeForContext(TSS_HCONTEXT context_handle,
                                         TSS_HTPM tpm_handle,
                                         uint32_t index) {
  unsigned int count = 0;
  TSS_RESULT result;

  UINT32 nv_index_data_length = 0;
  ScopedTssMemory nv_index_data(context_handle);
  if (TPM_ERROR(result = Tspi_TPM_GetCapability(tpm_handle,
                                                TSS_TPMCAP_NV_INDEX,
                                                sizeof(index),
                                                reinterpret_cast<BYTE*>(&index),
                                                &nv_index_data_length,
                                                nv_index_data.ptr()))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_TPM_GetCapability";
    return count;
  }
  if (nv_index_data_length == 0) {
    return count;
  }
  // TPM_NV_DATA_PUBLIC->dataSize is the last element in the struct.
  // Since packing the struct still doesn't eliminate inconsistencies between
  // the API and the hardware, this is the safest way to extract the data.
  uint32_t* nv_data_public = reinterpret_cast<uint32_t*>(
                               nv_index_data.value() + nv_index_data_length -
                               sizeof(UINT32));
  return htonl(*nv_data_public);
}

bool Tpm::IsNvramLocked(uint32_t index) {
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsUser(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "Could not connect to the TPM";
    return 0;
  }

  return IsNvramLockedForContext(context_handle, tpm_handle, index);
}

bool Tpm::IsNvramLockedForContext(TSS_HCONTEXT context_handle,
                                  TSS_HTPM tpm_handle,
                                  uint32_t index) {
  unsigned int count = 0;
  TSS_RESULT result;
  UINT32 nv_index_data_length = 0;
  ScopedTssMemory nv_index_data(context_handle);
  if (TPM_ERROR(result = Tspi_TPM_GetCapability(tpm_handle,
                                                TSS_TPMCAP_NV_INDEX,
                                                sizeof(index),
                                                reinterpret_cast<BYTE*>(&index),
                                                &nv_index_data_length,
                                                nv_index_data.ptr()))) {
    TPM_LOG(ERROR, result) << "Error calling Tspi_TPM_GetCapability";
    return count;
  }
  if (nv_index_data_length < sizeof(UINT32) + sizeof(TPM_BOOL)) {
    return count;
  }
  // TPM_NV_DATA_PUBLIC->bWriteDefine is the second to last element in the
  // struct.  Since packing the struct still doesn't eliminate inconsistencies
  // between the API and the hardware, this is the safest way to extract the
  // data.
  // TODO(wad) share data with GetNvramSize() to avoid extra calls.
  uint32_t* nv_data_public = reinterpret_cast<uint32_t*>(
                               nv_index_data.value() + nv_index_data_length -
                               (sizeof(UINT32) + sizeof(TPM_BOOL)));
  return (*nv_data_public != 0);
}

bool Tpm::ReadNvram(uint32_t index, SecureBlob* blob) {
  // TODO(wad) longer term, add support for checking when a space is restricted
  //           and needs an authenticated handle.
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsUser(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "Could not connect to the TPM";
    return false;
  }

  if (!IsNvramDefinedForContext(context_handle, tpm_handle, index)) {
    LOG(ERROR) << "Cannot read from non-existent NVRAM space.";
    return false;
  }

  // Create an NVRAM store object handle.
  TSS_RESULT result;
  ScopedTssNvStore nv_handle(context_handle);
  result = Tspi_Context_CreateObject(context_handle,
                                     TSS_OBJECT_TYPE_NV,
                                     0,
                                     nv_handle.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not acquire an NVRAM object handle";
    return false;
  }
  result = Tspi_SetAttribUint32(nv_handle, TSS_TSPATTRIB_NV_INDEX,
                                0, index);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not set index on NVRAM object: " << index;
    return false;
  }

  unsigned int size = GetNvramSizeForContext(context_handle, tpm_handle, index);
  if (size == 0) {
    LOG(ERROR) << "NvramSize is too small.";
    // TODO(wad) get attrs so we can explore more
    return false;
  }
  ScopedTssMemory space_data(context_handle);
  if ((result = Tspi_NV_ReadValue(nv_handle, 0, &size, space_data.ptr()))) {
    TPM_LOG(ERROR, result) << "Could not read to NVRAM space: " << index;
    return false;
  }
  if (!space_data.value()) {
    LOG(ERROR) << "No data read from NVRAM space: " << index;
    return false;
  }
  blob->resize(size);
  memcpy(blob->data(), space_data.value(), size);

  return true;
}

bool Tpm::WriteNvram(uint32_t index, const SecureBlob& blob) {
  // TODO(wad) longer term, add support for checking when a space is restricted
  //           and needs an authenticated handle.
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsUser(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "Could not connect to the TPM";
    return 0;
  }

  // Create an NVRAM store object handle.
  TSS_RESULT result;
  ScopedTssNvStore nv_handle(context_handle);
  result = Tspi_Context_CreateObject(context_handle,
                                     TSS_OBJECT_TYPE_NV,
                                     0,
                                     nv_handle.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not acquire an NVRAM object handle";
    return false;
  }

  result = Tspi_SetAttribUint32(nv_handle, TSS_TSPATTRIB_NV_INDEX,
                                0, index);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not set index on NVRAM object: " << index;
    return false;
  }

  scoped_array<BYTE> nv_data(new BYTE[blob.size()]);
  memcpy(nv_data.get(), blob.const_data(), blob.size());
  result = Tspi_NV_WriteValue(nv_handle, 0, blob.size(), nv_data.get());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Could not write to NVRAM space: " << index;
    return false;
  }

  return true;
}

void Tpm::ClearStoredOwnerPassword() {
  TpmStatus tpm_status;
  if (LoadTpmStatus(&tpm_status)) {
    int32 dependency_flags = TpmStatus::INSTALL_ATTRIBUTES_NEEDS_OWNER |
                             TpmStatus::ATTESTATION_NEEDS_OWNER;
    if (tpm_status.flags() & dependency_flags) {
      // The password is still needed, do not clear.
      return;
    }
    if (tpm_status.has_owner_password()) {
      tpm_status.clear_owner_password();
      StoreTpmStatus(tpm_status);
    }
  }
  password_sync_lock_.Acquire();
  owner_password_.resize(0);
  password_sync_lock_.Release();
}

void Tpm::RemoveOwnerDependency(TpmOwnerDependency dependency) {
  int32 flag_to_clear = TpmStatus::NONE;
  switch (dependency) {
    case kInstallAttributes:
      flag_to_clear = TpmStatus::INSTALL_ATTRIBUTES_NEEDS_OWNER;
      break;
    case kAttestation:
      flag_to_clear = TpmStatus::ATTESTATION_NEEDS_OWNER;
      break;
    default:
      CHECK(false);
  }
  TpmStatus tpm_status;
  if (!LoadTpmStatus(&tpm_status))
    return;
  tpm_status.set_flags(tpm_status.flags() & ~flag_to_clear);
  StoreTpmStatus(tpm_status);
}

bool Tpm::LoadTpmStatus(TpmStatus* serialized) {
  FilePath tpm_status_file(kTpmStatusFile);
  if (!file_util::PathExists(tpm_status_file)) {
    return false;
  }
  SecureBlob file_data;
  if (!platform_->ReadFile(kTpmStatusFile, &file_data)) {
    return false;
  }
  if (!serialized->ParseFromArray(
           static_cast<const unsigned char*>(file_data.data()),
           file_data.size())) {
    return false;
  }
  return true;
}

bool Tpm::StoreTpmStatus(const TpmStatus& serialized) {
  Platform platform;
  int old_mask = platform.SetMask(kDefaultUmask);
  FilePath tpm_status_file(kTpmStatusFile);
  if (file_util::PathExists(tpm_status_file)) {
    do {
      int64 file_size;
      if (!file_util::GetFileSize(tpm_status_file, &file_size)) {
        break;
      }
      SecureBlob random;
      if (!GetRandomData(file_size, &random)) {
        break;
      }
      FILE* file = file_util::OpenFile(tpm_status_file, "wb+");
      if (!file) {
        break;
      }
      if (fwrite(random.const_data(), 1, random.size(), file) !=
          random.size()) {
        file_util::CloseFile(file);
        break;
      }
      file_util::CloseFile(file);
    } while (false);
    file_util::Delete(tpm_status_file, false);
  }
  SecureBlob final_blob(serialized.ByteSize());
  serialized.SerializeWithCachedSizesToArray(
      static_cast<google::protobuf::uint8*>(final_blob.data()));
  unsigned int data_written = file_util::WriteFile(
      tpm_status_file,
      static_cast<const char*>(final_blob.const_data()),
      final_blob.size());

  if (data_written != final_blob.size()) {
    platform.SetMask(old_mask);
    return false;
  }
  platform.SetMask(old_mask);
  return true;
}

Value* Tpm::GetStatusValue(TpmInit* init) {
  DictionaryValue* dv = new DictionaryValue();
  TpmStatusInfo status;
  GetStatus(true, &status);

  dv->SetBoolean("enabled", status.Enabled);
  dv->SetBoolean("owned", status.Owned);
  dv->SetBoolean("being_owned", status.BeingOwned);
  dv->SetBoolean("can_connect", status.CanConnect);
  dv->SetBoolean("can_load_srk", status.CanLoadSrk);
  dv->SetBoolean("can_load_srk_pubkey", status.CanLoadSrkPublicKey);
  dv->SetBoolean("has_cryptohome_key", status.HasCryptohomeKey);
  dv->SetBoolean("can_encrypt", status.CanEncrypt);
  dv->SetBoolean("can_decrypt", status.CanDecrypt);
  dv->SetBoolean("has_context", status.ThisInstanceHasContext);
  dv->SetBoolean("has_key_handle", status.ThisInstanceHasKeyHandle);
  dv->SetInteger("last_error", status.LastTpmError);

  if (init) {
    dv->SetBoolean("enabled", init->IsTpmEnabled());
    dv->SetBoolean("owned", init->IsTpmOwned());
    dv->SetBoolean("being_owned", init->IsTpmBeingOwned());
  }

  return dv;
}

bool Tpm::GetEndorsementPublicKey(chromeos::SecureBlob* ek_public_key) {
  // Connect to the TPM as the owner.
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsOwner(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "GetEndorsementPublicKey: Could not connect to the TPM.";
    return false;
  }
  // Get a handle to the EK public key.
  ScopedTssKey ek_public_key_object(context_handle);
  TSS_RESULT result = Tspi_TPM_GetPubEndorsementKey(tpm_handle, true, NULL,
                                                    ek_public_key_object.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "GetEndorsementPublicKey: Failed to get key.";
    return false;
  }
  // Get the public key in TPM_PUBKEY form.
  UINT32 ek_public_key_length = 0;
  ScopedTssMemory ek_public_key_buf(context_handle);
  result = Tspi_GetAttribData(ek_public_key_object,
                              TSS_TSPATTRIB_KEY_BLOB,
                              TSS_TSPATTRIB_KEYBLOB_PUBLIC_KEY,
                              &ek_public_key_length,
                              ek_public_key_buf.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "GetEndorsementPublicKey: Failed to read key.";
    return false;
  }
  chromeos::SecureBlob ek_public_key_blob(ek_public_key_buf.value(),
                                          ek_public_key_length);
  chromeos::SecureMemset(ek_public_key_buf.value(), 0, ek_public_key_length);

  // Get the public key in DER encoded form.
  if (!ConvertPublicKeyToDER(ek_public_key_blob, ek_public_key)) {
    return false;
  }
  return true;
}

bool Tpm::DecryptIdentityRequest(RSA* pca_key,
                                 const chromeos::SecureBlob& request,
                                 chromeos::SecureBlob* identity_binding,
                                 chromeos::SecureBlob* endorsement_credential,
                                 chromeos::SecureBlob* platform_credential,
                                 chromeos::SecureBlob* conformance_credential) {
  // Parse the serialized TPM_IDENTITY_REQ structure.
  UINT64 offset = 0;
  BYTE* buffer = const_cast<BYTE*>(&request.front());
  TPM_IDENTITY_REQ request_parsed;
  TSS_RESULT result = Trspi_UnloadBlob_IDENTITY_REQ(&offset, buffer,
                                                    &request_parsed);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Failed to parse identity request.";
    return false;
  }
  scoped_ptr_malloc<BYTE> scoped_asym_blob(request_parsed.asymBlob);
  scoped_ptr_malloc<BYTE> scoped_sym_blob(request_parsed.symBlob);

  // Decrypt the symmetric key.
  unsigned char key_buffer[kDefaultTpmRsaKeyBits / 8];
  int key_length = RSA_private_decrypt(request_parsed.asymSize,
                                       request_parsed.asymBlob,
                                       key_buffer, pca_key, RSA_PKCS1_PADDING);
  if (key_length == -1) {
    LOG(ERROR) << "Failed to decrypt identity request key.";
    return false;
  }
  TPM_SYMMETRIC_KEY symmetric_key;
  offset = 0;
  result = Trspi_UnloadBlob_SYMMETRIC_KEY(&offset, key_buffer, &symmetric_key);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Failed to parse symmetric key.";
    return false;
  }
  scoped_ptr_malloc<BYTE> scoped_sym_key(symmetric_key.data);

  // Decrypt the request with the symmetric key.
  chromeos::SecureBlob proof_serial;
  proof_serial.resize(request_parsed.symSize);
  UINT32 proof_serial_length = proof_serial.size();
  result = Trspi_SymDecrypt(symmetric_key.algId, TR_SYM_MODE_CBC,
                            symmetric_key.data, NULL,
                            request_parsed.symBlob, request_parsed.symSize,
                            &proof_serial.front(), &proof_serial_length);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Failed to decrypt identity request.";
    return false;
  }

  // Parse the serialized TPM_IDENTITY_PROOF structure.
  TPM_IDENTITY_PROOF proof;
  offset = 0;
  result = Trspi_UnloadBlob_IDENTITY_PROOF(&offset, &proof_serial.front(),
                                           &proof);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Failed to parse identity proof.";
    return false;
  }
  scoped_ptr_malloc<BYTE> scoped_label(proof.labelArea);
  scoped_ptr_malloc<BYTE> scoped_binding(proof.identityBinding);
  scoped_ptr_malloc<BYTE> scoped_endorsement(proof.endorsementCredential);
  scoped_ptr_malloc<BYTE> scoped_platform(proof.platformCredential);
  scoped_ptr_malloc<BYTE> scoped_conformance(proof.conformanceCredential);
  scoped_ptr_malloc<BYTE> scoped_key(proof.identityKey.pubKey.key);
  scoped_ptr_malloc<BYTE> scoped_parms(proof.identityKey.algorithmParms.parms);

  identity_binding->assign(&proof.identityBinding[0],
                           &proof.identityBinding[proof.identityBindingSize]);
  chromeos::SecureMemset(proof.identityBinding, 0, proof.identityBindingSize);
  endorsement_credential->assign(
      &proof.endorsementCredential[0],
      &proof.endorsementCredential[proof.endorsementSize]);
  chromeos::SecureMemset(proof.endorsementCredential, 0, proof.endorsementSize);
  platform_credential->assign(&proof.platformCredential[0],
                              &proof.platformCredential[proof.platformSize]);
  chromeos::SecureMemset(proof.platformCredential, 0, proof.platformSize);
  conformance_credential->assign(
      &proof.conformanceCredential[0],
      &proof.conformanceCredential[proof.conformanceSize]);
  chromeos::SecureMemset(proof.conformanceCredential, 0, proof.conformanceSize);
  return true;
}

bool Tpm::ConvertPublicKeyToDER(const chromeos::SecureBlob& public_key,
                                chromeos::SecureBlob* public_key_der) {
  // Parse the serialized TPM_PUBKEY.
  UINT64 offset = 0;
  BYTE* buffer = const_cast<BYTE*>(&public_key.front());
  TPM_PUBKEY parsed;
  TSS_RESULT result = Trspi_UnloadBlob_PUBKEY(&offset, buffer, &parsed);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "Failed to parse TPM_PUBKEY.";
    return false;
  }
  scoped_ptr_malloc<BYTE> scoped_key(parsed.pubKey.key);
  scoped_ptr_malloc<BYTE> scoped_parms(parsed.algorithmParms.parms);
  TPM_RSA_KEY_PARMS* parms =
      reinterpret_cast<TPM_RSA_KEY_PARMS*>(parsed.algorithmParms.parms);
  RSA* rsa = RSA_new();
  CHECK(rsa);
  // Get the public exponent.
  if (parms->exponentSize == 0) {
    rsa->e = BN_new();
    CHECK(rsa->e);
    BN_set_word(rsa->e, kWellKnownExponent);
  } else {
    rsa->e = BN_bin2bn(parms->exponent, parms->exponentSize, NULL);
    CHECK(rsa->e);
  }
  // Get the modulus.
  rsa->n = BN_bin2bn(parsed.pubKey.key, parsed.pubKey.keyLength, NULL);
  CHECK(rsa->n);

  // DER encode.
  int der_length = i2d_RSAPublicKey(rsa, NULL);
  if (der_length < 0) {
    LOG(ERROR) << "Failed to DER-encode public key.";
    return false;
  }
  public_key_der->resize(der_length);
  unsigned char* der_buffer = &public_key_der->front();
  der_length = i2d_RSAPublicKey(rsa, &der_buffer);
  if (der_length < 0) {
    LOG(ERROR) << "Failed to DER-encode public key.";
    return false;
  }
  public_key_der->resize(der_length);
  RSA_free(rsa);
  return true;
}

bool Tpm::MakeIdentity(chromeos::SecureBlob* identity_public_key,
                       chromeos::SecureBlob* identity_key_blob,
                       chromeos::SecureBlob* identity_binding,
                       chromeos::SecureBlob* identity_label,
                       chromeos::SecureBlob* pca_public_key,
                       chromeos::SecureBlob* endorsement_credential,
                       chromeos::SecureBlob* platform_credential,
                       chromeos::SecureBlob* conformance_credential) {
  CHECK(identity_public_key && identity_key_blob && identity_binding &&
        identity_label && pca_public_key && endorsement_credential &&
        platform_credential && conformance_credential);
  // Connect to the TPM as the owner.
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsOwner(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "MakeIdentity: Could not connect to the TPM.";
    return false;
  }
  // Load the Storage Root Key.
  TSS_RESULT result;
  ScopedTssKey srk_handle(context_handle);
  if (!LoadSrk(context_handle, srk_handle.ptr(), &result)) {
    TPM_LOG(INFO, result) << "MakeIdentity: Cannot load SRK.";
    return false;
  }
  // The easiest way to create an AIK and get all the information we need out of
  // the TPM is to call Tspi_TPM_CollateIdentityRequest and disassemble the
  // request. For that we need to spoof a Privacy CA (PCA).
  class ScopedRSAKey {
   public:
    ScopedRSAKey(RSA* rsa) : rsa_(rsa) {}
    ~ScopedRSAKey() { if (rsa_) RSA_free(rsa_); }
    RSA* get() { return rsa_; }
   private:
    RSA* rsa_;
  } fake_pca_key(RSA_generate_key(kDefaultTpmRsaKeyBits, kWellKnownExponent,
                                  NULL, NULL));
  if (!fake_pca_key.get()) {
    LOG(ERROR) << "MakeIdentity: Failed to generate local key pair.";
    return false;
  }
  unsigned char modulus_buffer[kDefaultTpmRsaKeyBits/8];
  BN_bn2bin(fake_pca_key.get()->n, modulus_buffer);

  // Create a TSS object for the fake PCA public key.
  ScopedTssKey pca_public_key_object(context_handle);
  UINT32 pca_key_flags = kDefaultTpmRsaKeyFlag |
                         TSS_KEY_TYPE_LEGACY |
                         TSS_KEY_MIGRATABLE;
  result = Tspi_Context_CreateObject(context_handle, TSS_OBJECT_TYPE_RSAKEY,
                                     pca_key_flags,
                                     pca_public_key_object.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "MakeIdentity: Cannot create PCA public key.";
    return false;
  }
  result = Tspi_SetAttribData(pca_public_key_object,
                              TSS_TSPATTRIB_RSAKEY_INFO,
                              TSS_TSPATTRIB_KEYINFO_RSA_MODULUS,
                              arraysize(modulus_buffer), modulus_buffer);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "MakeIdentity: Cannot create PCA public key 2.";
    return false;
  }
  result = Tspi_SetAttribUint32(pca_public_key_object,
                                TSS_TSPATTRIB_KEY_INFO,
                                TSS_TSPATTRIB_KEYINFO_ENCSCHEME,
                                TSS_ES_RSAESPKCSV15);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "MakeIdentity: Cannot create PCA public key 3.";
    return false;
  }

  // Get the fake PCA public key in serialized TPM_PUBKEY form.
  UINT32 pca_public_key_length = 0;
  ScopedTssMemory pca_public_key_buf(context_handle);
  result = Tspi_GetAttribData(pca_public_key_object,
                              TSS_TSPATTRIB_KEY_BLOB,
                              TSS_TSPATTRIB_KEYBLOB_PUBLIC_KEY,
                              &pca_public_key_length, pca_public_key_buf.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "MakeIdentity: Failed to read PCA public key.";
    return false;
  }
  pca_public_key->assign(&pca_public_key_buf.value()[0],
                         &pca_public_key_buf.value()[pca_public_key_length]);

  // Construct an arbitrary unicode label.
  const char* label_text = "ChromeOS_AIK_1BJNAMQDR4RH44F4ET2KPAOMJMO043K1";
  BYTE* label_ascii =
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(label_text));
  unsigned int label_size = strlen(label_text);
  scoped_ptr_malloc<BYTE> label(
      Trspi_Native_To_UNICODE(label_ascii, &label_size));
  identity_label->assign(&label.get()[0],
                         &label.get()[label_size]);

  // Initialize a key object to hold the new identity key.
  ScopedTssKey identity_key(context_handle);
  UINT32 identity_key_flags = kDefaultTpmRsaKeyFlag |
                              TSS_KEY_TYPE_IDENTITY |
                              TSS_KEY_VOLATILE |
                              TSS_KEY_NOT_MIGRATABLE;
  result = Tspi_Context_CreateObject(context_handle, TSS_OBJECT_TYPE_RSAKEY,
                                     identity_key_flags, identity_key.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "MakeIdentity: Failed to create key object.";
    return false;
  }
  // Create the identity and receive the request intended for the PCA.
  UINT32 request_length = 0;
  ScopedTssMemory request(context_handle);
  result = Tspi_TPM_CollateIdentityRequest(tpm_handle, srk_handle,
                                           pca_public_key_object,
                                           label_size, label.get(),
                                           identity_key, TSS_ALG_3DES,
                                           &request_length, request.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "MakeIdentity: Failed to make identity.";
    return false;
  }
  // Decrypt and parse the identity request.
  chromeos::SecureBlob request_blob(request.value(), request_length);
  if (!DecryptIdentityRequest(fake_pca_key.get(), request_blob,
                              identity_binding, endorsement_credential,
                              platform_credential, conformance_credential)) {
    LOG(ERROR) << "MakeIdentity: Failed to decrypt the identity request.";
    return false;
  }
  chromeos::SecureMemset(request.value(), 0, request_length);

  // Get the AIK public key in DER encoded form.
  UINT32 identity_public_key_length = 0;
  ScopedTssMemory identity_public_key_buf(context_handle);
  result = Tspi_GetAttribData(identity_key,
                              TSS_TSPATTRIB_KEY_BLOB,
                              TSS_TSPATTRIB_KEYBLOB_PUBLIC_KEY,
                              &identity_public_key_length,
                              identity_public_key_buf.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "MakeIdentity: Failed to read public key.";
    return false;
  }
  chromeos::SecureBlob identity_public_key_blob(identity_public_key_buf.value(),
                                                identity_public_key_length);
  chromeos::SecureMemset(identity_public_key_buf.value(), 0,
                         identity_public_key_length);
  if (!ConvertPublicKeyToDER(identity_public_key_blob, identity_public_key)) {
    return false;
  }

  // Get the AIK blob so we can load it later.
  UINT32 blob_length = 0;
  ScopedTssMemory blob(context_handle);
  result = Tspi_GetAttribData(identity_key,
                              TSS_TSPATTRIB_KEY_BLOB,
                              TSS_TSPATTRIB_KEYBLOB_BLOB,
                              &blob_length, blob.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "MakeIdentity: Failed to read identity key blob.";
    return false;
  }
  identity_key_blob->assign(&blob.value()[0],
                            &blob.value()[blob_length]);
  chromeos::SecureMemset(blob.value(), 0, blob_length);

  return true;
}

bool Tpm::QuotePCR0(const chromeos::SecureBlob& identity_key_blob,
                    const chromeos::SecureBlob& external_data,
                    chromeos::SecureBlob* pcr_value,
                    chromeos::SecureBlob* quoted_data,
                    chromeos::SecureBlob* quote) {
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsUser(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "QuotePCR0: Failed to connect to the TPM.";
    return false;
  }
  // Load the Storage Root Key.
  TSS_RESULT result;
  ScopedTssKey srk_handle(context_handle);
  if (!LoadSrk(context_handle, srk_handle.ptr(), &result)) {
    TPM_LOG(INFO, result) << "QuotePCR0: Failed to load SRK.";
    return false;
  }
  // Load the AIK (which is wrapped by the SRK).
  ScopedTssKey identity_key(context_handle);
  result = Tspi_Context_LoadKeyByBlob(
      context_handle, srk_handle, identity_key_blob.size(),
      const_cast<BYTE*>(&identity_key_blob.front()), identity_key.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "QuotePCR0: Failed to load AIK.";
    return false;
  }
  // Create a PCRS object and select index 0.
  ScopedTssPcrs pcrs(context_handle);
  result = Tspi_Context_CreateObject(context_handle, TSS_OBJECT_TYPE_PCRS,
                                     TSS_PCRS_STRUCT_INFO, pcrs.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "QuotePCR0: Failed to create PCRS object.";
    return false;
  }
  result = Tspi_PcrComposite_SelectPcrIndex(pcrs, 0);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "QuotePCR0: Failed to select PCR0.";
    return false;
  }
  // Generate the quote.
  TSS_VALIDATION validation;
  memset(&validation, 0, sizeof(validation));
  validation.ulExternalDataLength = external_data.size();
  validation.rgbExternalData = const_cast<BYTE*>(&external_data.front());
  result = Tspi_TPM_Quote(tpm_handle, identity_key, pcrs, &validation);
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "QuotePCR0: Failed to generate quote.";
    return false;
  }
  ScopedTssMemory scoped_quoted_data(0, validation.rgbData);
  ScopedTssMemory scoped_quote(0, validation.rgbValidationData);

  // Get the PCR value that was quoted.
  ScopedTssMemory pcr_value_buffer;
  UINT32 pcr_value_length = 0;
  result = Tspi_PcrComposite_GetPcrValue(pcrs, 0, &pcr_value_length,
                                         pcr_value_buffer.ptr());
  if (TPM_ERROR(result)) {
    TPM_LOG(ERROR, result) << "QuotePCR0: Failed to get PCR value.";
    return false;
  }
  pcr_value->assign(&pcr_value_buffer.value()[0],
                    &pcr_value_buffer.value()[pcr_value_length]);
  // Get the data that was quoted.
  quoted_data->assign(&validation.rgbData[0],
                      &validation.rgbData[validation.ulDataLength]);
  // Get the quote.
  quote->assign(
      &validation.rgbValidationData[0],
      &validation.rgbValidationData[validation.ulValidationDataLength]);
  return true;
}

bool Tpm::SealToPCR0(const chromeos::Blob& value,
                     chromeos::Blob* sealed_value) {
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsUser(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "SealToPCR0: Failed to connect to the TPM.";
    return false;
  }
  // Load the Storage Root Key.
  TSS_RESULT result;
  ScopedTssKey srk_handle(context_handle);
  if (!LoadSrk(context_handle, srk_handle.ptr(), &result)) {
    TPM_LOG(INFO, result) << "SealToPCR0: Failed to load SRK.";
    return false;
  }

  // Check the SRK public key
  unsigned int size_n = 0;
  ScopedTssMemory public_srk(context_handle);
  if (TPM_ERROR(result = Tspi_Key_GetPubKey(srk_handle, &size_n,
                                            public_srk.ptr()))) {
    TPM_LOG(ERROR, result) << "SealToPCR0: Unable to get the SRK public key";
    return false;
  }

  // Create a PCRS object which holds the value of PCR0.
  ScopedTssPcrs pcrs_handle(context_handle);
  if (TPM_ERROR(result = Tspi_Context_CreateObject(context_handle,
                                                   TSS_OBJECT_TYPE_PCRS,
                                                   TSS_PCRS_STRUCT_INFO,
                                                   pcrs_handle.ptr()))) {
    TPM_LOG(ERROR, result)
        << "SealToPCR0: Error calling Tspi_Context_CreateObject";
    return false;
  }

  // Create a ENCDATA object to receive the sealed data.
  UINT32 pcr_len = 0;
  ScopedTssMemory pcr_value(context_handle);
  Tspi_TPM_PcrRead(tpm_handle, 0, &pcr_len, pcr_value.ptr());
  Tspi_PcrComposite_SetPcrValue(pcrs_handle, 0, pcr_len, pcr_value.value());

  ScopedTssKey enc_handle(context_handle);
  if (TPM_ERROR(result = Tspi_Context_CreateObject(context_handle,
                                                   TSS_OBJECT_TYPE_ENCDATA,
                                                   TSS_ENCDATA_SEAL,
                                                   enc_handle.ptr()))) {
    TPM_LOG(ERROR, result)
        << "SealToPCR0: Error calling Tspi_Context_CreateObject";
    return false;
  }

  // Seal the given value with the SRK.
  if (TPM_ERROR(result = Tspi_Data_Seal(enc_handle,
                                        srk_handle,
                                        value.size(),
                                        const_cast<BYTE *>(&value[0]),
                                        pcrs_handle))) {
    TPM_LOG(ERROR, result) << "SealToPCR0: Error calling Tspi_Data_Seal";
    return false;
  }

  // Extract the sealed value.
  ScopedTssMemory enc_data(context_handle);
  UINT32 enc_data_length = 0;
  if (TPM_ERROR(result = Tspi_GetAttribData(enc_handle,
                                            TSS_TSPATTRIB_ENCDATA_BLOB,
                                            TSS_TSPATTRIB_ENCDATABLOB_BLOB,
                                            &enc_data_length,
                                            enc_data.ptr()))) {
    TPM_LOG(ERROR, result) << "SealToPCR0: Error calling Tspi_GetAttribData";
    return false;
  }
  sealed_value->assign(&enc_data.value()[0],
                       &enc_data.value()[enc_data_length]);
  return true;
}

bool Tpm::Unseal(const chromeos::Blob& sealed_value, chromeos::Blob* value) {
  ScopedTssContext context_handle;
  TSS_HTPM tpm_handle;
  if (!ConnectContextAsUser(context_handle.ptr(), &tpm_handle)) {
    LOG(ERROR) << "Unseal: Failed to connect to the TPM.";
    return false;
  }
  // Load the Storage Root Key.
  TSS_RESULT result;
  ScopedTssKey srk_handle(context_handle);
  if (!LoadSrk(context_handle, srk_handle.ptr(), &result)) {
    TPM_LOG(INFO, result) << "Unseal: Failed to load SRK.";
    return false;
  }

  // Create an ENCDATA object with the sealed value.
  ScopedTssKey enc_handle(context_handle);
  if (TPM_ERROR(result = Tspi_Context_CreateObject(context_handle,
                                                   TSS_OBJECT_TYPE_ENCDATA,
                                                   TSS_ENCDATA_SEAL,
                                                   enc_handle.ptr()))) {
    TPM_LOG(ERROR, result) << "Unseal: Error calling Tspi_Context_CreateObject";
    return false;
  }

  if (TPM_ERROR(result = Tspi_SetAttribData(enc_handle,
      TSS_TSPATTRIB_ENCDATA_BLOB,
      TSS_TSPATTRIB_ENCDATABLOB_BLOB,
      sealed_value.size(),
      const_cast<BYTE *>(sealed_value.data())))) {
    TPM_LOG(ERROR, result) << "Unseal: Error calling Tspi_SetAttribData";
    return false;
  }

  // Unseal using the SRK.
  ScopedTssMemory dec_data(context_handle);
  UINT32 dec_data_length = 0;
  if (TPM_ERROR(result = Tspi_Data_Unseal(enc_handle,
                                          srk_handle,
                                          &dec_data_length,
                                          dec_data.ptr()))) {
    TPM_LOG(ERROR, result) << "Unseal: Error calling Tspi_Data_Unseal";
    return false;
  }
  value->assign(&dec_data.value()[0], &dec_data.value()[dec_data_length]);
  chromeos::SecureMemset(dec_data.value(), 0, dec_data_length);
  return true;
}

}  // namespace cryptohome
