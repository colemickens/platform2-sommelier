// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_TECHNOLOGY_
#define SHILL_TECHNOLOGY_

#include <string>
#include <vector>

namespace shill {

class Error;

class Technology {
 public:
  enum Identifier {
    kEthernet,
    kWifi,
    kWiFiMonitor,
    kCellular,
    kVPN,
    kBlacklisted,
    kUnknown,
  };

  static Identifier IdentifierFromName(const std::string &name);
  static std::string NameFromIdentifier(Identifier id);
  static Identifier IdentifierFromStorageGroup(const std::string &group);

  // Convert the comma-separated list of technology names in
  // |technologies_string| into a vector of technology identifiers output in
  // |technologies_vector|.  Returns true if the |technologies_string| contains
  // a valid set of technologies with no duplicate elements, false otherwise.
  static bool GetTechnologyVectorFromString(
      const std::string &technologies_string,
      std::vector<Identifier> *technologies_vector,
      Error *error);

 private:
  static const char kUnknownName[];
};

}  // namespace shill

#endif  // SHILL_TECHNOLOGY_
