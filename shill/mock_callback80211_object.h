// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CALLBACK80211_OBJECT_
#define SHILL_MOCK_CALLBACK80211_OBJECT_

#include <gmock/gmock.h>

#include "shill/callback80211_object.h"


namespace shill {

class Config80211;
class UserBoundNlMessage;

class MockCallback80211 : public Callback80211Object {
 public:
  explicit MockCallback80211(Config80211 *config80211)
      : Callback80211Object(config80211) {}
  MOCK_METHOD1(Config80211MessageCallback, void(const UserBoundNlMessage &msg));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallback80211);
};

}  // namespace shill

#endif  // SHILL_MOCK_CALLBACK80211_OBJECT_
