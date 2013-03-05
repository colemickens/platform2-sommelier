// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CALLBACK80211_OBJECT_
#define SHILL_MOCK_CALLBACK80211_OBJECT_

#include <gmock/gmock.h>

#include "shill/callback80211_object.h"

namespace shill {

class Nl80211Message;

class MockHandler80211 : public Callback80211Object {
 public:
  MockHandler80211() {}
  MOCK_METHOD1(Config80211MessageHandler, void(const NetlinkMessage &msg));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHandler80211);
};

}  // namespace shill

#endif  // SHILL_MOCK_CALLBACK80211_OBJECT_
