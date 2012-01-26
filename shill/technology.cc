// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/technology.h"

#include <string>
#include <vector>

#include <base/string_split.h>
#include <chromeos/dbus/service_constants.h>

namespace shill {

using std::string;
using std::vector;

const char Technology::kUnknownName[] = "Unknown";

// static
Technology::Identifier Technology::IdentifierFromName(const string &name) {
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

// static
string Technology::NameFromIdentifier(Technology::Identifier id) {
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

// static
Technology::Identifier Technology::IdentifierFromStorageGroup(
    const string &group) {
  vector<string> group_parts;
  base::SplitString(group, '_', &group_parts);
  if (group_parts.empty()) {
    return kUnknown;
  }
  return IdentifierFromName(group_parts[0]);
}

}  // namespace shill
