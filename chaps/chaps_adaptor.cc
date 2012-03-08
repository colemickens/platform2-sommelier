// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_adaptor.h"

#include <base/file_path.h>
#include <base/logging.h>
#include <base/synchronization/lock.h>

#include "chaps/chaps.h"
#include "chaps/chaps_interface.h"
#include "chaps/chaps_utility.h"
#include "chaps/login_event_listener.h"

using base::AutoLock;
using base::Lock;
using std::string;
using std::vector;

namespace chaps {

// Helper used when calling the ObjectAdaptor constructor.
static DBus::Connection& GetConnection() {
  static DBus::Connection connection = DBus::Connection::SystemBus();
  connection.request_name(kChapsServiceName);
  return connection;
}

ChapsAdaptor::ChapsAdaptor(Lock* lock,
                           ChapsInterface* service,
                           LoginEventListener* login_listener)
    : DBus::ObjectAdaptor(GetConnection(),
                          DBus::Path(kChapsServicePath),
                          REGISTER_NOW),
      lock_(lock),
      service_(service),
      login_listener_(login_listener) {}

ChapsAdaptor::~ChapsAdaptor() {}

void ChapsAdaptor::OnLogin(const string& path,
                           const vector<uint8_t>& auth_data,
                           ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  if (login_listener_)
    login_listener_->OnLogin(FilePath(path),
                             ConvertByteVectorToString(auth_data));
}

void ChapsAdaptor::OnLogout(const string& path,
                            ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  if (login_listener_)
    login_listener_->OnLogout(FilePath(path));
}

void ChapsAdaptor::OnChangeAuthData(const string& path,
                                    const vector<uint8_t>& old_auth_data,
                                    const vector<uint8_t>& new_auth_data,
                                    ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  if (login_listener_)
    login_listener_->OnChangeAuthData(FilePath(path),
                                      ConvertByteVectorToString(old_auth_data),
                                      ConvertByteVectorToString(new_auth_data));
}

void ChapsAdaptor::GetSlotList(const bool& token_present,
                               vector<uint64_t>& slot_list,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "token_present=" << token_present;
  result = service_->GetSlotList(token_present, &slot_list);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "slot_list="
                               << PrintIntVector(slot_list);
}

void ChapsAdaptor::GetSlotInfo(const uint64_t& slot_id,
                               vector<uint8_t>& slot_description,
                               vector<uint8_t>& manufacturer_id,
                               uint64_t& flags,
                               uint8_t& hardware_version_major,
                               uint8_t& hardware_version_minor,
                               uint8_t& firmware_version_major,
                               uint8_t& firmware_version_minor,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  result = service_->GetSlotInfo(slot_id,
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

void ChapsAdaptor::GetTokenInfo(const uint64_t& slot_id,
                                vector<uint8_t>& label,
                                vector<uint8_t>& manufacturer_id,
                                vector<uint8_t>& model,
                                vector<uint8_t>& serial_number,
                                uint64_t& flags,
                                uint64_t& max_session_count,
                                uint64_t& session_count,
                                uint64_t& max_session_count_rw,
                                uint64_t& session_count_rw,
                                uint64_t& max_pin_len,
                                uint64_t& min_pin_len,
                                uint64_t& total_public_memory,
                                uint64_t& free_public_memory,
                                uint64_t& total_private_memory,
                                uint64_t& free_private_memory,
                                uint8_t& hardware_version_major,
                                uint8_t& hardware_version_minor,
                                uint8_t& firmware_version_major,
                                uint8_t& firmware_version_minor,
                                uint32_t& result,
                                ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  result = service_->GetTokenInfo(slot_id,
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

void ChapsAdaptor::GetMechanismList(const uint64_t& slot_id,
                                    vector<uint64_t>& mechanism_list,
                                    uint32_t& result,
                                    ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  result = service_->GetMechanismList(slot_id, &mechanism_list);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "mechanism_list="
                               << PrintIntVector(mechanism_list);
}

void ChapsAdaptor::GetMechanismInfo(const uint64_t& slot_id,
                                    const uint64_t& mechanism_type,
                                    uint64_t& min_key_size,
                                    uint64_t& max_key_size,
                                    uint64_t& flags,
                                    uint32_t& result,
                                    ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  result = service_->GetMechanismInfo(slot_id,
                                      mechanism_type,
                                      &min_key_size,
                                      &max_key_size,
                                      &flags);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "min_key_size=" << min_key_size;
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "max_key_size=" << max_key_size;
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "flags=" << flags;
}

uint32_t ChapsAdaptor::InitToken(const uint64_t& slot_id,
                                 const bool& use_null_pin,
                                 const string& optional_so_pin,
                                 const vector<uint8_t>& new_token_label,
                                 ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  VLOG(2) << "IN: " << "new_token_label="
                    << ConvertByteVectorToString(new_token_label);
  const string* tmp_pin = use_null_pin ? NULL : &optional_so_pin;
  return service_->InitToken(slot_id, tmp_pin, new_token_label);
}

uint32_t ChapsAdaptor::InitPIN(const uint64_t& session_id,
                               const bool& use_null_pin,
                               const string& optional_user_pin,
                               ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "use_null_pin=" << use_null_pin;
  const string* tmp_pin = use_null_pin ? NULL : &optional_user_pin;
  return service_->InitPIN(session_id, tmp_pin);
}

uint32_t ChapsAdaptor::SetPIN(const uint64_t& session_id,
                              const bool& use_null_old_pin,
                              const string& optional_old_pin,
                              const bool& use_null_new_pin,
                              const string& optional_new_pin,
                              ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "use_null_old_pin=" << use_null_old_pin;
  VLOG(2) << "IN: " << "use_null_new_pin=" << use_null_new_pin;
  const string* tmp_old_pin = use_null_old_pin ? NULL : &optional_old_pin;
  const string* tmp_new_pin = use_null_new_pin ? NULL : &optional_new_pin;
  return service_->SetPIN(session_id, tmp_old_pin, tmp_new_pin);
}

void ChapsAdaptor::OpenSession(const uint64_t& slot_id, const uint64_t& flags,
                               uint64_t& session_id,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  VLOG(2) << "IN: " << "flags=" << flags;
  result = service_->OpenSession(slot_id, flags, &session_id);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "session_id=" << session_id;
}

uint32_t ChapsAdaptor::CloseSession(const uint64_t& session_id,
                                    ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  return service_->CloseSession(session_id);
}

uint32_t ChapsAdaptor::CloseAllSessions(const uint64_t& slot_id,
                                        ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "slot_id=" << slot_id;
  return service_->CloseAllSessions(slot_id);
}

void ChapsAdaptor::GetSessionInfo(const uint64_t& session_id,
                                  uint64_t& slot_id,
                                  uint64_t& state,
                                  uint64_t& flags,
                                  uint64_t& device_error,
                                  uint32_t& result,
                                  ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  result = service_->GetSessionInfo(session_id,
                                    &slot_id,
                                    &state,
                                    &flags,
                                    &device_error);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "slot_id=" << slot_id;
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "state=" << state;
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "flags=" << flags;
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "device_error=" << device_error;
}

void ChapsAdaptor::GetOperationState(const uint64_t& session_id,
                                     vector<uint8_t>& operation_state,
                                     uint32_t& result,
                                     ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  result = service_->GetOperationState(session_id, &operation_state);
}

uint32_t ChapsAdaptor::SetOperationState(
      const uint64_t& session_id,
      const vector<uint8_t>& operation_state,
      const uint64_t& encryption_key_handle,
      const uint64_t& authentication_key_handle,
      ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  return service_->SetOperationState(session_id,
                                     operation_state,
                                     encryption_key_handle,
                                     authentication_key_handle);
}

uint32_t ChapsAdaptor::Login(const uint64_t& session_id,
                         const uint64_t& user_type,
                         const bool& use_null_pin,
                         const string& optional_pin,
                         ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "user_type=" << user_type;
  VLOG(2) << "IN: " << "use_null_pin=" << use_null_pin;
  const string* pin = use_null_pin ? NULL : &optional_pin;
  return service_->Login(session_id, user_type, pin);
}

uint32_t ChapsAdaptor::Logout(const uint64_t& session_id,
                              ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  return service_->Logout(session_id);
}

void ChapsAdaptor::CreateObject(
      const uint64_t& session_id,
      const vector<uint8_t>& attributes,
      uint64_t& new_object_handle,
      uint32_t& result,
      ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  result = service_->CreateObject(session_id,
                                  attributes,
                                  &new_object_handle);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "new_object_handle="
                               << new_object_handle;
}

void ChapsAdaptor::CopyObject(
      const uint64_t& session_id,
      const uint64_t& object_handle,
      const vector<uint8_t>& attributes,
      uint64_t& new_object_handle,
      uint32_t& result,
      ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "object_handle=" << object_handle;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  result = service_->CopyObject(session_id,
                                object_handle,
                                attributes,
                                &new_object_handle);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "new_object_handle="
                               << new_object_handle;
}

uint32_t ChapsAdaptor::DestroyObject(const uint64_t& session_id,
                                 const uint64_t& object_handle,
                                 ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "object_handle=" << object_handle;
  return service_->DestroyObject(session_id, object_handle);
}

void ChapsAdaptor::GetObjectSize(const uint64_t& session_id,
                           const uint64_t& object_handle,
                           uint64_t& object_size,
                           uint32_t& result,
                           ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "object_handle=" << object_handle;
  result = service_->GetObjectSize(session_id, object_handle, &object_size);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "object_size=" << object_size;
}

void ChapsAdaptor::GetAttributeValue(const uint64_t& session_id,
                               const uint64_t& object_handle,
                               const vector<uint8_t>& attributes_in,
                               vector<uint8_t>& attributes_out,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "object_handle=" << object_handle;
  VLOG(2) << "IN: " << "attributes_in="
                    << PrintAttributes(attributes_in, false);
  result = service_->GetAttributeValue(session_id,
                                       object_handle,
                                       attributes_in,
                                       &attributes_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "attributes_out="
                               << PrintAttributes(attributes_out, true);
}

uint32_t ChapsAdaptor::SetAttributeValue(const uint64_t& session_id,
                                   const uint64_t& object_handle,
                                   const vector<uint8_t>& attributes,
                                   ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "object_handle=" << object_handle;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  return service_->SetAttributeValue(session_id,
                                     object_handle,
                                     attributes);
}

uint32_t ChapsAdaptor::FindObjectsInit(const uint64_t& session_id,
                                       const vector<uint8_t>& attributes,
                                       ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  return service_->FindObjectsInit(session_id, attributes);
}

void ChapsAdaptor::FindObjects(const uint64_t& session_id,
                               const uint64_t& max_object_count,
                               vector<uint64_t>& object_list,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_object_count=" << max_object_count;
  result = service_->FindObjects(session_id, max_object_count, &object_list);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "object_list="
                               << PrintIntVector(object_list);
}

uint32_t ChapsAdaptor::FindObjectsFinal(const uint64_t& session_id,
                                        ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  return service_->FindObjectsFinal(session_id);
}

uint32_t ChapsAdaptor::EncryptInit(
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const uint64_t& key_handle,
    ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  return service_->EncryptInit(session_id,
                               mechanism_type,
                               mechanism_parameter,
                               key_handle);
}

void ChapsAdaptor::Encrypt(const uint64_t& session_id,
                           const vector<uint8_t>& data_in,
                           const uint64_t& max_out_length,
                           uint64_t& actual_out_length,
                           vector<uint8_t>& data_out,
                           uint32_t& result,
                           ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->Encrypt(session_id,
                             data_in,
                             max_out_length,
                             &actual_out_length,
                             &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::EncryptUpdate(const uint64_t& session_id,
                                 const vector<uint8_t>& data_in,
                                 const uint64_t& max_out_length,
                                 uint64_t& actual_out_length,
                                 vector<uint8_t>& data_out,
                                 uint32_t& result, ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->EncryptUpdate(session_id,
                                   data_in,
                                   max_out_length,
                                   &actual_out_length,
                                   &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::EncryptFinal(const uint64_t& session_id,
                                const uint64_t& max_out_length,
                                uint64_t& actual_out_length,
                                vector<uint8_t>& data_out,
                                uint32_t& result,
                                ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->EncryptFinal(session_id,
                                  max_out_length,
                                  &actual_out_length,
                                  &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

uint32_t ChapsAdaptor::DecryptInit(
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const uint64_t& key_handle,
    ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  return service_->DecryptInit(session_id,
                               mechanism_type,
                               mechanism_parameter,
                               key_handle);
}

void ChapsAdaptor::Decrypt(const uint64_t& session_id,
                           const vector<uint8_t>& data_in,
                           const uint64_t& max_out_length,
                           uint64_t& actual_out_length,
                           vector<uint8_t>& data_out,
                           uint32_t& result,
                           ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->Decrypt(session_id,
                             data_in,
                             max_out_length,
                             &actual_out_length,
                             &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::DecryptUpdate(const uint64_t& session_id,
                                 const vector<uint8_t>& data_in,
                                 const uint64_t& max_out_length,
                                 uint64_t& actual_out_length,
                                 vector<uint8_t>& data_out,
                                 uint32_t& result,
                                 ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->DecryptUpdate(session_id,
                                   data_in,
                                   max_out_length,
                                   &actual_out_length,
                                   &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::DecryptFinal(const uint64_t& session_id,
                                const uint64_t& max_out_length,
                                uint64_t& actual_out_length,
                                vector<uint8_t>& data_out,
                                uint32_t& result,
                                ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->DecryptFinal(session_id,
                                  max_out_length,
                                  &actual_out_length,
                                  &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

uint32_t ChapsAdaptor::DigestInit(
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  return service_->DigestInit(session_id,
                              mechanism_type,
                              mechanism_parameter);
}

void ChapsAdaptor::Digest(const uint64_t& session_id,
                          const vector<uint8_t>& data_in,
                          const uint64_t& max_out_length,
                          uint64_t& actual_out_length,
                          vector<uint8_t>& digest,
                          uint32_t& result,
                          ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->Digest(session_id,
                            data_in,
                            max_out_length,
                            &actual_out_length,
                            &digest);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

uint32_t ChapsAdaptor::DigestUpdate(const uint64_t& session_id,
                                    const vector<uint8_t>& data_in,
                                    ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  return service_->DigestUpdate(session_id, data_in);
}

uint32_t ChapsAdaptor::DigestKey(const uint64_t& session_id,
                                 const uint64_t& key_handle,
                                 ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  return service_->DigestKey(session_id, key_handle);
}

void ChapsAdaptor::DigestFinal(const uint64_t& session_id,
                               const uint64_t& max_out_length,
                               uint64_t& actual_out_length,
                               vector<uint8_t>& digest,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->DigestFinal(session_id,
                                 max_out_length,
                                 &actual_out_length,
                                 &digest);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

uint32_t ChapsAdaptor::SignInit(const uint64_t& session_id,
                                const uint64_t& mechanism_type,
                                const vector<uint8_t>& mechanism_parameter,
                                const uint64_t& key_handle,
                                ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  return service_->SignInit(session_id,
                            mechanism_type,
                            mechanism_parameter,
                            key_handle);
}

void ChapsAdaptor::Sign(const uint64_t& session_id,
                        const vector<uint8_t>& data,
                        const uint64_t& max_out_length,
                        uint64_t& actual_out_length,
                        vector<uint8_t>& signature,
                        uint32_t& result,
                        ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->Sign(session_id,
                          data,
                          max_out_length,
                          &actual_out_length,
                          &signature);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

uint32_t ChapsAdaptor::SignUpdate(const uint64_t& session_id,
                                  const vector<uint8_t>& data_part,
                                  ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  return service_->SignUpdate(session_id, data_part);
}

void ChapsAdaptor::SignFinal(const uint64_t& session_id,
                             const uint64_t& max_out_length,
                             uint64_t& actual_out_length,
                             vector<uint8_t>& signature,
                             uint32_t& result, ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->SignFinal(session_id,
                               max_out_length,
                               &actual_out_length,
                               &signature);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

uint32_t ChapsAdaptor::SignRecoverInit(
      const uint64_t& session_id,
      const uint64_t& mechanism_type,
      const vector<uint8_t>& mechanism_parameter,
      const uint64_t& key_handle,
      ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  return service_->SignRecoverInit(session_id,
                                   mechanism_type,
                                   mechanism_parameter,
                                   key_handle);
}

void ChapsAdaptor::SignRecover(const uint64_t& session_id,
                               const vector<uint8_t>& data,
                               const uint64_t& max_out_length,
                               uint64_t& actual_out_length,
                               vector<uint8_t>& signature,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->SignRecover(session_id,
                                 data,
                                 max_out_length,
                                 &actual_out_length,
                                 &signature);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

uint32_t ChapsAdaptor::VerifyInit(const uint64_t& session_id,
                                  const uint64_t& mechanism_type,
                                  const vector<uint8_t>& mechanism_parameter,
                                  const uint64_t& key_handle,
                                  ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  return service_->VerifyInit(session_id,
                              mechanism_type,
                              mechanism_parameter,
                              key_handle);
}

uint32_t ChapsAdaptor::Verify(const uint64_t& session_id,
                              const vector<uint8_t>& data,
                              const vector<uint8_t>& signature,
                              ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  return service_->Verify(session_id, data, signature);
}

uint32_t ChapsAdaptor::VerifyUpdate(const uint64_t& session_id,
                                    const vector<uint8_t>& data_part,
                                    ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  return service_->VerifyUpdate(session_id, data_part);
}

uint32_t ChapsAdaptor::VerifyFinal(const uint64_t& session_id,
                                   const vector<uint8_t>& signature,
                                   ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  return service_->VerifyFinal(session_id, signature);
}

uint32_t ChapsAdaptor::VerifyRecoverInit(
      const uint64_t& session_id,
      const uint64_t& mechanism_type,
      const vector<uint8_t>& mechanism_parameter,
      const uint64_t& key_handle,
      ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  return service_->VerifyRecoverInit(session_id,
                                     mechanism_type,
                                     mechanism_parameter,
                                     key_handle);
}

void ChapsAdaptor::VerifyRecover(const uint64_t& session_id,
                                 const vector<uint8_t>& signature,
                                 const uint64_t& max_out_length,
                                 uint64_t& actual_out_length,
                                 vector<uint8_t>& data,
                                 uint32_t& result,
                                 ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->VerifyRecover(session_id,
                                   signature,
                                   max_out_length,
                                   &actual_out_length,
                                   &data);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::DigestEncryptUpdate(const uint64_t& session_id,
                                       const vector<uint8_t>& data_in,
                                       const uint64_t& max_out_length,
                                       uint64_t& actual_out_length,
                                       vector<uint8_t>& data_out,
                                       uint32_t& result,
                                       ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->DigestEncryptUpdate(session_id,
                                         data_in,
                                         max_out_length,
                                         &actual_out_length,
                                         &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::DecryptDigestUpdate(const uint64_t& session_id,
                                       const vector<uint8_t>& data_in,
                                       const uint64_t& max_out_length,
                                       uint64_t& actual_out_length,
                                       vector<uint8_t>& data_out,
                                       uint32_t& result,
                                       ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->DecryptDigestUpdate(session_id,
                                         data_in,
                                         max_out_length,
                                         &actual_out_length,
                                         &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::SignEncryptUpdate(const uint64_t& session_id,
                                     const vector<uint8_t>& data_in,
                                     const uint64_t& max_out_length,
                                     uint64_t& actual_out_length,
                                     vector<uint8_t>& data_out,
                                     uint32_t& result,
                                     ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->SignEncryptUpdate(session_id,
                                       data_in,
                                       max_out_length,
                                       &actual_out_length,
                                       &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::DecryptVerifyUpdate(const uint64_t& session_id,
                                       const vector<uint8_t>& data_in,
                                       const uint64_t& max_out_length,
                                       uint64_t& actual_out_length,
                                       vector<uint8_t>& data_out,
                                       uint32_t& result,
                                       ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->DecryptVerifyUpdate(session_id,
                                         data_in,
                                         max_out_length,
                                         &actual_out_length,
                                         &data_out);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "actual_out_length="
                               << actual_out_length;
}

void ChapsAdaptor::GenerateKey(
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const vector<uint8_t>& attributes,
    uint64_t& key_handle,
    uint32_t& result,
    ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  result = service_->GenerateKey(session_id,
                                 mechanism_type,
                                 mechanism_parameter,
                                 attributes,
                                 &key_handle);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "key_handle=" << key_handle;
}

void ChapsAdaptor::GenerateKeyPair(
    const uint64_t& session_id,
    const uint64_t& mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const vector<uint8_t>& public_attributes,
    const vector<uint8_t>& private_attributes,
    uint64_t& public_key_handle,
    uint64_t& private_key_handle,
    uint32_t& result,
    ::DBus::Error& /*error*/) {
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
  result = service_->GenerateKeyPair(session_id,
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

void ChapsAdaptor::WrapKey(const uint64_t& session_id,
                           const uint64_t& mechanism_type,
                           const vector<uint8_t>& mechanism_parameter,
                           const uint64_t& wrapping_key_handle,
                           const uint64_t& key_handle,
                           const uint64_t& max_out_length,
                           uint64_t& actual_out_length,
                           vector<uint8_t>& wrapped_key,
                           uint32_t& result,
                           ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "wrapping_key_handle=" << wrapping_key_handle;
  VLOG(2) << "IN: " << "key_handle=" << key_handle;
  VLOG(2) << "IN: " << "max_out_length=" << max_out_length;
  result = service_->WrapKey(session_id,
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

void ChapsAdaptor::UnwrapKey(const uint64_t& session_id,
                             const uint64_t& mechanism_type,
                             const vector<uint8_t>& mechanism_parameter,
                             const uint64_t& wrapping_key_handle,
                             const vector<uint8_t>& wrapped_key,
                             const vector<uint8_t>& attributes,
                             uint64_t& key_handle,
                             uint32_t& result,
                             ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "wrapping_key_handle=" << wrapping_key_handle;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  result = service_->UnwrapKey(session_id,
                               mechanism_type,
                               mechanism_parameter,
                               wrapping_key_handle,
                               wrapped_key,
                               attributes,
                               &key_handle);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "key_handle=" << key_handle;
}

void ChapsAdaptor::DeriveKey(const uint64_t& session_id,
                             const uint64_t& mechanism_type,
                             const vector<uint8_t>& mechanism_parameter,
                             const uint64_t& base_key_handle,
                             const vector<uint8_t>& attributes,
                             uint64_t& key_handle,
                             uint32_t& result,
                             ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "mechanism_type=" << mechanism_type;
  VLOG(2) << "IN: " << "mechanism_parameter="
          << PrintIntVector(mechanism_parameter);
  VLOG(2) << "IN: " << "base_key_handle=" << base_key_handle;
  VLOG(2) << "IN: " << "attributes=" << PrintAttributes(attributes, true);
  result = service_->DeriveKey(session_id,
                               mechanism_type,
                               mechanism_parameter,
                               base_key_handle,
                               attributes,
                               &key_handle);
  VLOG_IF(2, result == CKR_OK) << "OUT: " << "key_handle=" << key_handle;
}

uint32_t ChapsAdaptor::SeedRandom(const uint64_t& session_id,
                              const vector<uint8_t>& seed,
                              ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "num_bytes=" << seed.size();
  return service_->SeedRandom(session_id, seed);
}

void ChapsAdaptor::GenerateRandom(const uint64_t& session_id,
                              const uint64_t& num_bytes,
                              vector<uint8_t>& random_data,
                              uint32_t& result,
                              ::DBus::Error& /*error*/) {
  AutoLock lock(*lock_);
  VLOG(1) << "CALL: " << __func__;
  VLOG(2) << "IN: " << "session_id=" << session_id;
  VLOG(2) << "IN: " << "num_bytes=" << num_bytes;
  result = service_->GenerateRandom(session_id, num_bytes, &random_data);
}

}  // namespace
