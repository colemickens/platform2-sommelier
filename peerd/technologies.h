// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_TECHNOLOGIES_H_
#define PEERD_TECHNOLOGIES_H_

#include <bitset>
#include <limits>
#include <string>
#include <vector>

namespace peerd {
namespace technologies {

extern const char kMDNSText[];
extern const char kBTText[];
extern const char kBTLEText[];

enum Technology {
  kMDNS,
  kBT,
  kBTLE,
};

static const size_t kTechnologiesBitSize =
    std::numeric_limits<std::underlying_type<Technology>::type>::digits;
using TechnologySet = std::bitset<kTechnologiesBitSize>;

// Adds a technology listed above to the given |tech|.
bool add_to(const std::string& text, TechnologySet* tech);

// Maps from a set of technologies to their string representations.
std::vector<std::string> techs2strings(const TechnologySet& tech);

}  // namespace technologies
}  // namespace peerd

#endif  // PEERD_TECHNOLOGIES_H_
