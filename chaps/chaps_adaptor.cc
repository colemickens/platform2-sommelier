// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_adaptor.h"

#include "chaps/chaps.h"
#include "chaps/chaps_interface.h"
#include "chaps/chaps_utility.h"

using std::string;
using std::vector;

namespace chaps {

// Helper used when calling the ObjectAdaptor constructor.
static DBus::Connection& GetConnection() {
  static DBus::Connection connection = DBus::Connection::SystemBus();
  connection.request_name(kChapsServiceName);
  return connection;
}

ChapsAdaptor::ChapsAdaptor(ChapsInterface* service)
    : DBus::ObjectAdaptor(GetConnection(), DBus::Path(kChapsServicePath),
                          REGISTER_NOW),
      service_(service) {}

ChapsAdaptor::~ChapsAdaptor() {}

void ChapsAdaptor::GetSlotList(const bool& token_present,
                               vector<uint32_t>& slotList,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  result = service_->GetSlotList(token_present, &slotList);
}

void ChapsAdaptor::GetSlotInfo(const uint32_t& slot_id,
                               string& slot_description,
                               string& manufacturer_id,
                               uint32_t& flags,
                               uint8_t& hardware_version_major,
                               uint8_t& hardware_version_minor,
                               uint8_t& firmware_version_major,
                               uint8_t& firmware_version_minor,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  result = service_->GetSlotInfo(slot_id,
                                 &slot_description,
                                 &manufacturer_id,
                                 &flags,
                                 &hardware_version_major,
                                 &hardware_version_minor,
                                 &firmware_version_major,
                                 &firmware_version_minor);
}

void ChapsAdaptor::GetTokenInfo(const uint32_t& slot_id,
                                string& label,
                                string& manufacturer_id,
                                string& model,
                                string& serial_number,
                                uint32_t& flags,
                                uint32_t& max_session_count,
                                uint32_t& session_count,
                                uint32_t& max_session_count_rw,
                                uint32_t& session_count_rw,
                                uint32_t& max_pin_len,
                                uint32_t& min_pin_len,
                                uint32_t& total_public_memory,
                                uint32_t& free_public_memory,
                                uint32_t& total_private_memory,
                                uint32_t& free_private_memory,
                                uint8_t& hardware_version_major,
                                uint8_t& hardware_version_minor,
                                uint8_t& firmware_version_major,
                                uint8_t& firmware_version_minor,
                                uint32_t& result,
                                ::DBus::Error& /*error*/) {
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
}

void ChapsAdaptor::GetMechanismList(const uint32_t& slot_id,
                                    vector<uint32_t>& mechanism_list,
                                    uint32_t& result,
                                    ::DBus::Error& /*error*/) {
  result = service_->GetMechanismList(slot_id, &mechanism_list);
}

void ChapsAdaptor::GetMechanismInfo(const uint32_t& slot_id,
                                    const uint32_t& mechanism_type,
                                    uint32_t& min_key_size,
                                    uint32_t& max_key_size,
                                    uint32_t& flags,
                                    uint32_t& result,
                                    ::DBus::Error& /*error*/) {
  result = service_->GetMechanismInfo(slot_id,
                                      mechanism_type,
                                      &min_key_size,
                                      &max_key_size,
                                      &flags);
}

uint32_t ChapsAdaptor::InitToken(const uint32_t& slot_id,
                                 const bool& use_null_pin,
                                 const string& optional_so_pin,
                                 const string& new_token_label,
                                 ::DBus::Error& /*error*/) {
  const string* tmp_pin = use_null_pin ? NULL : &optional_so_pin;
  return service_->InitToken(slot_id, tmp_pin, new_token_label);
}

uint32_t ChapsAdaptor::InitPIN(const uint32_t& session_id,
                               const bool& use_null_pin,
                               const string& optional_user_pin,
                               ::DBus::Error& /*error*/) {
  const string* tmp_pin = use_null_pin ? NULL : &optional_user_pin;
  return service_->InitPIN(session_id, tmp_pin);
}

uint32_t ChapsAdaptor::SetPIN(const uint32_t& session_id,
                              const bool& use_null_old_pin,
                              const string& optional_old_pin,
                              const bool& use_null_new_pin,
                              const string& optional_new_pin,
                              ::DBus::Error& /*error*/) {
  const string* tmp_old_pin = use_null_old_pin ? NULL : &optional_old_pin;
  const string* tmp_new_pin = use_null_new_pin ? NULL : &optional_new_pin;
  return service_->SetPIN(session_id, tmp_old_pin, tmp_new_pin);
}

void ChapsAdaptor::OpenSession(const uint32_t& slot_id, const uint32_t& flags,
                               uint32_t& session_id,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  result = service_->OpenSession(slot_id, flags, &session_id);
}

uint32_t ChapsAdaptor::CloseSession(const uint32_t& session_id,
                                    ::DBus::Error& /*error*/) {
  return service_->CloseSession(session_id);
}

uint32_t ChapsAdaptor::CloseAllSessions(const uint32_t& slot_id,
                                        ::DBus::Error& /*error*/) {
  return service_->CloseAllSessions(slot_id);
}

void ChapsAdaptor::GetSessionInfo(const uint32_t& session_id,
                                  uint32_t& slot_id,
                                  uint32_t& state,
                                  uint32_t& flags,
                                  uint32_t& device_error,
                                  uint32_t& result,
                                  ::DBus::Error& /*error*/) {
  result = service_->GetSessionInfo(session_id,
                                    &slot_id,
                                    &state,
                                    &flags,
                                    &device_error);
}

void ChapsAdaptor::GetOperationState(const uint32_t& session_id,
                                     vector<uint8_t>& operation_state,
                                     uint32_t& result,
                                     ::DBus::Error& /*error*/) {
  result = service_->GetOperationState(session_id, &operation_state);
}

uint32_t ChapsAdaptor::SetOperationState(
      const uint32_t& session_id,
      const vector<uint8_t>& operation_state,
      const uint32_t& encryption_key_handle,
      const uint32_t& authentication_key_handle,
      ::DBus::Error& /*error*/) {
  return service_->SetOperationState(session_id,
                                     operation_state,
                                     encryption_key_handle,
                                     authentication_key_handle);
}

uint32_t ChapsAdaptor::Login(const uint32_t& session_id,
                         const uint32_t& user_type,
                         const bool& use_null_pin,
                         const string& optional_pin,
                         ::DBus::Error& /*error*/) {
  const string* pin = use_null_pin ? NULL : &optional_pin;
  return service_->Login(session_id, user_type, pin);
}

uint32_t ChapsAdaptor::Logout(const uint32_t& session_id,
                              ::DBus::Error& /*error*/) {
  return service_->Logout(session_id);
}

void ChapsAdaptor::CreateObject(
      const uint32_t& session_id,
      const vector<uint8_t>& attributes,
      uint32_t& new_object_handle,
      uint32_t& result,
      ::DBus::Error& /*error*/) {
  result = service_->CreateObject(session_id,
                                  attributes,
                                  &new_object_handle);
}

void ChapsAdaptor::CopyObject(
      const uint32_t& session_id,
      const uint32_t& object_handle,
      const vector<uint8_t>& attributes,
      uint32_t& new_object_handle,
      uint32_t& result,
      ::DBus::Error& /*error*/) {
  result = service_->CopyObject(session_id,
                                object_handle,
                                attributes,
                                &new_object_handle);
}

uint32_t ChapsAdaptor::DestroyObject(const uint32_t& session_id,
                                 const uint32_t& object_handle,
                                 ::DBus::Error& /*error*/) {
  return service_->DestroyObject(session_id, object_handle);
}

void ChapsAdaptor::GetObjectSize(const uint32_t& session_id,
                           const uint32_t& object_handle,
                           uint32_t& object_size,
                           uint32_t& result,
                           ::DBus::Error& /*error*/) {
  result = service_->GetObjectSize(session_id, object_handle, &object_size);
}

void ChapsAdaptor::GetAttributeValue(const uint32_t& session_id,
                               const uint32_t& object_handle,
                               const std::vector<uint8_t>& attributes_in,
                               std::vector<uint8_t>& attributes_out,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  result = service_->GetAttributeValue(session_id,
                                       object_handle,
                                       attributes_in,
                                       &attributes_out);
}

uint32_t ChapsAdaptor::SetAttributeValue(const uint32_t& session_id,
                                   const uint32_t& object_handle,
                                   const std::vector<uint8_t>& attributes,
                                   ::DBus::Error& /*error*/) {
  return service_->SetAttributeValue(session_id,
                                     object_handle,
                                     attributes);
}

uint32_t ChapsAdaptor::FindObjectsInit(const uint32_t& session_id,
                                       const std::vector<uint8_t>& attributes,
                                       ::DBus::Error& /*error*/) {
  return service_->FindObjectsInit(session_id, attributes);
}

void ChapsAdaptor::FindObjects(const uint32_t& session_id,
                               const uint32_t& max_object_count,
                               std::vector<uint32_t>& object_list,
                               uint32_t& result,
                               ::DBus::Error& /*error*/) {
  result = service_->FindObjects(session_id, max_object_count, &object_list);
}

uint32_t ChapsAdaptor::FindObjectsFinal(const uint32_t& session_id,
                                        ::DBus::Error& /*error*/) {
  return service_->FindObjectsFinal(session_id);
}

uint32_t ChapsAdaptor::EncryptInit(
    const uint32_t& session_id,
    const uint32_t& mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    const uint32_t& key_handle,
    ::DBus::Error& /*error*/) {
  return service_->EncryptInit(session_id,
                               mechanism_type,
                               mechanism_parameter,
                               key_handle);
}

void ChapsAdaptor::Encrypt(const uint32_t& session_id,
                           const std::vector<uint8_t>& data_in,
                           const uint32_t& max_out_length,
                           uint32_t& actual_out_length,
                           std::vector<uint8_t>& data_out,
                           uint32_t& result,
                           ::DBus::Error& /*error*/) {
  result = service_->Encrypt(session_id,
                             data_in,
                             max_out_length,
                             &actual_out_length,
                             &data_out);
}

void ChapsAdaptor::EncryptUpdate(const uint32_t& session_id,
                                 const std::vector< uint8_t >& data_in,
                                 const uint32_t& max_out_length,
                                 uint32_t& actual_out_length,
                                 std::vector<uint8_t>& data_out,
                                 uint32_t& result, ::DBus::Error& /*error*/) {
  result = service_->EncryptUpdate(session_id,
                                   data_in,
                                   max_out_length,
                                   &actual_out_length,
                                   &data_out);
}

void ChapsAdaptor::EncryptFinal(const uint32_t& session_id,
                                const uint32_t& max_out_length,
                                uint32_t& actual_out_length,
                                std::vector<uint8_t>& data_out,
                                uint32_t& result,
                                ::DBus::Error& /*error*/) {
  result = service_->EncryptFinal(session_id,
                                  max_out_length,
                                  &actual_out_length,
                                  &data_out);
}

uint32_t ChapsAdaptor::DecryptInit(
    const uint32_t& session_id,
    const uint32_t& mechanism_type,
    const std::vector<uint8_t>& mechanism_parameter,
    const uint32_t& key_handle,
    ::DBus::Error& /*error*/) {
  return service_->DecryptInit(session_id,
                               mechanism_type,
                               mechanism_parameter,
                               key_handle);
}

void ChapsAdaptor::Decrypt(const uint32_t& session_id,
                           const std::vector< uint8_t >& data_in,
                           const uint32_t& max_out_length,
                           uint32_t& actual_out_length,
                           std::vector<uint8_t>& data_out,
                           uint32_t& result,
                           ::DBus::Error& /*error*/) {
  result = service_->Decrypt(session_id,
                             data_in,
                             max_out_length,
                             &actual_out_length,
                             &data_out);
}

void ChapsAdaptor::DecryptUpdate(const uint32_t& session_id,
                                 const std::vector<uint8_t>& data_in,
                                 const uint32_t& max_out_length,
                                 uint32_t& actual_out_length,
                                 std::vector<uint8_t>& data_out,
                                 uint32_t& result,
                                 ::DBus::Error& /*error*/) {
  result = service_->DecryptUpdate(session_id,
                                   data_in,
                                   max_out_length,
                                   &actual_out_length,
                                   &data_out);
}

void ChapsAdaptor::DecryptFinal(const uint32_t& session_id,
                                const uint32_t& max_out_length,
                                uint32_t& actual_out_length,
                                std::vector<uint8_t>& data_out,
                                uint32_t& result,
                                ::DBus::Error& /*error*/) {
  result = service_->DecryptFinal(session_id,
                                  max_out_length,
                                  &actual_out_length,
                                  &data_out);
}

}  // namespace
