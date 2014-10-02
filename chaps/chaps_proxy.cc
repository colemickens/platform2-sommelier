// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_proxy.h"

#include <base/logging.h>

#include "chaps/chaps.h"
#include "chaps/chaps_utility.h"
#include "chaps/isolate.h"
#include "pkcs11/cryptoki.h"

using base::AutoLock;
using std::string;
using std::vector;
using chromeos::SecureBlob;

namespace chaps {

ChapsProxyImpl::ChapsProxyImpl() {}

ChapsProxyImpl::~ChapsProxyImpl() {}

bool ChapsProxyImpl::Init() {
  try {
    if (!DBus::default_dispatcher)
      DBus::default_dispatcher = new DBus::BusDispatcher();
    if (!proxy_.get()) {
      // Establish a D-Bus connection.
      DBus::Connection connection = DBus::Connection::SystemBus();
      // Set the timeout to 5 minutes; some TPM operations can take long.  In
      // practice, calls have been noted to take more than 1 minute.
      connection.set_timeout(300000);
      proxy_.reset(new Proxy(connection, kChapsServicePath,
                             kChapsServiceName));
    }
    if (proxy_.get()) {
      if (!WaitForService())
        return false;
      VLOG(1) << "Chaps proxy initialized (" << kChapsServicePath << ").";
      return true;
    }
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return false;
}

bool ChapsProxyImpl::OpenIsolate(SecureBlob* isolate_credential,
                                 bool* new_isolate_created) {
  AutoLock lock(lock_);
  bool result = false;
  if (!proxy_.get()) {
    LOG(ERROR) << __func__ << ": Proxy not initialized.";
    return false;
  }
  try {
    SecureBlob isolate_credential_in;
    SecureBlob isolate_credential_out;
    isolate_credential_in.swap(*isolate_credential);
    proxy_->OpenIsolate(isolate_credential_in, isolate_credential_out,
                        *new_isolate_created, result);
    isolate_credential->swap(isolate_credential_out);
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
    result = false;
  }
  return result;
}

void ChapsProxyImpl::CloseIsolate(const SecureBlob& isolate_credential) {
  AutoLock lock(lock_);
  if (!proxy_.get()) {
    LOG(ERROR) << __func__ << ": Proxy not initialized.";
    return;
  }
  try {
    proxy_->CloseIsolate(isolate_credential);
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
}

bool ChapsProxyImpl::LoadToken(const SecureBlob& isolate_credential,
                               const string& path,
                               const vector<uint8_t>& auth_data,
                               const string& label,
                               uint64_t* slot_id) {
  AutoLock lock(lock_);
  if (!proxy_.get()) {
    LOG(ERROR) << __func__ << ": Proxy not initialized.";
    return false;
  }
  bool result = false;;
  try {
    proxy_->LoadToken(isolate_credential,
                      path,
                      auth_data,
                      label,
                      *slot_id,
                      result);
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
    result = false;
  }
  return result;
}

void ChapsProxyImpl::UnloadToken(const SecureBlob& isolate_credential,
                                 const string& path) {
  AutoLock lock(lock_);
  if (!proxy_.get()) {
    LOG(ERROR) << __func__ << ": Proxy not initialized.";
    return;
  }
  try {
    proxy_->UnloadToken(isolate_credential, path);
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
}

void ChapsProxyImpl::ChangeTokenAuthData(const string& path,
                                         const vector<uint8_t>& old_auth_data,
                                         const vector<uint8_t>& new_auth_data) {
  AutoLock lock(lock_);
  if (!proxy_.get()) {
    LOG(ERROR) << __func__ << ": Proxy not initialized.";
    return;
  }
  try {
    proxy_->ChangeTokenAuthData(path, old_auth_data, new_auth_data);
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
}

bool ChapsProxyImpl::GetTokenPath(const SecureBlob& isolate_credential,
                                  uint64_t slot_id,
                                  string* path) {
  AutoLock lock(lock_);
  if (!proxy_.get()) {
    LOG(ERROR) << __func__ << ": Proxy not initialized.";
    return false;
  }
  bool result = false;;
  try {
    proxy_->GetTokenPath(isolate_credential, slot_id, *path, result);
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
    result = false;
  }
  return result;
}

void ChapsProxyImpl::SetLogLevel(const int32_t& level) {
  AutoLock lock(lock_);
  if (!proxy_.get()) {
    LOG(ERROR) << "Failed to set logging level: proxy not initialized.";
    return;
  }
  try {
    proxy_->SetLogLevel(level);
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
}

uint32_t ChapsProxyImpl::GetSlotList(const SecureBlob& isolate_credential,
                                     bool token_present,
                                     vector<uint64_t>* slot_list) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!slot_list, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GetSlotList(isolate_credential, token_present, *slot_list, result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetSlotInfo(const SecureBlob& isolate_credential,
                                     uint64_t slot_id,
                                     vector<uint8_t>* slot_description,
                                     vector<uint8_t>* manufacturer_id,
                                     uint64_t* flags,
                                     uint8_t* hardware_version_major,
                                     uint8_t* hardware_version_minor,
                                     uint8_t* firmware_version_major,
                                     uint8_t* firmware_version_minor) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!slot_description || !manufacturer_id || !flags ||
      !hardware_version_major || !hardware_version_minor ||
      !firmware_version_major || !firmware_version_minor)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GetSlotInfo(isolate_credential,
                        slot_id,
                        *slot_description,
                        *manufacturer_id,
                        *flags,
                        *hardware_version_major,
                        *hardware_version_minor,
                        *firmware_version_major,
                        *firmware_version_minor,
                        result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetTokenInfo(const SecureBlob& isolate_credential,
                                      uint64_t slot_id,
                                      vector<uint8_t>* label,
                                      vector<uint8_t>* manufacturer_id,
                                      vector<uint8_t>* model,
                                      vector<uint8_t>* serial_number,
                                      uint64_t* flags,
                                      uint64_t* max_session_count,
                                      uint64_t* session_count,
                                      uint64_t* max_session_count_rw,
                                      uint64_t* session_count_rw,
                                      uint64_t* max_pin_len,
                                      uint64_t* min_pin_len,
                                      uint64_t* total_public_memory,
                                      uint64_t* free_public_memory,
                                      uint64_t* total_private_memory,
                                      uint64_t* free_private_memory,
                                      uint8_t* hardware_version_major,
                                      uint8_t* hardware_version_minor,
                                      uint8_t* firmware_version_major,
                                      uint8_t* firmware_version_minor) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!label || !manufacturer_id || !model || !serial_number || !flags ||
      !max_session_count || !session_count || !max_session_count_rw ||
      !session_count_rw || !max_pin_len || !min_pin_len ||
      !total_public_memory || !free_public_memory || !total_private_memory ||
      !total_public_memory ||
      !hardware_version_major || !hardware_version_minor ||
      !firmware_version_major || !firmware_version_minor)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GetTokenInfo(isolate_credential,
                         slot_id,
                         *label,
                         *manufacturer_id,
                         *model,
                         *serial_number,
                         *flags,
                         *max_session_count,
                         *session_count,
                         *max_session_count_rw,
                         *session_count_rw,
                         *max_pin_len,
                         *min_pin_len,
                         *total_public_memory,
                         *free_public_memory,
                         *total_private_memory,
                         *free_private_memory,
                         *hardware_version_major,
                         *hardware_version_minor,
                         *firmware_version_major,
                         *firmware_version_minor,
                         result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetMechanismList(
    const SecureBlob& isolate_credential,
    uint64_t slot_id,
    vector<uint64_t>* mechanism_list) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!mechanism_list, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GetMechanismList(isolate_credential, slot_id, *mechanism_list,
                             result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetMechanismInfo(const SecureBlob& isolate_credential,
                                          uint64_t slot_id,
                                          uint64_t mechanism_type,
                                          uint64_t* min_key_size,
                                          uint64_t* max_key_size,
                                          uint64_t* flags) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!min_key_size || !max_key_size || !flags)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GetMechanismInfo(isolate_credential,
                             slot_id,
                             mechanism_type,
                             *min_key_size,
                             *max_key_size,
                             *flags,
                             result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::InitToken(const SecureBlob& isolate_credential,
                                   uint64_t slot_id,
                                   const string* so_pin,
                                   const vector<uint8_t>& label) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    string tmp_pin;
    if (so_pin)
      tmp_pin = *so_pin;
    result = proxy_->InitToken(isolate_credential, slot_id, (so_pin == NULL),
                               tmp_pin, label);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::InitPIN(const SecureBlob& isolate_credential,
                                 uint64_t session_id, const string* pin) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    string tmp_pin;
    if (pin)
      tmp_pin = *pin;
    result = proxy_->InitPIN(isolate_credential, session_id, (pin == NULL),
                             tmp_pin);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::SetPIN(const SecureBlob& isolate_credential,
                                uint64_t session_id,
                                const string* old_pin,
                                const string* new_pin) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    string tmp_old_pin;
    if (old_pin)
      tmp_old_pin = *old_pin;
    string tmp_new_pin;
    if (new_pin)
      tmp_new_pin = *new_pin;
    result = proxy_->SetPIN(isolate_credential, session_id, (old_pin == NULL),
                            tmp_old_pin, (new_pin == NULL), tmp_new_pin);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::OpenSession(const SecureBlob& isolate_credential,
                                     uint64_t slot_id, uint64_t flags,
                                     uint64_t* session_id) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!session_id, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->OpenSession(isolate_credential, slot_id, flags, *session_id,
                        result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::CloseSession(const SecureBlob& isolate_credential,
                                      uint64_t session_id) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->CloseSession(isolate_credential, session_id);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::CloseAllSessions(const SecureBlob& isolate_credential,
                                          uint64_t slot_id) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->CloseAllSessions(isolate_credential, slot_id);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetSessionInfo(const SecureBlob& isolate_credential,
                                        uint64_t session_id,
                                        uint64_t* slot_id,
                                        uint64_t* state,
                                        uint64_t* flags,
                                        uint64_t* device_error) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!slot_id || !state || !flags || !device_error)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GetSessionInfo(isolate_credential, session_id, *slot_id, *state,
                           *flags, *device_error, result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetOperationState(const SecureBlob& isolate_credential,
                                           uint64_t session_id,
                                           vector<uint8_t>* operation_state) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!operation_state, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GetOperationState(isolate_credential, session_id, *operation_state,
                              result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::SetOperationState(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    const vector<uint8_t>& operation_state,
    uint64_t encryption_key_handle,
    uint64_t authentication_key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->SetOperationState(isolate_credential,
                                       session_id,
                                       operation_state,
                                       encryption_key_handle,
                                       authentication_key_handle);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::Login(const SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t user_type,
                               const string* pin) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    string tmp_pin;
    if (pin)
      tmp_pin = *pin;
    result = proxy_->Login(isolate_credential, session_id, user_type,
                           (pin == NULL), tmp_pin);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::Logout(const SecureBlob& isolate_credential,
                                uint64_t session_id) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->Logout(isolate_credential, session_id);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::CreateObject(const SecureBlob& isolate_credential,
                                      uint64_t session_id,
                                      const vector<uint8_t>& attributes,
                                      uint64_t* new_object_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!new_object_handle, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->CreateObject(isolate_credential,
                         session_id,
                         attributes,
                         *new_object_handle,
                         result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::CopyObject(const SecureBlob& isolate_credential,
                                    uint64_t session_id,
                                    uint64_t object_handle,
                                    const vector<uint8_t>& attributes,
                                    uint64_t* new_object_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!new_object_handle, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->CopyObject(isolate_credential,
                       session_id,
                       object_handle,
                       attributes,
                       *new_object_handle,
                       result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DestroyObject(const SecureBlob& isolate_credential,
                                       uint64_t session_id,
                                       uint64_t object_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->DestroyObject(isolate_credential, session_id,
                                   object_handle);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetObjectSize(const SecureBlob& isolate_credential,
                                       uint64_t session_id,
                                       uint64_t object_handle,
                                       uint64_t* object_size) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!object_size, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GetObjectSize(isolate_credential, session_id, object_handle,
                          *object_size, result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetAttributeValue(const SecureBlob& isolate_credential,
                                           uint64_t session_id,
                                           uint64_t object_handle,
                                           const vector<uint8_t>& attributes_in,
                                           vector<uint8_t>* attributes_out) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!attributes_out, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GetAttributeValue(isolate_credential,
                              session_id,
                              object_handle,
                              attributes_in,
                              *attributes_out,
                              result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::SetAttributeValue(const SecureBlob& isolate_credential,
                                           uint64_t session_id,
                                           uint64_t object_handle,
                                           const vector<uint8_t>& attributes) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->SetAttributeValue(isolate_credential,
                                       session_id,
                                       object_handle,
                                       attributes);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::FindObjectsInit(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    const vector<uint8_t>& attributes) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->FindObjectsInit(isolate_credential, session_id,
                                     attributes);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::FindObjects(const SecureBlob& isolate_credential,
                                     uint64_t session_id,
                                     uint64_t max_object_count,
                                     vector<uint64_t>* object_list) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!object_list || object_list->size() > 0)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->FindObjects(isolate_credential, session_id, max_object_count,
                        *object_list, result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::FindObjectsFinal(const SecureBlob& isolate_credential,
                                          uint64_t session_id) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->FindObjectsFinal(isolate_credential, session_id);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::EncryptInit(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->EncryptInit(isolate_credential,
                                 session_id,
                                 mechanism_type,
                                 mechanism_parameter,
                                 key_handle);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::Encrypt(const SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 const vector<uint8_t>& data_in,
                                 uint64_t max_out_length,
                                 uint64_t* actual_out_length,
                                 vector<uint8_t>* data_out) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->Encrypt(isolate_credential,
                    session_id,
                    data_in,
                    max_out_length,
                    *actual_out_length,
                    *data_out,
                    result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::EncryptUpdate(const SecureBlob& isolate_credential,
                                       uint64_t session_id,
                                       const vector<uint8_t>& data_in,
                                       uint64_t max_out_length,
                                       uint64_t* actual_out_length,
                                       vector<uint8_t>* data_out) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->EncryptUpdate(isolate_credential,
                          session_id,
                          data_in,
                          max_out_length,
                          *actual_out_length,
                          *data_out,
                          result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::EncryptFinal(const SecureBlob& isolate_credential,
                                      uint64_t session_id,
                                      uint64_t max_out_length,
                                      uint64_t* actual_out_length,
                                      vector<uint8_t>* data_out) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->EncryptFinal(isolate_credential,
                         session_id,
                         max_out_length,
                         *actual_out_length,
                         *data_out,
                         result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DecryptInit(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->DecryptInit(isolate_credential,
                                 session_id,
                                 mechanism_type,
                                 mechanism_parameter,
                                 key_handle);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::Decrypt(const SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 const vector<uint8_t>& data_in,
                                 uint64_t max_out_length,
                                 uint64_t* actual_out_length,
                                 vector<uint8_t>* data_out) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->Decrypt(isolate_credential,
                    session_id,
                    data_in,
                    max_out_length,
                    *actual_out_length,
                    *data_out,
                    result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DecryptUpdate(const SecureBlob& isolate_credential,
                                       uint64_t session_id,
                                       const vector<uint8_t>& data_in,
                                       uint64_t max_out_length,
                                       uint64_t* actual_out_length,
                                       vector<uint8_t>* data_out) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->DecryptUpdate(isolate_credential,
                          session_id,
                          data_in,
                          max_out_length,
                          *actual_out_length,
                          *data_out,
                          result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DecryptFinal(const SecureBlob& isolate_credential,
                                      uint64_t session_id,
                                      uint64_t max_out_length,
                                      uint64_t* actual_out_length,
                                      vector<uint8_t>* data_out) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->DecryptFinal(isolate_credential,
                         session_id,
                         max_out_length,
                         *actual_out_length,
                         *data_out,
                         result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DigestInit(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->DigestInit(isolate_credential,
                                session_id,
                                mechanism_type,
                                mechanism_parameter);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::Digest(const SecureBlob& isolate_credential,
                                uint64_t session_id,
                                const vector<uint8_t>& data_in,
                                uint64_t max_out_length,
                                uint64_t* actual_out_length,
                                vector<uint8_t>* digest) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->Digest(isolate_credential,
                   session_id,
                   data_in,
                   max_out_length,
                   *actual_out_length,
                   *digest,
                   result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DigestUpdate(const SecureBlob& isolate_credential,
                                      uint64_t session_id,
                                      const vector<uint8_t>& data_in) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->DigestUpdate(isolate_credential, session_id, data_in);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DigestKey(const SecureBlob& isolate_credential,
                                   uint64_t session_id,
                                   uint64_t key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->DigestKey(isolate_credential, session_id, key_handle);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DigestFinal(const SecureBlob& isolate_credential,
                                     uint64_t session_id,
                                     uint64_t max_out_length,
                                     uint64_t* actual_out_length,
                                     vector<uint8_t>* digest) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->DigestFinal(isolate_credential,
                        session_id,
                        max_out_length,
                        *actual_out_length,
                        *digest,
                        result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::SignInit(const SecureBlob& isolate_credential,
                                  uint64_t session_id,
                                  uint64_t mechanism_type,
                                  const vector<uint8_t>& mechanism_parameter,
                                  uint64_t key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->SignInit(isolate_credential,
                              session_id,
                              mechanism_type,
                              mechanism_parameter,
                              key_handle);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::Sign(const SecureBlob& isolate_credential,
                              uint64_t session_id,
                              const vector<uint8_t>& data,
                              uint64_t max_out_length,
                              uint64_t* actual_out_length,
                              vector<uint8_t>* signature) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->Sign(isolate_credential,
                 session_id,
                 data,
                 max_out_length,
                 *actual_out_length,
                 *signature,
                 result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::SignUpdate(const SecureBlob& isolate_credential,
                                    uint64_t session_id,
                                    const vector<uint8_t>& data_part) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->SignUpdate(isolate_credential, session_id, data_part);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::SignFinal(const SecureBlob& isolate_credential,
                                   uint64_t session_id,
                                   uint64_t max_out_length,
                                   uint64_t* actual_out_length,
                                   vector<uint8_t>* signature) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->SignFinal(isolate_credential,
                      session_id,
                      max_out_length,
                      *actual_out_length,
                      *signature,
                      result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::SignRecoverInit(
      const SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t mechanism_type,
      const vector<uint8_t>& mechanism_parameter,
      uint64_t key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->SignRecoverInit(isolate_credential,
                                     session_id,
                                     mechanism_type,
                                     mechanism_parameter,
                                     key_handle);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::SignRecover(const SecureBlob& isolate_credential,
                                     uint64_t session_id,
                                     const vector<uint8_t>& data,
                                     uint64_t max_out_length,
                                     uint64_t* actual_out_length,
                                     vector<uint8_t>* signature) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->SignRecover(isolate_credential,
                        session_id,
                        data,
                        max_out_length,
                        *actual_out_length,
                        *signature,
                        result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::VerifyInit(const SecureBlob& isolate_credential,
                                    uint64_t session_id,
                                    uint64_t mechanism_type,
                                    const vector<uint8_t>& mechanism_parameter,
                                    uint64_t key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->VerifyInit(isolate_credential,
                                session_id,
                                mechanism_type,
                                mechanism_parameter,
                                key_handle);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::Verify(const SecureBlob& isolate_credential,
                                uint64_t session_id,
                                const vector<uint8_t>& data,
                                const vector<uint8_t>& signature) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->Verify(isolate_credential, session_id, data, signature);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::VerifyUpdate(const SecureBlob& isolate_credential,
                                      uint64_t session_id,
                                      const vector<uint8_t>& data_part) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->VerifyUpdate(isolate_credential, session_id, data_part);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::VerifyFinal(const SecureBlob& isolate_credential,
                                     uint64_t session_id,
                                     const vector<uint8_t>& signature) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->VerifyFinal(isolate_credential, session_id, signature);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::VerifyRecoverInit(
      const SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t mechanism_type,
      const vector<uint8_t>& mechanism_parameter,
      uint64_t key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->VerifyRecoverInit(isolate_credential,
                                       session_id,
                                       mechanism_type,
                                       mechanism_parameter,
                                       key_handle);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::VerifyRecover(const SecureBlob& isolate_credential,
                                       uint64_t session_id,
                                       const vector<uint8_t>& signature,
                                       uint64_t max_out_length,
                                       uint64_t* actual_out_length,
                                       vector<uint8_t>* data) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->VerifyRecover(isolate_credential,
                          session_id,
                          signature,
                          max_out_length,
                          *actual_out_length,
                          *data,
                          result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DigestEncryptUpdate(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    const vector<uint8_t>& data_in,
    uint64_t max_out_length,
    uint64_t* actual_out_length,
    vector<uint8_t>* data_out) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->DigestEncryptUpdate(isolate_credential,
                                session_id,
                                data_in,
                                max_out_length,
                                *actual_out_length,
                                *data_out,
                                result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DecryptDigestUpdate(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    const vector<uint8_t>& data_in,
    uint64_t max_out_length,
    uint64_t* actual_out_length,
    vector<uint8_t>* data_out) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->DecryptDigestUpdate(isolate_credential,
                                session_id,
                                data_in,
                                max_out_length,
                                *actual_out_length,
                                *data_out,
                                result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::SignEncryptUpdate(const SecureBlob& isolate_credential,
                                           uint64_t session_id,
                                           const vector<uint8_t>& data_in,
                                           uint64_t max_out_length,
                                           uint64_t* actual_out_length,
                                           vector<uint8_t>* data_out) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->SignEncryptUpdate(isolate_credential,
                              session_id,
                              data_in,
                              max_out_length,
                              *actual_out_length,
                              *data_out,
                              result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DecryptVerifyUpdate(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    const vector<uint8_t>& data_in,
    uint64_t max_out_length,
    uint64_t* actual_out_length,
    vector<uint8_t>* data_out) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->DecryptVerifyUpdate(isolate_credential,
                                session_id,
                                data_in,
                                max_out_length,
                                *actual_out_length,
                                *data_out,
                                result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GenerateKey(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const vector<uint8_t>& attributes,
    uint64_t* key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!key_handle, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GenerateKey(isolate_credential,
                        session_id,
                        mechanism_type,
                        mechanism_parameter,
                        attributes,
                        *key_handle,
                        result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GenerateKeyPair(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const vector<uint8_t>& public_attributes,
    const vector<uint8_t>& private_attributes,
    uint64_t* public_key_handle,
    uint64_t* private_key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!public_key_handle || !private_key_handle,
                          CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GenerateKeyPair(isolate_credential,
                            session_id,
                            mechanism_type,
                            mechanism_parameter,
                            public_attributes,
                            private_attributes,
                            *public_key_handle,
                            *private_key_handle,
                            result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::WrapKey(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t wrapping_key_handle,
    uint64_t key_handle,
    uint64_t max_out_length,
    uint64_t* actual_out_length,
    vector<uint8_t>* wrapped_key) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !wrapped_key,
                          CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->WrapKey(isolate_credential,
                    session_id,
                    mechanism_type,
                    mechanism_parameter,
                    wrapping_key_handle,
                    key_handle,
                    max_out_length,
                    *actual_out_length,
                    *wrapped_key,
                    result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::UnwrapKey(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t wrapping_key_handle,
    const vector<uint8_t>& wrapped_key,
    const vector<uint8_t>& attributes,
    uint64_t* key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!key_handle, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->UnwrapKey(isolate_credential,
                      session_id,
                      mechanism_type,
                      mechanism_parameter,
                      wrapping_key_handle,
                      wrapped_key,
                      attributes,
                      *key_handle,
                      result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::DeriveKey(
    const SecureBlob& isolate_credential,
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t base_key_handle,
    const vector<uint8_t>& attributes,
    uint64_t* key_handle) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!key_handle, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->DeriveKey(isolate_credential,
                      session_id,
                      mechanism_type,
                      mechanism_parameter,
                      base_key_handle,
                      attributes,
                      *key_handle,
                      result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::SeedRandom(const SecureBlob& isolate_credential,
                                    uint64_t session_id,
                                    const vector<uint8_t>& seed) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(seed.size() == 0, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    result = proxy_->SeedRandom(isolate_credential, session_id, seed);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GenerateRandom(const SecureBlob& isolate_credential,
                                        uint64_t session_id,
                                        uint64_t num_bytes,
                                        vector<uint8_t>* random_data) {
  AutoLock lock(lock_);
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!random_data || num_bytes == 0, CKR_ARGUMENTS_BAD);
  uint32_t result = CKR_GENERAL_ERROR;
  try {
    proxy_->GenerateRandom(isolate_credential, session_id, num_bytes,
                           *random_data, result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

bool ChapsProxyImpl::WaitForService() {
  const useconds_t kDelayOnFailureUs = 10000;  // 10ms.
  const int kMaxAttempts = 500;  // 5 seconds.
  vector<uint64_t> slot_list;
  uint32_t result;
  string last_error_message;
  for (int i = 0; i < kMaxAttempts; ++i) {
    try {
      proxy_->GetSlotList(
          IsolateCredentialManager::GetDefaultIsolateCredential(),
          false, slot_list, result);
      return true;
    } catch (DBus::Error err) {
      last_error_message = err.what();
    }
    usleep(kDelayOnFailureUs);
  }
  LOG(ERROR) << "Chaps service is not available: " << last_error_message;
  return false;
}

}  // namespace chaps
