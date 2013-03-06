// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CONFIG80211_H_
#define SHILL_MOCK_CONFIG80211_H_

#include <gmock/gmock.h>

#include "shill/config80211.h"

namespace shill {

class MockConfig80211 : public Config80211 {
 public:
  MockConfig80211() {}
  ~MockConfig80211() {}
};

}  // namespace

#endif  // SHILL_MOCK_CONFIG80211_H_
