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

}  // namespace
