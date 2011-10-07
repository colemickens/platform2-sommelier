// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_adaptor.h"

#include "chaps/chaps.h"
#include "chaps/chaps_interface.h"

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
                               ::DBus::Error &error) {
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
                               ::DBus::Error &error) {
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
                                std::string& label,
                                std::string& manufacturer_id,
                                std::string& model,
                                std::string& serial_number,
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
                                ::DBus::Error &error) {
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
                                    std::vector<uint32_t>& mechanism_list,
                                    uint32_t& result,
                                    ::DBus::Error &error) {
  result = service_->GetMechanismList(slot_id, &mechanism_list);
}

void ChapsAdaptor::GetMechanismInfo(const uint32_t& slot_id,
                                    const uint32_t& mechanism_type,
                                    uint32_t& min_key_size,
                                    uint32_t& max_key_size,
                                    uint32_t& flags,
                                    uint32_t& result,
                                    ::DBus::Error &error) {
  result = service_->GetMechanismInfo(slot_id,
                                      mechanism_type,
                                      &min_key_size,
                                      &max_key_size,
                                      &flags);
}

uint32_t ChapsAdaptor::InitToken(const uint32_t& slot_id,
                                 const bool& use_null_pin,
                                 const std::string& optional_so_pin,
                                 const std::string& new_token_label,
                                 ::DBus::Error &error) {
  const string* tmp_pin = use_null_pin ? NULL : &optional_so_pin;
  return service_->InitToken(slot_id, tmp_pin, new_token_label);
}

uint32_t ChapsAdaptor::InitPIN(const uint32_t& session_id,
                               const bool& use_null_pin,
                               const std::string& optional_user_pin,
                               ::DBus::Error &error) {
  const string* tmp_pin = use_null_pin ? NULL : &optional_user_pin;
  return service_->InitPIN(session_id, tmp_pin);
}

uint32_t ChapsAdaptor::SetPIN(const uint32_t& session_id,
                              const bool& use_null_old_pin,
                              const std::string& optional_old_pin,
                              const bool& use_null_new_pin,
                              const std::string& optional_new_pin,
                              ::DBus::Error &error) {
  const string* tmp_old_pin = use_null_old_pin ? NULL : &optional_old_pin;
  const string* tmp_new_pin = use_null_new_pin ? NULL : &optional_new_pin;
  return service_->SetPIN(session_id, tmp_old_pin, tmp_new_pin);
}

void ChapsAdaptor::OpenSession(const uint32_t& slot_id, const uint32_t& flags,
                               uint32_t& session_id,
                               uint32_t& result,
                               ::DBus::Error &error) {
  result = service_->OpenSession(slot_id, flags, &session_id);
}

uint32_t ChapsAdaptor::CloseSession(const uint32_t& session_id,
                                    ::DBus::Error &error) {
  return service_->CloseSession(session_id);
}

uint32_t ChapsAdaptor::CloseAllSessions(const uint32_t& slot_id,
                                        ::DBus::Error &error) {
  return service_->CloseAllSessions(slot_id);
}

void ChapsAdaptor::GetSessionInfo(const uint32_t& session_id,
                                  uint32_t& slot_id,
                                  uint32_t& state,
                                  uint32_t& flags,
                                  uint32_t& device_error,
                                  uint32_t& result,
                                  ::DBus::Error &error) {
  result = service_->GetSessionInfo(session_id,
                                    &slot_id,
                                    &state,
                                    &flags,
                                    &device_error);
}

void ChapsAdaptor::GetOperationState(const uint32_t& session_id,
                                     std::vector<uint8_t>& operation_state,
                                     uint32_t& result,
                                     ::DBus::Error &error) {
  result = service_->GetOperationState(session_id, &operation_state);
}

uint32_t ChapsAdaptor::SetOperationState(
      const uint32_t& session_id,
      const std::vector<uint8_t>& operation_state,
      const uint32_t& encryption_key_handle,
      const uint32_t& authentication_key_handle,
      ::DBus::Error &error) {
  return service_->SetOperationState(session_id,
                                     operation_state,
                                     encryption_key_handle,
                                     authentication_key_handle);
}

}  // namespace
