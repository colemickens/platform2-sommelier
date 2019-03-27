// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet/ethernet_temporary_service.h"

#include <string>

#include "shill/control_interface.h"
#include "shill/manager.h"

using std::string;

namespace shill {

EthernetTemporaryService::EthernetTemporaryService(
    Manager* manager, const string& storage_identifier)
    : Service(manager, Technology::kEthernet),
      storage_identifier_(storage_identifier) {
  set_friendly_name("Ethernet");
}

EthernetTemporaryService::~EthernetTemporaryService() = default;

std::string EthernetTemporaryService::GetDeviceRpcId(Error* /*error*/) const {
  return control_interface()->NullRPCIdentifier();
}

string EthernetTemporaryService::GetStorageIdentifier() const {
  return storage_identifier_;
}

bool EthernetTemporaryService::IsVisible() const {
  return false;
}

}  // namespace shill
