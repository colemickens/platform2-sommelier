// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/technologies.h"

namespace peerd {
namespace technologies {

const char kMDNSText[] = "mDNS";
const char kBTText[] = "BT";
const char kBTLEText[] = "BT_LE";

bool add_to(const std::string& text, TechnologySet* tech) {
  if (text == kMDNSText) {
    tech->set(kMDNS);
    return true;
  }
  if (text == kBTText) {
    tech->set(kBT);
    return true;
  }
  if (text == kBTLEText) {
    tech->set(kBTLE);
    return true;
  }
  return false;
}

std::vector<std::string> techs2strings(const TechnologySet& tech) {
  std::vector<std::string> result;
  if (tech.test(kMDNS)) {
    result.push_back(kMDNSText);
  }
  if (tech.test(kBT)) {
    result.push_back(kBTText);
  }
  if (tech.test(kBTLE)) {
    result.push_back(kBTLEText);
  }
  return result;
}

}  // namespace technologies
}  // namespace peerd
