// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_TECHNOLOGY_H_
#define SHILL_TECHNOLOGY_H_

#include <string>
#include <vector>

namespace shill {

class Error;

// A class that provides functions for converting between technology names
// and identifiers.
class Technology {
 public:
  enum Identifier {
    kEthernet,
    kEthernetEap,
    kWifi,
    kWiFiMonitor,
    kWiMax,
    kCellular,
    kVPN,
    kTunnel,
    kBlacklisted,
    kLoopback,
    kCDCEthernet,      // Only for internal use in DeviceInfo.
    kVirtioEthernet,   // Only for internal use in DeviceInfo.
    kNoDeviceSymlink,  // Only for internal use in DeviceInfo.
    kPPP,
    kPPPoE,
    kArc,
    // Virtual tap devices used by guest OS and clients getting Internet via
    // Chrome OS host kernel.
    kGuestInterface,
    kUnknown,
  };

  // Returns the technology identifier for a technology name in |name|,
  // or Technology::kUnknown if the technology name is unknown.
  static Identifier IdentifierFromName(const std::string& name);

  // Returns the technology name for a technology identifier in |id|,
  // or Technology::kUnknownName ("Unknown") if the technology identifier
  // is unknown.
  static std::string NameFromIdentifier(Identifier id);

  // Returns the technology identifier for a storage group identifier in
  // |group|, which should have the format of <technology name>_<suffix>,
  // or Technology::kUnknown if |group| is not prefixed with a known
  // technology name.
  static Identifier IdentifierFromStorageGroup(const std::string& group);

  // Converts the comma-separated list of technology names (with no whitespace
  // around commas) in |technologies_string| into a vector of technology
  // identifiers output in |technologies_vector|. Returns true if the
  // |technologies_string| contains a valid set of technologies with no
  // duplicate elements, false otherwise.
  static bool GetTechnologyVectorFromString(
      const std::string& technologies_string,
      std::vector<Identifier>* technologies_vector,
      Error* error);

  // Returns true if |technology| is a primary connectivity technology, i.e.
  // Ethernet, Cellular, WiFi, WiMAX, or PPPoE.
  static bool IsPrimaryConnectivityTechnology(Identifier technology);
};

}  // namespace shill

#endif  // SHILL_TECHNOLOGY_H_
