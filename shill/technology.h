// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_TECHNOLOGY_
#define SHILL_TECHNOLOGY_

#include <string>

namespace shill {

class Technology {
 public:
  enum Identifier {
    kEthernet,
    kWifi,
    kWiFiMonitor,
    kCellular,
    kBlacklisted,
    kUnknown,
  };

  static Identifier IdentifierFromName(const std::string &name);
  static std::string NameFromIdentifier(Identifier id);

 private:
  static const char kUnknownName[];
};

}  // namespace shill

#endif  // SHILL_TECHNOLOGY_
