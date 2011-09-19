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

}  // namespace
