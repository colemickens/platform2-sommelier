// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/technology.h"

#include <string>

#include <chromeos/dbus/service_constants.h>

namespace shill {

using std::string;

const char Technology::kUnknownName[] = "Unknown";


Technology::Identifier Technology::IdentifierFromName(const std::string &name) {
  if (name == flimflam::kTypeEthernet) {
    return kEthernet;
  } else if (name == flimflam::kTypeWifi) {
    return kWifi;
  } else if (name == flimflam::kTypeCellular) {
    return kCellular;
  } else {
    return kUnknown;
  }
}

std::string Technology::NameFromIdentifier(Technology::Identifier id) {
  if (id == kEthernet) {
    return flimflam::kTypeEthernet;
  } else if (id == kWifi) {
    return flimflam::kTypeWifi;
  } else if (id == kCellular) {
    return flimflam::kTypeCellular;
  } else {
    return kUnknownName;
  }
}

}  // namespace shill
