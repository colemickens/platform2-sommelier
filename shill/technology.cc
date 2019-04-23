// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/technology.h"

#include <set>
#include <string>
#include <vector>

#include <base/stl_util.h>
#include <base/strings/string_split.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/logging.h"

namespace shill {

using std::set;
using std::string;
using std::vector;

namespace {

constexpr char kLoopbackName[] = "loopback";
constexpr char kTunnelName[] = "tunnel";
constexpr char kPPPName[] = "ppp";
constexpr char kUnknownName[] = "unknown";

}  // namespace

// static
Technology::Identifier Technology::IdentifierFromName(const string& name) {
  if (name == kTypeEthernet) {
    return kEthernet;
  } else if (name == kTypeEthernetEap) {
    return kEthernetEap;
  } else if (name == kTypeWifi) {
    return kWifi;
  } else if (name == kTypeCellular) {
    return kCellular;
  } else if (name == kTypeVPN) {
    return kVPN;
  } else if (name == kTypePPPoE) {
    return kPPPoE;
  } else if (name == kLoopbackName) {
    return kLoopback;
  } else if (name == kTunnelName) {
    return kTunnel;
  } else if (name == kPPPName) {
    return kPPP;
  } else {
    return kUnknown;
  }
}

// static
string Technology::NameFromIdentifier(Technology::Identifier id) {
  if (id == kEthernet) {
    return kTypeEthernet;
  } else if (id == kEthernetEap) {
    return kTypeEthernetEap;
  } else if (id == kWifi) {
    return kTypeWifi;
  } else if (id == kCellular) {
    return kTypeCellular;
  } else if (id == kVPN) {
    return kTypeVPN;
  } else if (id == kLoopback) {
    return kLoopbackName;
  } else if (id == kTunnel) {
    return kTunnelName;
  } else if (id == kPPP) {
    return kPPPName;
  } else if (id == kPPPoE) {
    return kTypePPPoE;
  } else {
    return kUnknownName;
  }
}

// static
Technology::Identifier Technology::IdentifierFromStorageGroup(
    const string& group) {
  vector<string> group_parts = base::SplitString(
      group, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (group_parts.empty()) {
    return kUnknown;
  }
  return IdentifierFromName(group_parts[0]);
}

// static
bool Technology::GetTechnologyVectorFromString(
    const string& technologies_string,
    vector<Identifier>* technologies_vector,
    Error* error) {
  CHECK(technologies_vector);
  CHECK(error);

  vector<string> technology_parts;
  set<Technology::Identifier> seen;
  technologies_vector->clear();

  // Check if |technologies_string| is empty as some versions of
  // base::SplitString return a vector with one empty string when given an
  // empty string.
  if (!technologies_string.empty()) {
    technology_parts = base::SplitString(
        technologies_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }

  for (const auto& name : technology_parts) {
    Technology::Identifier identifier = Technology::IdentifierFromName(name);

    if (identifier == Technology::kUnknown) {
      Error::PopulateAndLog(FROM_HERE, error, Error::kInvalidArguments,
                            name + " is an unknown technology name");
      return false;
    }

    if (base::ContainsKey(seen, identifier)) {
      Error::PopulateAndLog(FROM_HERE, error, Error::kInvalidArguments,
                            name + " is duplicated in the list");
      return false;
    }
    seen.insert(identifier);
    technologies_vector->push_back(identifier);
  }

  return true;
}

// static
bool Technology::IsPrimaryConnectivityTechnology(Identifier technology) {
  return (technology == kCellular ||
          technology == kEthernet ||
          technology == kWifi ||
          technology == kPPPoE);
}

}  // namespace shill
