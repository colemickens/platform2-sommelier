// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CALLBACK80211_OBJECT_
#define SHILL_MOCK_CALLBACK80211_OBJECT_

#include <gmock/gmock.h>

#include "shill/callback80211_object.h"

namespace shill {

class UserBoundNlMessage;

class MockCallback80211 : public Callback80211Object {
 public:
  MockCallback80211() {}
  MOCK_METHOD1(Config80211MessageCallback, void(const UserBoundNlMessage &msg));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallback80211);
};

}  // namespace shill

#endif  // SHILL_MOCK_CALLBACK80211_OBJECT_
