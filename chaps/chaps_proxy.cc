// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_proxy.h"

#include <base/logging.h>

#include "pkcs11/cryptoki.h"

using std::string;
using std::vector;

namespace chaps {

ChapsProxyImpl::ChapsProxyImpl() {}

ChapsProxyImpl::~ChapsProxyImpl() {}

bool ChapsProxyImpl::Connect(const string& username) {
  bool success = false;
  try {
    if (proxy_ == NULL) {
      // Establish a D-Bus connection.
      DBus::Connection connection = DBus::Connection::SessionBus();
      proxy_.reset(new Proxy(connection, "/org/chromium/Chaps",
                             "org.chromium.Chaps"));
    }
    // Connect to our service via D-Bus.
    if (proxy_ != NULL)
      proxy_->Connect(username, session_id_, success);
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return success;
}

void ChapsProxyImpl::Disconnect() {
  try {
    if (proxy_ != NULL)
      proxy_->Disconnect(session_id_);
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
}

uint32_t ChapsProxyImpl::GetSlotList(const bool& token_present,
                                     vector<uint32_t>& slot_list) {
  uint32_t return_value = CKR_OK;
  if (proxy_ == NULL)
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  try {
    proxy_->GetSlotList(session_id_, token_present, slot_list, return_value);
  } catch (DBus::Error err) {
    return_value = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return return_value;
}

uint32_t ChapsProxyImpl::GetSlotInfo(const uint32_t& slot_id,
                                     string& slot_description,
                                     string& manufacturer_id,
                                     uint32_t& flags,
                                     uint8_t& hardware_version_major,
                                     uint8_t& hardware_version_minor,
                                     uint8_t& firmware_version_major,
                                     uint8_t& firmware_version_minor) {
  uint32_t return_value = CKR_OK;
  if (proxy_ == NULL)
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  try {
    proxy_->GetSlotInfo(session_id_,
                        slot_id,
                        slot_description,
                        manufacturer_id,
                        flags,
                        hardware_version_major,
                        hardware_version_minor,
                        firmware_version_major,
                        firmware_version_minor,
                        return_value);
  } catch (DBus::Error err) {
    return_value = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return return_value;
}

}  // namespace

