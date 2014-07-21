// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_adaptor.h"

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/synchronization/lock.h>

#include "chaps/chaps.h"
#include "chaps/chaps_interface.h"
#include "chaps/chaps_utility.h"
#include "chaps/token_manager_interface.h"

using base::AutoLock;
using base::FilePath;
using base::Lock;
using chromeos::SecureBlob;
using std::string;
using std::vector;

namespace chaps {

// Helper used when calling the ObjectAdaptor constructor.
static DBus::Connection& GetConnection() {
  static DBus::Connection connection = DBus::Connection::SystemBus();
  CHECK(connection.acquire_name(kChapsServiceName));
  return connection;
}

ChapsAdaptor::ChapsAdaptor(Lock* lock,
                           ChapsInterface* service,
                           TokenManagerInterface* token_manager)
    : DBus::ObjectAdaptor(GetConnection(),
                          DBus::Path(kChapsServicePath)),
      lock_(lock),
      service_(service),
      token_manager_(token_manager) {}

ChapsAdaptor::~ChapsAdaptor() {}

void ChapsAdaptor::OpenIsolate(
      const std::vector<uint8_t>& isolate_credential_in,
      std::vector<uint8_t>& isolate_credential_out,  // NOLINT - refs
      bool& new_isolate_created,  // NOLINT(runtime/references)
      bool& result) {  // NOLINT(runtime/references)
  VLOG(1) << "CALL: " << __func__;
  result = false;
  SecureBlob isolate_credential(&isolate_credential_in.front(),
                                isolate_credential_in.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential_in));
  if (token_manager_)
    result = token_manager_->OpenIsolate(&isolate_credential,
                                         &new_isolate_created);
  isolate_credential_out.swap(isolate_credential);
}

void ChapsAdaptor::OpenIsolate(
      const std::vector<uint8_t>& isolate_credential_in,
      std::vector<uint8_t>& isolate_credential_out,  // NOLINT - refs
      bool& new_isolate_created,  // NOLINT(runtime/references)
      bool& result,  // NOLINT(runtime/references)
      ::DBus::Error& /*error*/) {
  OpenIsolate(isolate_credential_in, isolate_credential_out,
              new_isolate_created, result);
}

void ChapsAdaptor::CloseIsolate(const vector<uint8_t>& isolate_credential) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  if (token_manager_)
    token_manager_->CloseIsolate(isolate_credential_blob);
}

void ChapsAdaptor::CloseIsolate(const vector<uint8_t>& isolate_credential,
                                ::DBus::Error& /*error*/) {
  CloseIsolate(isolate_credential);
}

void ChapsAdaptor::LoadToken(const vector<uint8_t>& isolate_credential,
                             const string& path,
                             const vector<uint8_t>& auth_data,
                             const string& label,
                             uint64_t& slot_id,  // NOLINT(runtime/references)
                             bool& result) {  // NOLINT(runtime/references)
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  SecureBlob auth_data_blob(&auth_data.front(), auth_data.size());
  if (token_manager_)
    result = token_manager_->LoadToken(isolate_credential_blob,
                                       FilePath(path),
                                       auth_data_blob,
                                       label,
                                       PreservedValue<uint64_t, int>(&slot_id));
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  ClearVector(const_cast<vector<uint8_t>*>(&auth_data));
}

void ChapsAdaptor::LoadToken(const vector<uint8_t>& isolate_credential,
                             const string& path,
                             const vector<uint8_t>& auth_data,
                             const string& label,
                             uint64_t& slot_id,  // NOLINT(runtime/references)
                             bool& result,  // NOLINT(runtime/references)
                             ::DBus::Error& /*error*/) {
  LoadToken(isolate_credential, path, auth_data, label, slot_id, result);
}

void ChapsAdaptor::UnloadToken(const vector<uint8_t>& isolate_credential,
                               const string& path) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  if (token_manager_)
    token_manager_->UnloadToken(isolate_credential_blob, FilePath(path));
}

void ChapsAdaptor::UnloadToken(const vector<uint8_t>& isolate_credential,
                               const string& path,
                               ::DBus::Error& /*error*/) {
  UnloadToken(isolate_credential, path);
}

void ChapsAdaptor::ChangeTokenAuthData(const string& path,
                                       const vector<uint8_t>& old_auth_data,
                                       const vector<uint8_t>& new_auth_data) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  if (token_manager_)
    token_manager_->ChangeTokenAuthData(
        FilePath(path),
        SecureBlob(&old_auth_data.front(), old_auth_data.size()),
        SecureBlob(&new_auth_data.front(), new_auth_data.size()));
  ClearVector(const_cast<vector<uint8_t>*>(&old_auth_data));
  ClearVector(const_cast<vector<uint8_t>*>(&new_auth_data));
}

void ChapsAdaptor::ChangeTokenAuthData(const string& path,
                                       const vector<uint8_t>& old_auth_data,
                                       const vector<uint8_t>& new_auth_data,
                                       ::DBus::Error& /*error*/) {
  ChangeTokenAuthData(path, old_auth_data, new_auth_data);
}

void ChapsAdaptor::GetTokenPath(const std::vector<uint8_t>& isolate_credential,
                                const uint64_t& slot_id,
                                std::string& path,  // NOLINT - refs
                                bool& result) {  // NOLINT(runtime/references)
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  if (token_manager_) {
    FilePath tmp;
    result = token_manager_->GetTokenPath(isolate_credential_blob,
                                          slot_id,
                                          &tmp);
    path = tmp.value();
  }
}

void ChapsAdaptor::GetTokenPath(const std::vector<uint8_t>& isolate_credential,
                                const uint64_t& slot_id,
                                std::string& path,  // NOLINT - refs
                                bool& result,  // NOLINT(runtime/references)
                                ::DBus::Error& /*error*/) {
  GetTokenPath(isolate_credential, slot_id, path, result);
}

void ChapsAdaptor::SetLogLevel(const int32_t& level) {
  logging::SetMinLogLevel(level);
}

void ChapsAdaptor::SetLogLevel(const int32_t& level,
                               ::DBus::Error& /*error*/) {
  SetLogLevel(level);
}

void ChapsAdaptor::GetSlotList(const vector<uint8_t>& isolate_credential,
                               const bool& token_present,
                               vector<uint64_t>& slot_list,  // NOLINT - refs
                               uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "token_present=" << token_present;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GetSlotList(isolate_credential_blob, token_present,
                                 &slot_list);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "slot_list="
                               << PrintIntVector(slot_list);
}

void ChapsAdaptor::GetSlotList(const vector<uint8_t>& isolate_credential,
                               const bool& token_present,
                               vector<uint64_t>& slot_list,  // NOLINT - rfs
                               uint32_t& result,  // NOLINT(runtime/references)
                               ::DBus::Error& /*error*/) {
  GetSlotList(isolate_credential, token_present, slot_list, result);
}

void ChapsAdaptor::GetSlotInfo(const vector<uint8_t>& isolate_credential,
                               const uint64_t& slot_id,
                               vector<uint8_t>& slot_description,  // NOLINT - refs
                               vector<uint8_t>& manufacturer_id,  // NOLINT - refs
                               uint64_t& flags,  // NOLINT - refs
                               uint8_t& hardware_version_major,  // NOLINT - refs
                               uint8_t& hardware_version_minor,  // NOLINT - refs
                               uint8_t& firmware_version_major,  // NOLINT - refs
                               uint8_t& firmware_version_minor,  // NOLINT - refs
                               uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GetSlotInfo(isolate_credential_blob,
                                 slot_id,
                                 &slot_description,
                                 &manufacturer_id,
                                 &flags,
                                 &hardware_version_major,
                                 &hardware_version_minor,
                                 &firmware_version_major,
                                 &firmware_version_minor);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "slot_description="
                               << ConvertByteVectorToString(slot_description);
}

void ChapsAdaptor::GetSlotInfo(const vector<uint8_t>& isolate_credential,
                               const uint64_t& slot_id,
                               vector<uint8_t>& slot_description,  // NOLINT - refs
                               vector<uint8_t>& manufacturer_id,  // NOLINT - refs
                               uint64_t& flags,  // NOLINT - refs
                               uint8_t& hardware_version_major,  // NOLINT - refs
                               uint8_t& hardware_version_minor,  // NOLINT - refs
                               uint8_t& firmware_version_major,  // NOLINT - refs
                               uint8_t& firmware_version_minor,  // NOLINT - refs
                               uint32_t& result,  // NOLINT - refs
                               ::DBus::Error& /*error*/) {
  GetSlotInfo(isolate_credential, slot_id, slot_description, manufacturer_id,
              flags, hardware_version_major, hardware_version_minor,
              firmware_version_major, firmware_version_minor,
              result);
}

void ChapsAdaptor::GetTokenInfo(const vector<uint8_t>& isolate_credential,
                                const uint64_t& slot_id,
                                vector<uint8_t>& label,  // NOLINT - refs
                                vector<uint8_t>& manufacturer_id,  // NOLINT - refs
                                vector<uint8_t>& model,  // NOLINT - refs
                                vector<uint8_t>& serial_number,  // NOLINT - refs
                                uint64_t& flags,  // NOLINT - refs
                                uint64_t& max_session_count,  // NOLINT - refs
                                uint64_t& session_count,  // NOLINT - refs
                                uint64_t& max_session_count_rw,  // NOLINT - refs
                                uint64_t& session_count_rw,  // NOLINT - refs
                                uint64_t& max_pin_len,  // NOLINT - refs
                                uint64_t& min_pin_len,  // NOLINT - refs
                                uint64_t& total_public_memory,  // NOLINT - refs
                                uint64_t& free_public_memory,  // NOLINT - refs
                                uint64_t& total_private_memory,  // NOLINT - refs
                                uint64_t& free_private_memory,  // NOLINT - refs
                                uint8_t& hardware_version_major,  // NOLINT - refs
                                uint8_t& hardware_version_minor,  // NOLINT - refs
                                uint8_t& firmware_version_major,  // NOLINT - refs
                                uint8_t& firmware_version_minor,  // NOLINT - refs
                                uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GetTokenInfo(isolate_credential_blob,
                                  slot_id,
                                  &label,
                                  &manufacturer_id,
                                  &model,
                                  &serial_number,
                                  &flags,
                                  &max_session_count,
                                  &session_count,
                                  &max_session_count_rw,
                                  &session_count_rw,
                                  &max_pin_len,
                                  &min_pin_len,
                                  &total_public_memory,
                                  &free_public_memory,
                                  &total_private_memory,
                                  &free_private_memory,
                                  &hardware_version_major,
                                  &hardware_version_minor,
                                  &firmware_version_major,
                                  &firmware_version_minor);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "label="
                               << ConvertByteVectorToString(label);
}

void ChapsAdaptor::GetTokenInfo(const vector<uint8_t>& isolate_credential,
                                const uint64_t& slot_id,
                                vector<uint8_t>& label,  // NOLINT - refs
                                vector<uint8_t>& manufacturer_id,  // NOLINT - refs
                                vector<uint8_t>& model,  // NOLINT - refs
                                vector<uint8_t>& serial_number,  // NOLINT - refs
                                uint64_t& flags,  // NOLINT - refs
                                uint64_t& max_session_count,  // NOLINT - refs
                                uint64_t& session_count,  // NOLINT - refs
                                uint64_t& max_session_count_rw,  // NOLINT - refs
                                uint64_t& session_count_rw,  // NOLINT - refs
                                uint64_t& max_pin_len,  // NOLINT - refs
                                uint64_t& min_pin_len,  // NOLINT - refs
                                uint64_t& total_public_memory,  // NOLINT - refs
                                uint64_t& free_public_memory,  // NOLINT - refs
                                uint64_t& total_private_memory,  // NOLINT - refs
                                uint64_t& free_private_memory,  // NOLINT - refs
                                uint8_t& hardware_version_major,  // NOLINT - refs
                                uint8_t& hardware_version_minor,  // NOLINT - refs
                                uint8_t& firmware_version_major,  // NOLINT - refs
                                uint8_t& firmware_version_minor,  // NOLINT - refs
                                uint32_t& result,  // NOLINT - refs
                                ::DBus::Error& /*error*/) {
  GetTokenInfo(isolate_credential, slot_id, label, manufacturer_id, model,
               serial_number, flags, max_session_count, session_count,
               max_session_count_rw, session_count_rw, max_pin_len, min_pin_len,
               total_public_memory, free_public_memory, total_private_memory,
               free_private_memory, hardware_version_major,
               hardware_version_minor, firmware_version_major,
               firmware_version_minor, result);
}

void ChapsAdaptor::GetMechanismList(const vector<uint8_t>& isolate_credential,
                                    const uint64_t& slot_id,
                                    vector<uint64_t>& mechanism_list,  // NOLINT - refs
                                    uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GetMechanismList(isolate_credential_blob,
                                      slot_id,
                                      &mechanism_list);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "mechanism_list="
                               << PrintIntVector(mechanism_list);
}

void ChapsAdaptor::GetMechanismList(const vector<uint8_t>& isolate_credential,
                                    const uint64_t& slot_id,
                                    vector<uint64_t>& mechanism_list,  // NOLINT - refs
                                    uint32_t& result,  // NOLINT - refs
                                    ::DBus::Error& /*error*/) {
  GetMechanismList(isolate_credential, slot_id, mechanism_list, result);
}

void ChapsAdaptor::GetMechanismInfo(const vector<uint8_t>& isolate_credential,
                                    const uint64_t& slot_id,
                                    const uint64_t& mechanism_type,
                                    uint64_t& min_key_size,  // NOLINT - refs
                                    uint64_t& max_key_size,  // NOLINT - refs
                                    uint64_t& flags,  // NOLINT - refs
                                    uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GetMechanismInfo(isolate_credential_blob,
                                      slot_id,
                                      mechanism_type,
                                      &min_key_size,
                                      &max_key_size,
                                      &flags);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "min_key_size=" << min_key_size;
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "max_key_size=" << max_key_size;
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "flags=" << flags;
}

void ChapsAdaptor::GetMechanismInfo(const vector<uint8_t>& isolate_credential,
                                    const uint64_t& slot_id,
                                    const uint64_t& mechanism_type,
                                    uint64_t& min_key_size,  // NOLINT - refs
                                    uint64_t& max_key_size,  // NOLINT - refs
                                    uint64_t& flags,  // NOLINT - refs
                                    uint32_t& result,  // NOLINT - refs
                                    ::DBus::Error& /*error*/) {
  GetMechanismInfo(isolate_credential, slot_id, mechanism_type, min_key_size,
                   max_key_size, flags, result);
}

uint32_t ChapsAdaptor::InitToken(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& slot_id,
                                 const bool& use_null_pin,
                                 const string& optional_so_pin,
                                 const vector<uint8_t>& new_token_label) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  VLOG(2) << "IN: " << "new_token_label="
                    << ConvertByteVectorToString(new_token_label);
  const string* tmp_pin = use_null_pin ? NULL : &optional_so_pin;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->InitToken(isolate_credential_blob,
                             slot_id,
                             tmp_pin,
                             new_token_label);
}

uint32_t ChapsAdaptor::InitToken(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& slot_id,
                                 const bool& use_null_pin,
                                 const string& optional_so_pin,
                                 const vector<uint8_t>& new_token_label,
                                 ::DBus::Error& /*error*/) {
  return InitToken(isolate_credential, slot_id, use_null_pin, optional_so_pin,
                   new_token_label);
}

uint32_t ChapsAdaptor::InitPIN(const vector<uint8_t>& isolate_credential,
                               const uint64_t& session_id,
                               const bool& use_null_pin,
                               const string& optional_user_pin) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "use_null_pin=" << use_null_pin;
  const string* tmp_pin = use_null_pin ? NULL : &optional_user_pin;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->InitPIN(isolate_credential_blob,
                           session_id,
                           tmp_pin);
}

uint32_t ChapsAdaptor::InitPIN(const vector<uint8_t>& isolate_credential,
                               const uint64_t& session_id,
                               const bool& use_null_pin,
                               const string& optional_user_pin,
                               ::DBus::Error& /*error*/) {
  return InitPIN(isolate_credential, session_id, use_null_pin,
                 optional_user_pin);
}

uint32_t ChapsAdaptor::SetPIN(const vector<uint8_t>& isolate_credential,
                              const uint64_t& session_id,
                              const bool& use_null_old_pin,
                              const string& optional_old_pin,
                              const bool& use_null_new_pin,
                              const string& optional_new_pin) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "use_null_old_pin=" << use_null_old_pin;
  VLOG(2) << "IN: " << "use_null_new_pin=" << use_null_new_pin;
  const string* tmp_old_pin = use_null_old_pin ? NULL : &optional_old_pin;
  const string* tmp_new_pin = use_null_new_pin ? NULL : &optional_new_pin;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->SetPIN(isolate_credential_blob,
                          session_id,
                          tmp_old_pin,
                          tmp_new_pin);
}

uint32_t ChapsAdaptor::SetPIN(const vector<uint8_t>& isolate_credential,
                              const uint64_t& session_id,
                              const bool& use_null_old_pin,
                              const string& optional_old_pin,
                              const bool& use_null_new_pin,
                              const string& optional_new_pin,
                              ::DBus::Error& /*error*/) {
  return SetPIN(isolate_credential, session_id, use_null_old_pin,
                optional_old_pin, use_null_new_pin,  optional_new_pin);
}

void ChapsAdaptor::OpenSession(const vector<uint8_t>& isolate_credential,
                               const uint64_t& slot_id,
                               const uint64_t& flags,
                               uint64_t& session_id,  // NOLINT - refs
                               uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  VLOG(2) << "IN: " << "flags=" << flags;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->OpenSession(isolate_credential_blob,
                                 slot_id,
                                 flags,
                                 &session_id);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "session_id=" << session_id;
}

void ChapsAdaptor::OpenSession(const vector<uint8_t>& isolate_credential,
                               const uint64_t& slot_id,
                               const uint64_t& flags,
                               uint64_t& session_id,  // NOLINT - refs
                               uint32_t& result,  // NOLINT - refs
                               ::DBus::Error& /*error*/) {
  OpenSession(isolate_credential, slot_id, flags, session_id, result);
}

uint32_t ChapsAdaptor::CloseSession(const vector<uint8_t>& isolate_credential,
                                    const uint64_t& session_id) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->CloseSession(isolate_credential_blob,
                                session_id);
}

uint32_t ChapsAdaptor::CloseSession(const vector<uint8_t>& isolate_credential,
                                    const uint64_t& session_id,
                                    ::DBus::Error& /*error*/) {
  return CloseSession(isolate_credential, session_id);
}

uint32_t ChapsAdaptor::CloseAllSessions(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& slot_id) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->CloseAllSessions(
      isolate_credential_blob,
      slot_id);
}

uint32_t ChapsAdaptor::CloseAllSessions(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& slot_id,
    ::DBus::Error& /*error*/) {
  return CloseAllSessions(isolate_credential, slot_id);
}

void ChapsAdaptor::GetSessionInfo(const vector<uint8_t>& isolate_credential,
                                  const uint64_t& session_id,
                                  uint64_t& slot_id,  // NOLINT - refs
                                  uint64_t& state,  // NOLINT - refs
                                  uint64_t& flags,  // NOLINT - refs
                                  uint64_t& device_error,  // NOLINT - refs
                                  uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GetSessionInfo(isolate_credential_blob,
                                    session_id,
                                    &slot_id,
                                    &state,
                                    &flags,
                                    &device_error);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "slot_id=" << slot_id;
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "state=" << state;
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "flags=" << flags;
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "device_error=" << device_error;
}

void ChapsAdaptor::GetSessionInfo(const vector<uint8_t>& isolate_credential,
                                  const uint64_t& session_id,
                                  uint64_t& slot_id,  // NOLINT - refs
                                  uint64_t& state,  // NOLINT - refs
                                  uint64_t& flags,  // NOLINT - refs
                                  uint64_t& device_error,  // NOLINT - refs
                                  uint32_t& result,  // NOLINT - refs
                                  ::DBus::Error& /*error*/) {
  GetSessionInfo(isolate_credential, session_id, slot_id, state, flags,
                 device_error, result);
}

void ChapsAdaptor::GetOperationState(const vector<uint8_t>& isolate_credential,
                                     const uint64_t& session_id,
                                     vector<uint8_t>& operation_state,  // NOLINT - refs
                                     uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GetOperationState(isolate_credential_blob,
                                       session_id,
                                       &operation_state);
}

void ChapsAdaptor::GetOperationState(const vector<uint8_t>& isolate_credential,
                                     const uint64_t& session_id,
                                     vector<uint8_t>& operation_state,  // NOLINT - refs
                                     uint32_t& result,  // NOLINT - refs
                                     ::DBus::Error& /*error*/) {
  GetOperationState(isolate_credential, session_id, operation_state, result);
}

uint32_t ChapsAdaptor::SetOperationState(
      const vector<uint8_t>& isolate_credential,
      const uint64_t& session_id,
      const vector<uint8_t>& operation_state,
      const uint64_t& encryption_key_handle,
      const uint64_t& authentication_key_handle) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->SetOperationState(isolate_credential_blob,
                                     session_id,
                                     operation_state,
                                     encryption_key_handle,
                                     authentication_key_handle);
}

uint32_t ChapsAdaptor::SetOperationState(
      const vector<uint8_t>& isolate_credential,
      const uint64_t& session_id,
      const vector<uint8_t>& operation_state,
      const uint64_t& encryption_key_handle,
      const uint64_t& authentication_key_handle,
      ::DBus::Error& /*error*/) {
  return SetOperationState(isolate_credential, session_id, operation_state,
                           encryption_key_handle, authentication_key_handle);
}

uint32_t ChapsAdaptor::Login(const vector<uint8_t>& isolate_credential,
                             const uint64_t& session_id,
                             const uint64_t& user_type,
                             const bool& use_null_pin,
                             const string& optional_pin) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "user_type=" << user_type;
  VLOG(2) << "IN: " << "use_null_pin=" << use_null_pin;
  const string* pin = use_null_pin ? NULL : &optional_pin;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->Login(isolate_credential_blob,
                         session_id,
                         user_type,
                         pin);
}

uint32_t ChapsAdaptor::Login(const vector<uint8_t>& isolate_credential,
                             const uint64_t& session_id,
                             const uint64_t& user_type,
                             const bool& use_null_pin,
                             const string& optional_pin,
                             ::DBus::Error& /*error*/) {
  return Login(isolate_credential, session_id, user_type, use_null_pin,
               optional_pin);
}

uint32_t ChapsAdaptor::Logout(const vector<uint8_t>& isolate_credential,
                              const uint64_t& session_id) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->Logout(isolate_credential_blob,
                          session_id);
}

uint32_t ChapsAdaptor::Logout(const vector<uint8_t>& isolate_credential,
                              const uint64_t& session_id,
                              ::DBus::Error& /*error*/) {
  return Logout(isolate_credential, session_id);
}

void ChapsAdaptor::CreateObject(
      const vector<uint8_t>& isolate_credential,
      const uint64_t& session_id,
      const vector<uint8_t>& attributes,
      uint64_t& new_object_handle,  // NOLINT(runtime/references)
      uint32_t& result) {  // NOLINT(runtime/references)
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->CreateObject(isolate_credential_blob,
                                  session_id,
                                  attributes,
                                  &new_object_handle);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "new_object_handle="
                               << new_object_handle;
}

void ChapsAdaptor::CreateObject(
      const vector<uint8_t>& isolate_credential,
      const uint64_t& session_id,
      const vector<uint8_t>& attributes,
      uint64_t& new_object_handle,  // NOLINT(runtime/references)
      uint32_t& result,  // NOLINT(runtime/references)
      ::DBus::Error& /*error*/) {
  CreateObject(isolate_credential, session_id, attributes, new_object_handle,
               result);
}

void ChapsAdaptor::CopyObject(
      const vector<uint8_t>& isolate_credential,
      const uint64_t& session_id,
      const uint64_t& object_handle,
      const vector<uint8_t>& attributes,
      uint64_t& new_object_handle,  // NOLINT(runtime/references)
      uint32_t& result) {  // NOLINT(runtime/references)
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "object_handle=" << object_handle;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->CopyObject(isolate_credential_blob,
                                session_id,
                                object_handle,
                                attributes,
                                &new_object_handle);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "new_object_handle="
                               << new_object_handle;
}

void ChapsAdaptor::CopyObject(
      const vector<uint8_t>& isolate_credential,
      const uint64_t& session_id,
      const uint64_t& object_handle,
      const vector<uint8_t>& attributes,
      uint64_t& new_object_handle,  // NOLINT(runtime/references)
      uint32_t& result,  // NOLINT(runtime/references)
      ::DBus::Error& /*error*/) {
  CopyObject(isolate_credential, session_id, object_handle, attributes,
             new_object_handle, result);
}

uint32_t ChapsAdaptor::DestroyObject(const vector<uint8_t>& isolate_credential,
                                     const uint64_t& session_id,
                                     const uint64_t& object_handle) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "object_handle=" << object_handle;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->DestroyObject(isolate_credential_blob,
                                 session_id,
                                 object_handle);
}

uint32_t ChapsAdaptor::DestroyObject(const vector<uint8_t>& isolate_credential,
                                     const uint64_t& session_id,
                                     const uint64_t& object_handle,
                                     ::DBus::Error& /*error*/) {
  return DestroyObject(isolate_credential, session_id, object_handle);
}

void ChapsAdaptor::GetObjectSize(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& session_id,
                                 const uint64_t& object_handle,
                                 uint64_t& object_size,  // NOLINT - refs
                                 uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "object_handle=" << object_handle;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GetObjectSize(isolate_credential_blob,
                                   session_id,
                                   object_handle,
                                   &object_size);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "object_size=" << object_size;
}

void ChapsAdaptor::GetObjectSize(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& session_id,
                                 const uint64_t& object_handle,
                                 uint64_t& object_size,  // NOLINT - refs
                                 uint32_t& result,  // NOLINT - refs
                                 ::DBus::Error& /*error*/) {
  GetObjectSize(isolate_credential, session_id, object_handle, object_size,
                result);
}

void ChapsAdaptor::GetAttributeValue(const vector<uint8_t>& isolate_credential,
                                     const uint64_t& session_id,
                                     const uint64_t& object_handle,
                                     const vector<uint8_t>& attributes_in,
                                     vector<uint8_t>& attributes_out,  // NOLINT - refs
                                     uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "object_handle=" << object_handle;
  VLOG(2) << "IN: " << "attributes_in="
                    << PrintAttributes(attributes_in, false);
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GetAttributeValue(isolate_credential_blob,
                                       session_id,
                                       object_handle,
                                       attributes_in,
                                       &attributes_out);
  VLOG_IF(2, result == CKR_OK || result == CKR_ATTRIBUTE_TYPE_INVALID)
      << "OUT: " << "attributes_out=" << PrintAttributes(attributes_out, true);
}

void ChapsAdaptor::GetAttributeValue(const vector<uint8_t>& isolate_credential,
                                     const uint64_t& session_id,
                                     const uint64_t& object_handle,
                                     const vector<uint8_t>& attributes_in,
                                     vector<uint8_t>& attributes_out,  // NOLINT - refs
                                     uint32_t& result,  // NOLINT - refs
                                     ::DBus::Error& /*error*/) {
  GetAttributeValue(isolate_credential, session_id, object_handle,
                    attributes_in, attributes_out, result);
}

uint32_t ChapsAdaptor::SetAttributeValue(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& object_handle,
    const vector<uint8_t>& attributes) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "object_handle=" << object_handle;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->SetAttributeValue(isolate_credential_blob,
                                     session_id,
                                     object_handle,
                                     attributes);
}

uint32_t ChapsAdaptor::SetAttributeValue(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& object_handle,
    const vector<uint8_t>& attributes,
    ::DBus::Error& /*error*/) {
  return SetAttributeValue(isolate_credential, session_id, object_handle,
                           attributes);
}

uint32_t ChapsAdaptor::FindObjectsInit(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const vector<uint8_t>& attributes) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->FindObjectsInit(isolate_credential_blob,
                                   session_id,
                                   attributes);
}

uint32_t ChapsAdaptor::FindObjectsInit(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const vector<uint8_t>& attributes,
    ::DBus::Error& /*error*/) {
  return FindObjectsInit(isolate_credential, session_id, attributes);
}

void ChapsAdaptor::FindObjects(const vector<uint8_t>& isolate_credential,
                               const uint64_t& session_id,
                               const uint64_t& max_object_count,
                               vector<uint64_t>& object_list,  // NOLINT - refs
                               uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_object_count=" << max_object_count;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->FindObjects(isolate_credential_blob,
                                 session_id,
                                 max_object_count,
                                 &object_list);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "object_list="
                               << PrintIntVector(object_list);
}

void ChapsAdaptor::FindObjects(const vector<uint8_t>& isolate_credential,
                               const uint64_t& session_id,
                               const uint64_t& max_object_count,
                               vector<uint64_t>& object_list,  // NOLINT - refs
                               uint32_t& result,  // NOLINT - refs
                               ::DBus::Error& /*error*/) {
  FindObjects(isolate_credential, session_id, max_object_count, object_list,
              result);
}

uint32_t ChapsAdaptor::FindObjectsFinal(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->FindObjectsFinal(isolate_credential_blob, session_id);
}

uint32_t ChapsAdaptor::FindObjectsFinal(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    ::DBus::Error& /*error*/) {
  return FindObjectsFinal(isolate_credential, session_id);
}

uint32_t ChapsAdaptor::EncryptInit(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const uint64_t& key_handle) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->EncryptInit(isolate_credential_blob,
                               session_id,
                               mechanism_type,
                               mechanism_parameter,
                               key_handle);
}

uint32_t ChapsAdaptor::EncryptInit(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const uint64_t& key_handle,
    ::DBus::Error& /*error*/) {
  return EncryptInit(isolate_credential, session_id, mechanism_type,
                     mechanism_parameter, key_handle);
}

void ChapsAdaptor::Encrypt(const vector<uint8_t>& isolate_credential,
                           const uint64_t& session_id,
                           const vector<uint8_t>& data_in,
                           const uint64_t& max_out_length,
                           uint64_t& actual_out_length,  // NOLINT - refs
                           vector<uint8_t>& data_out,  // NOLINT - refs
                           uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->Encrypt(isolate_credential_blob,
                             session_id,
                             data_in,
                             max_out_length,
                             &actual_out_length,
                             &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::Encrypt(const vector<uint8_t>& isolate_credential,
                           const uint64_t& session_id,
                           const vector<uint8_t>& data_in,
                           const uint64_t& max_out_length,
                           uint64_t& actual_out_length,  // NOLINT - refs
                           vector<uint8_t>& data_out,  // NOLINT - refs
                           uint32_t& result,  // NOLINT - refs
                           ::DBus::Error& /*error*/) {
  Encrypt(isolate_credential, session_id, data_in, max_out_length,
          actual_out_length, data_out, result);
}

void ChapsAdaptor::EncryptUpdate(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& session_id,
                                 const vector<uint8_t>& data_in,
                                 const uint64_t& max_out_length,
                                 uint64_t& actual_out_length,  // NOLINT - refs
                                 vector<uint8_t>& data_out,  // NOLINT - refs
                                 uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->EncryptUpdate(isolate_credential_blob,
                                   session_id,
                                   data_in,
                                   max_out_length,
                                   &actual_out_length,
                                   &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::EncryptUpdate(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& session_id,
                                 const vector<uint8_t>& data_in,
                                 const uint64_t& max_out_length,
                                 uint64_t& actual_out_length,  // NOLINT - refs
                                 vector<uint8_t>& data_out,  // NOLINT - refs
                                 uint32_t& result,  // NOLINT - refs
                                 ::DBus::Error& /*error*/) {
  EncryptUpdate(isolate_credential, session_id, data_in, max_out_length,
                actual_out_length, data_out, result);
}

void ChapsAdaptor::EncryptFinal(const vector<uint8_t>& isolate_credential,
                                const uint64_t& session_id,
                                const uint64_t& max_out_length,
                                uint64_t& actual_out_length,  // NOLINT - refs
                                vector<uint8_t>& data_out,  // NOLINT - refs
                                uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->EncryptFinal(isolate_credential_blob,
                                  session_id,
                                  max_out_length,
                                  &actual_out_length,
                                  &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::EncryptFinal(const vector<uint8_t>& isolate_credential,
                                const uint64_t& session_id,
                                const uint64_t& max_out_length,
                                uint64_t& actual_out_length,  // NOLINT - refs
                                vector<uint8_t>& data_out,  // NOLINT - refs
                                uint32_t& result,  // NOLINT - refs
                                ::DBus::Error& /*error*/) {
  EncryptFinal(isolate_credential, session_id, max_out_length,
               actual_out_length, data_out, result);
}

uint32_t ChapsAdaptor::DecryptInit(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const uint64_t& key_handle) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->DecryptInit(isolate_credential_blob,
                               session_id,
                               mechanism_type,
                               mechanism_parameter,
                               key_handle);
}

uint32_t ChapsAdaptor::DecryptInit(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const uint64_t& key_handle,
    ::DBus::Error& /*error*/) {
  return DecryptInit(isolate_credential, session_id, mechanism_type,
                     mechanism_parameter, key_handle);
}

void ChapsAdaptor::Decrypt(const vector<uint8_t>& isolate_credential,
                           const uint64_t& session_id,
                           const vector<uint8_t>& data_in,
                           const uint64_t& max_out_length,
                           uint64_t& actual_out_length,  // NOLINT - refs
                           vector<uint8_t>& data_out,  // NOLINT - refs
                           uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->Decrypt(isolate_credential_blob,
                             session_id,
                             data_in,
                             max_out_length,
                             &actual_out_length,
                             &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::Decrypt(const vector<uint8_t>& isolate_credential,
                           const uint64_t& session_id,
                           const vector<uint8_t>& data_in,
                           const uint64_t& max_out_length,
                           uint64_t& actual_out_length,  // NOLINT - refs
                           vector<uint8_t>& data_out,  // NOLINT - refs
                           uint32_t& result,  // NOLINT - refs
                           ::DBus::Error& /*error*/) {
  Decrypt(isolate_credential, session_id, data_in, max_out_length,
          actual_out_length, data_out, result);
}

void ChapsAdaptor::DecryptUpdate(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& session_id,
                                 const vector<uint8_t>& data_in,
                                 const uint64_t& max_out_length,
                                 uint64_t& actual_out_length,  // NOLINT - refs
                                 vector<uint8_t>& data_out,  // NOLINT - refs
                                 uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->DecryptUpdate(isolate_credential_blob,
                                   session_id,
                                   data_in,
                                   max_out_length,
                                   &actual_out_length,
                                   &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::DecryptUpdate(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& session_id,
                                 const vector<uint8_t>& data_in,
                                 const uint64_t& max_out_length,
                                 uint64_t& actual_out_length,  // NOLINT - refs
                                 vector<uint8_t>& data_out,  // NOLINT - refs
                                 uint32_t& result,  // NOLINT - refs
                                 ::DBus::Error& /*error*/) {
  DecryptUpdate(isolate_credential, session_id, data_in, max_out_length,
                actual_out_length, data_out, result);
}

void ChapsAdaptor::DecryptFinal(const vector<uint8_t>& isolate_credential,
                                const uint64_t& session_id,
                                const uint64_t& max_out_length,
                                uint64_t& actual_out_length,  // NOLINT - refs
                                vector<uint8_t>& data_out,  // NOLINT - refs
                                uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->DecryptFinal(isolate_credential_blob,
                                  session_id,
                                  max_out_length,
                                  &actual_out_length,
                                  &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::DecryptFinal(const vector<uint8_t>& isolate_credential,
                                const uint64_t& session_id,
                                const uint64_t& max_out_length,
                                uint64_t& actual_out_length,  // NOLINT - refs
                                vector<uint8_t>& data_out,  // NOLINT - refs
                                uint32_t& result,  // NOLINT - refs
                                ::DBus::Error& /*error*/) {
  DecryptFinal(isolate_credential, session_id, max_out_length,
               actual_out_length, data_out, result);
}

uint32_t ChapsAdaptor::DigestInit(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->DigestInit(isolate_credential_blob,
                              session_id,
                              mechanism_type,
                              mechanism_parameter);
}

uint32_t ChapsAdaptor::DigestInit(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    ::DBus::Error& /*error*/) {
  return DigestInit(isolate_credential, session_id, mechanism_type,
                    mechanism_parameter);
}

void ChapsAdaptor::Digest(const vector<uint8_t>& isolate_credential,
                          const uint64_t& session_id,
                          const vector<uint8_t>& data_in,
                          const uint64_t& max_out_length,
                          uint64_t& actual_out_length,  // NOLINT - refs
                          vector<uint8_t>& digest,  // NOLINT - refs
                          uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->Digest(isolate_credential_blob,
                            session_id,
                            data_in,
                            max_out_length,
                            &actual_out_length,
                            &digest);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::Digest(const vector<uint8_t>& isolate_credential,
                          const uint64_t& session_id,
                          const vector<uint8_t>& data_in,
                          const uint64_t& max_out_length,
                          uint64_t& actual_out_length,  // NOLINT - refs
                          vector<uint8_t>& digest,  // NOLINT - refs
                          uint32_t& result,  // NOLINT - refs
                          ::DBus::Error& /*error*/) {
  Digest(isolate_credential, session_id, data_in, max_out_length,
         actual_out_length, digest, result);
}

uint32_t ChapsAdaptor::DigestUpdate(const vector<uint8_t>& isolate_credential,
                                    const uint64_t& session_id,
                                    const vector<uint8_t>& data_in) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->DigestUpdate(isolate_credential_blob,
                                session_id,
                                data_in);
}

uint32_t ChapsAdaptor::DigestUpdate(const vector<uint8_t>& isolate_credential,
                                    const uint64_t& session_id,
                                    const vector<uint8_t>& data_in,
                                    ::DBus::Error& /*error*/) {
  return DigestUpdate(isolate_credential, session_id, data_in);
}

uint32_t ChapsAdaptor::DigestKey(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& session_id,
                                 const uint64_t& key_handle) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->DigestKey(isolate_credential_blob,
                             session_id,
                             key_handle);
}

uint32_t ChapsAdaptor::DigestKey(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& session_id,
                                 const uint64_t& key_handle,
                                 ::DBus::Error& /*error*/) {
  return DigestKey(isolate_credential, session_id, key_handle);
}


void ChapsAdaptor::DigestFinal(const vector<uint8_t>& isolate_credential,
                               const uint64_t& session_id,
                               const uint64_t& max_out_length,
                               uint64_t& actual_out_length,  // NOLINT - refs
                               vector<uint8_t>& digest,  // NOLINT - refs
                               uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->DigestFinal(isolate_credential_blob,
                                 session_id,
                                 max_out_length,
                                 &actual_out_length,
                                 &digest);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::DigestFinal(const vector<uint8_t>& isolate_credential,
                               const uint64_t& session_id,
                               const uint64_t& max_out_length,
                               uint64_t& actual_out_length,  // NOLINT - refs
                               vector<uint8_t>& digest,  // NOLINT - refs
                               uint32_t& result,  // NOLINT - refs
                               ::DBus::Error& /*error*/) {
  DigestFinal(isolate_credential, session_id, max_out_length, actual_out_length,
              digest, result);
}

uint32_t ChapsAdaptor::SignInit(const vector<uint8_t>& isolate_credential,
                                const uint64_t& session_id,
                                const uint64_t& mechanism_type,
                                const vector<uint8_t>& mechanism_parameter,
                                const uint64_t& key_handle) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->SignInit(isolate_credential_blob,
                            session_id,
                            mechanism_type,
                            mechanism_parameter,
                            key_handle);
}

uint32_t ChapsAdaptor::SignInit(const vector<uint8_t>& isolate_credential,
                                const uint64_t& session_id,
                                const uint64_t& mechanism_type,
                                const vector<uint8_t>& mechanism_parameter,
                                const uint64_t& key_handle,
                                ::DBus::Error& /*error*/) {
  return SignInit(isolate_credential, session_id, mechanism_type,
                  mechanism_parameter, key_handle);
}

void ChapsAdaptor::Sign(const vector<uint8_t>& isolate_credential,
                        const uint64_t& session_id,
                        const vector<uint8_t>& data,
                        const uint64_t& max_out_length,
                        uint64_t& actual_out_length,  // NOLINT - refs
                        vector<uint8_t>& signature,  // NOLINT - refs
                        uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->Sign(isolate_credential_blob,
                          session_id,
                          data,
                          max_out_length,
                          &actual_out_length,
                          &signature);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::Sign(const vector<uint8_t>& isolate_credential,
                        const uint64_t& session_id,
                        const vector<uint8_t>& data,
                        const uint64_t& max_out_length,
                        uint64_t& actual_out_length,  // NOLINT - refs
                        vector<uint8_t>& signature,  // NOLINT - refs
                        uint32_t& result,  // NOLINT - refs
                        ::DBus::Error& /*error*/) {
  Sign(isolate_credential, session_id, data, max_out_length, actual_out_length,
       signature, result);
}

uint32_t ChapsAdaptor::SignUpdate(const vector<uint8_t>& isolate_credential,
                                  const uint64_t& session_id,
                                  const vector<uint8_t>& data_part) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->SignUpdate(isolate_credential_blob,
                              session_id,
                              data_part);
}

uint32_t ChapsAdaptor::SignUpdate(const vector<uint8_t>& isolate_credential,
                                  const uint64_t& session_id,
                                  const vector<uint8_t>& data_part,
                                  ::DBus::Error& /*error*/) {
  return SignUpdate(isolate_credential, session_id, data_part);
}

void ChapsAdaptor::SignFinal(const vector<uint8_t>& isolate_credential,
                             const uint64_t& session_id,
                             const uint64_t& max_out_length,
                             uint64_t& actual_out_length,  // NOLINT - refs
                             vector<uint8_t>& signature,  // NOLINT - refs
                             uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->SignFinal(isolate_credential_blob,
                               session_id,
                               max_out_length,
                               &actual_out_length,
                               &signature);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::SignFinal(const vector<uint8_t>& isolate_credential,
                             const uint64_t& session_id,
                             const uint64_t& max_out_length,
                             uint64_t& actual_out_length,  // NOLINT - refs
                             vector<uint8_t>& signature,  // NOLINT - refs
                             uint32_t& result,  // NOLINT - refs
                             ::DBus::Error& /*error*/) {
  SignFinal(isolate_credential, session_id, max_out_length, actual_out_length,
            signature, result);
}

uint32_t ChapsAdaptor::SignRecoverInit(
      const vector<uint8_t>& isolate_credential,
      const uint64_t& session_id,
      const uint64_t& mechanism_type,
      const vector<uint8_t>& mechanism_parameter,
      const uint64_t& key_handle) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->SignRecoverInit(isolate_credential_blob,
                                   session_id,
                                   mechanism_type,
                                   mechanism_parameter,
                                   key_handle);
}

uint32_t ChapsAdaptor::SignRecoverInit(
      const vector<uint8_t>& isolate_credential,
      const uint64_t& session_id,
      const uint64_t& mechanism_type,
      const vector<uint8_t>& mechanism_parameter,
      const uint64_t& key_handle,
      ::DBus::Error& /*error*/) {
  return SignRecoverInit(isolate_credential, session_id, mechanism_type,
                         mechanism_parameter, key_handle);
}

void ChapsAdaptor::SignRecover(const vector<uint8_t>& isolate_credential,
                               const uint64_t& session_id,
                               const vector<uint8_t>& data,
                               const uint64_t& max_out_length,
                               uint64_t& actual_out_length,  // NOLINT - refs
                               vector<uint8_t>& signature,  // NOLINT - refs
                               uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->SignRecover(isolate_credential_blob,
                                 session_id,
                                 data,
                                 max_out_length,
                                 &actual_out_length,
                                 &signature);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::SignRecover(const vector<uint8_t>& isolate_credential,
                               const uint64_t& session_id,
                               const vector<uint8_t>& data,
                               const uint64_t& max_out_length,
                               uint64_t& actual_out_length,  // NOLINT - refs
                               vector<uint8_t>& signature,  // NOLINT - refs
                               uint32_t& result,  // NOLINT - refs
                               ::DBus::Error& /*error*/) {
  SignRecover(isolate_credential, session_id, data, max_out_length,
              actual_out_length, signature, result);
}

uint32_t ChapsAdaptor::VerifyInit(const vector<uint8_t>& isolate_credential,
                                  const uint64_t& session_id,
                                  const uint64_t& mechanism_type,
                                  const vector<uint8_t>& mechanism_parameter,
                                  const uint64_t& key_handle) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->VerifyInit(isolate_credential_blob,
                              session_id,
                              mechanism_type,
                              mechanism_parameter,
                              key_handle);
}

uint32_t ChapsAdaptor::VerifyInit(const vector<uint8_t>& isolate_credential,
                                  const uint64_t& session_id,
                                  const uint64_t& mechanism_type,
                                  const vector<uint8_t>& mechanism_parameter,
                                  const uint64_t& key_handle,
                                  ::DBus::Error& /*error*/) {
  return VerifyInit(isolate_credential, session_id, mechanism_type,
                    mechanism_parameter, key_handle);
}

uint32_t ChapsAdaptor::Verify(const vector<uint8_t>& isolate_credential,
                              const uint64_t& session_id,
                              const vector<uint8_t>& data,
                              const vector<uint8_t>& signature) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->Verify(isolate_credential_blob,
                          session_id,
                          data,
                          signature);
}

uint32_t ChapsAdaptor::Verify(const vector<uint8_t>& isolate_credential,
                              const uint64_t& session_id,
                              const vector<uint8_t>& data,
                              const vector<uint8_t>& signature,
                              ::DBus::Error& /*error*/) {
  return Verify(isolate_credential, session_id, data, signature);
}

uint32_t ChapsAdaptor::VerifyUpdate(const vector<uint8_t>& isolate_credential,
                                    const uint64_t& session_id,
                                    const vector<uint8_t>& data_part) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->VerifyUpdate(isolate_credential_blob,
                                session_id,
                                data_part);
}

uint32_t ChapsAdaptor::VerifyUpdate(const vector<uint8_t>& isolate_credential,
                                    const uint64_t& session_id,
                                    const vector<uint8_t>& data_part,
                                    ::DBus::Error& /*error*/) {
  return VerifyUpdate(isolate_credential, session_id, data_part);
}

uint32_t ChapsAdaptor::VerifyFinal(const vector<uint8_t>& isolate_credential,
                                   const uint64_t& session_id,
                                   const vector<uint8_t>& signature) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->VerifyFinal(isolate_credential_blob,
                               session_id,
                               signature);
}

uint32_t ChapsAdaptor::VerifyFinal(const vector<uint8_t>& isolate_credential,
                                   const uint64_t& session_id,
                                   const vector<uint8_t>& signature,
                                   ::DBus::Error& /*error*/) {
  return VerifyFinal(isolate_credential, session_id, signature);
}

uint32_t ChapsAdaptor::VerifyRecoverInit(
      const vector<uint8_t>& isolate_credential,
      const uint64_t& session_id,
      const uint64_t& mechanism_type,
      const vector<uint8_t>& mechanism_parameter,
      const uint64_t& key_handle) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->VerifyRecoverInit(isolate_credential_blob,
                                     session_id,
                                     mechanism_type,
                                     mechanism_parameter,
                                     key_handle);
}

uint32_t ChapsAdaptor::VerifyRecoverInit(
      const vector<uint8_t>& isolate_credential,
      const uint64_t& session_id,
      const uint64_t& mechanism_type,
      const vector<uint8_t>& mechanism_parameter,
      const uint64_t& key_handle,
      ::DBus::Error& /*error*/) {
  return VerifyRecoverInit(isolate_credential, session_id, mechanism_type,
                           mechanism_parameter, key_handle);
}

void ChapsAdaptor::VerifyRecover(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& session_id,
                                 const vector<uint8_t>& signature,
                                 const uint64_t& max_out_length,
                                 uint64_t& actual_out_length,  // NOLINT - refs
                                 vector<uint8_t>& data,  // NOLINT - refs
                                 uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->VerifyRecover(isolate_credential_blob,
                                   session_id,
                                   signature,
                                   max_out_length,
                                   &actual_out_length,
                                   &data);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::VerifyRecover(const vector<uint8_t>& isolate_credential,
                                 const uint64_t& session_id,
                                 const vector<uint8_t>& signature,
                                 const uint64_t& max_out_length,
                                 uint64_t& actual_out_length,  // NOLINT - refs
                                 vector<uint8_t>& data,  // NOLINT - refs
                                 uint32_t& result,  // NOLINT - refs
                                 ::DBus::Error& /*error*/) {
  VerifyRecover(isolate_credential, session_id, signature, max_out_length,
                actual_out_length, data, result);
}

void ChapsAdaptor::DigestEncryptUpdate(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const vector<uint8_t>& data_in,
    const uint64_t& max_out_length,
    uint64_t& actual_out_length,  // NOLINT - refs
    vector<uint8_t>& data_out,  // NOLINT - refs
    uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->DigestEncryptUpdate(isolate_credential_blob,
                                         session_id,
                                         data_in,
                                         max_out_length,
                                         &actual_out_length,
                                         &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::DigestEncryptUpdate(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const vector<uint8_t>& data_in,
    const uint64_t& max_out_length,
    uint64_t& actual_out_length,  // NOLINT - refs
    vector<uint8_t>& data_out,  // NOLINT - refs
    uint32_t& result,  // NOLINT - refs
    ::DBus::Error& /*error*/) {
  DigestEncryptUpdate(isolate_credential, session_id, data_in, max_out_length,
                      actual_out_length, data_out, result);
}

void ChapsAdaptor::DecryptDigestUpdate(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const vector<uint8_t>& data_in,
    const uint64_t& max_out_length,
    uint64_t& actual_out_length,  // NOLINT - refs
    vector<uint8_t>& data_out,  // NOLINT - refs
    uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->DecryptDigestUpdate(isolate_credential_blob,
                                         session_id,
                                         data_in,
                                         max_out_length,
                                         &actual_out_length,
                                         &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::DecryptDigestUpdate(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const vector<uint8_t>& data_in,
    const uint64_t& max_out_length,
    uint64_t& actual_out_length,  // NOLINT - refs
    vector<uint8_t>& data_out,  // NOLINT - refs
    uint32_t& result,  // NOLINT - refs
    ::DBus::Error& /*error*/) {
  DecryptDigestUpdate(isolate_credential, session_id, data_in, max_out_length,
                      actual_out_length, data_out, result);
}

void ChapsAdaptor::SignEncryptUpdate(const vector<uint8_t>& isolate_credential,
                                     const uint64_t& session_id,
                                     const vector<uint8_t>& data_in,
                                     const uint64_t& max_out_length,
                                     uint64_t& actual_out_length,  // NOLINT - refs
                                     vector<uint8_t>& data_out,  // NOLINT - refs
                                     uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->SignEncryptUpdate(isolate_credential_blob,
                                       session_id,
                                       data_in,
                                       max_out_length,
                                       &actual_out_length,
                                       &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::SignEncryptUpdate(const vector<uint8_t>& isolate_credential,
                                     const uint64_t& session_id,
                                     const vector<uint8_t>& data_in,
                                     const uint64_t& max_out_length,
                                     uint64_t& actual_out_length,  // NOLINT - refs
                                     vector<uint8_t>& data_out,  // NOLINT - refs
                                     uint32_t& result,  // NOLINT - refs
                                     ::DBus::Error& /*error*/) {
  SignEncryptUpdate(isolate_credential, session_id, data_in, max_out_length,
                    actual_out_length, data_out, result);
}

void ChapsAdaptor::DecryptVerifyUpdate(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const vector<uint8_t>& data_in,
    const uint64_t& max_out_length,
    uint64_t& actual_out_length,  // NOLINT - refs
    vector<uint8_t>& data_out,  // NOLINT - refs
    uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->DecryptVerifyUpdate(isolate_credential_blob,
                                         session_id,
                                         data_in,
                                         max_out_length,
                                         &actual_out_length,
                                         &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::DecryptVerifyUpdate(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const vector<uint8_t>& data_in,
    const uint64_t& max_out_length,
    uint64_t& actual_out_length,  // NOLINT - refs
    vector<uint8_t>& data_out,  // NOLINT - refs
    uint32_t& result,  // NOLINT - refs
    ::DBus::Error& /*error*/) {
  DecryptVerifyUpdate(isolate_credential, session_id, data_in, max_out_length,
                      actual_out_length, data_out, result);
}

void ChapsAdaptor::GenerateKey(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const vector<uint8_t>& attributes,
    uint64_t& key_handle,  // NOLINT - refs
    uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GenerateKey(isolate_credential_blob,
                                 session_id,
                                 mechanism_type,
                                 mechanism_parameter,
                                 attributes,
                                 &key_handle);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "key_handle=" << key_handle;
}

void ChapsAdaptor::GenerateKey(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const vector<uint8_t>& attributes,
    uint64_t& key_handle,  // NOLINT - refs
    uint32_t& result,  // NOLINT - refs
    ::DBus::Error& /*error*/) {
  GenerateKey(isolate_credential, session_id, mechanism_type,
              mechanism_parameter, attributes, key_handle, result);
}

void ChapsAdaptor::GenerateKeyPair(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const vector<uint8_t>& public_attributes,
    const vector<uint8_t>& private_attributes,
    uint64_t& public_key_handle,  // NOLINT - refs
    uint64_t& private_key_handle,  // NOLINT - refs
    uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "public_attributes="
          << PrintAttributes(public_attributes, true);
  VLOG(2) << "IN: " << "private_attributes="
          << PrintAttributes(private_attributes, true);
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GenerateKeyPair(isolate_credential_blob,
                                     session_id,
                                     mechanism_type,
                                     mechanism_parameter,
                                     public_attributes,
                                     private_attributes,
                                     &public_key_handle,
                                     &private_key_handle);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "public_key_handle="
                               << public_key_handle;
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "private_key_handle="
                               << private_key_handle;
}

void ChapsAdaptor::GenerateKeyPair(
    const vector<uint8_t>& isolate_credential,
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const vector<uint8_t>& public_attributes,
    const vector<uint8_t>& private_attributes,
    uint64_t& public_key_handle,  // NOLINT - refs
    uint64_t& private_key_handle,  // NOLINT - refs
    uint32_t& result,  // NOLINT - refs
    ::DBus::Error& /*error*/) {
  GenerateKeyPair(isolate_credential, session_id, mechanism_type,
                  mechanism_parameter, public_attributes, private_attributes,
                  public_key_handle, private_key_handle, result);
}

void ChapsAdaptor::WrapKey(const vector<uint8_t>& isolate_credential,
                           const uint64_t& session_id,
                           const uint64_t& mechanism_type,
                           const vector<uint8_t>& mechanism_parameter,
                           const uint64_t& wrapping_key_handle,
                           const uint64_t& key_handle,
                           const uint64_t& max_out_length,
                           uint64_t& actual_out_length,  // NOLINT - refs
                           vector<uint8_t>& wrapped_key,  // NOLINT - refs
                           uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "wrapping_key_handle=" << wrapping_key_handle;
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->WrapKey(isolate_credential_blob,
                             session_id,
                             mechanism_type,
                             mechanism_parameter,
                             wrapping_key_handle,
                             key_handle,
                             max_out_length,
                             &actual_out_length,
                             &wrapped_key);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::WrapKey(const vector<uint8_t>& isolate_credential,
                           const uint64_t& session_id,
                           const uint64_t& mechanism_type,
                           const vector<uint8_t>& mechanism_parameter,
                           const uint64_t& wrapping_key_handle,
                           const uint64_t& key_handle,
                           const uint64_t& max_out_length,
                           uint64_t& actual_out_length,  // NOLINT - refs
                           vector<uint8_t>& wrapped_key,  // NOLINT - refs
                           uint32_t& result,  // NOLINT - refs
                           ::DBus::Error& /*error*/) {
  WrapKey(isolate_credential, session_id, mechanism_type, mechanism_parameter,
          wrapping_key_handle, key_handle,  max_out_length, actual_out_length,
          wrapped_key, result);
}

void ChapsAdaptor::UnwrapKey(const vector<uint8_t>& isolate_credential,
                             const uint64_t& session_id,
                             const uint64_t& mechanism_type,
                             const vector<uint8_t>& mechanism_parameter,
                             const uint64_t& wrapping_key_handle,
                             const vector<uint8_t>& wrapped_key,
                             const vector<uint8_t>& attributes,
                             uint64_t& key_handle,  // NOLINT - refs
                             uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "wrapping_key_handle=" << wrapping_key_handle;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->UnwrapKey(isolate_credential_blob,
                               session_id,
                               mechanism_type,
                               mechanism_parameter,
                               wrapping_key_handle,
                               wrapped_key,
                               attributes,
                               &key_handle);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "key_handle=" << key_handle;
}

void ChapsAdaptor::UnwrapKey(const vector<uint8_t>& isolate_credential,
                             const uint64_t& session_id,
                             const uint64_t& mechanism_type,
                             const vector<uint8_t>& mechanism_parameter,
                             const uint64_t& wrapping_key_handle,
                             const vector<uint8_t>& wrapped_key,
                             const vector<uint8_t>& attributes,
                             uint64_t& key_handle,  // NOLINT - refs
                             uint32_t& result,  // NOLINT - refs
                             ::DBus::Error& /*error*/) {
  UnwrapKey(isolate_credential, session_id, mechanism_type, mechanism_parameter,
            wrapping_key_handle, wrapped_key, attributes, key_handle, result);
}

void ChapsAdaptor::DeriveKey(const vector<uint8_t>& isolate_credential,
                             const uint64_t& session_id,
                             const uint64_t& mechanism_type,
                             const vector<uint8_t>& mechanism_parameter,
                             const uint64_t& base_key_handle,
                             const vector<uint8_t>& attributes,
                             uint64_t& key_handle,  // NOLINT - refs
                             uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "base_key_handle=" << base_key_handle;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->DeriveKey(isolate_credential_blob,
                               session_id,
                               mechanism_type,
                               mechanism_parameter,
                               base_key_handle,
                               attributes,
                               &key_handle);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "key_handle=" << key_handle;
}

void ChapsAdaptor::DeriveKey(const vector<uint8_t>& isolate_credential,
                             const uint64_t& session_id,
                             const uint64_t& mechanism_type,
                             const vector<uint8_t>& mechanism_parameter,
                             const uint64_t& base_key_handle,
                             const vector<uint8_t>& attributes,
                             uint64_t& key_handle,  // NOLINT - refs
                             uint32_t& result,  // NOLINT - refs
                             ::DBus::Error& /*error*/) {
  DeriveKey(isolate_credential, session_id, mechanism_type, mechanism_parameter,
            base_key_handle, attributes, key_handle, result);
}

uint32_t ChapsAdaptor::SeedRandom(const vector<uint8_t>& isolate_credential,
                                  const uint64_t& session_id,
                                  const vector<uint8_t>& seed) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "num_bytes=" << seed.size();
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  return service_->SeedRandom(isolate_credential_blob,
                              session_id,
                              seed);
}

uint32_t ChapsAdaptor::SeedRandom(const vector<uint8_t>& isolate_credential,
                                  const uint64_t& session_id,
                                  const vector<uint8_t>& seed,
                                  ::DBus::Error& /*error*/) {
  return SeedRandom(isolate_credential, session_id, seed);
}

void ChapsAdaptor::GenerateRandom(const vector<uint8_t>& isolate_credential,
                                  const uint64_t& session_id,
                                  const uint64_t& num_bytes,
                                  vector<uint8_t>& random_data,  // NOLINT - refs
                                  uint32_t& result) {  // NOLINT - refs
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "num_bytes=" << num_bytes;
  SecureBlob isolate_credential_blob(&isolate_credential.front(),
                                     isolate_credential.size());
  ClearVector(const_cast<vector<uint8_t>*>(&isolate_credential));
  result = service_->GenerateRandom(isolate_credential_blob,
                                    session_id,
                                    num_bytes,
                                    &random_data);
}

void ChapsAdaptor::GenerateRandom(const vector<uint8_t>& isolate_credential,
                                  const uint64_t& session_id,
                                  const uint64_t& num_bytes,
                                  vector<uint8_t>& random_data,  // NOLINT - refs
                                  uint32_t& result,  // NOLINT - refs
                                  ::DBus::Error& /*error*/) {
  GenerateRandom(isolate_credential, session_id, num_bytes, random_data,
                 result);
}

}  // namespace chaps
