// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/technology.h"

#include <set>
#include <string>
#include <vector>

#include <base/stl_util.h>
#include <base/string_split.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"

namespace shill {

using std::set;
using std::string;
using std::vector;

const char Technology::kTunnelName[] = "Tunnel";
const char Technology::kUnknownName[] = "Unknown";

// static
Technology::Identifier Technology::IdentifierFromName(const string &name) {
  if (name == flimflam::kTypeEthernet) {
    return kEthernet;
  } else if (name == flimflam::kTypeWifi) {
    return kWifi;
  } else if (name == flimflam::kTypeCellular) {
    return kCellular;
  } else if (name == flimflam::kTypeVPN) {
    return kVPN;
  } else if (name == kTunnelName) {
    return kTunnel;
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
  } else if (id == kVPN) {
    return flimflam::kTypeVPN;
  } else if (id == kTunnel) {
    return kTunnelName;
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

// static
bool Technology::GetTechnologyVectorFromString(
    const string &technologies_string,
    vector<Identifier> *technologies_vector,
    Error *error) {
  vector<string> technology_parts;
  base::SplitString(technologies_string, ',', &technology_parts);
  set<Technology::Identifier> seen;

  for (vector<string>::iterator it = technology_parts.begin();
       it != technology_parts.end();
       ++it) {
    Technology::Identifier identifier = Technology::IdentifierFromName(*it);

    if (identifier == Technology::kUnknown) {
      Error::PopulateAndLog(error, Error::kInvalidArguments,
                            *it + " is an unknown technology name");
      return false;
    }

    if (ContainsKey(seen, identifier)) {
      Error::PopulateAndLog(error, Error::kInvalidArguments,
                            *it + " is duplicated in the list");
      return false;
    }
    seen.insert(identifier);
    technologies_vector->push_back(identifier);
  }

  return true;
}

}  // namespace shill
