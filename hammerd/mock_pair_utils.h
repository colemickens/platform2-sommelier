// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HAMMERD_MOCK_PAIR_UTILS_H_
#define HAMMERD_MOCK_PAIR_UTILS_H_

#include <gmock/gmock.h>

#include "hammerd/pair_utils.h"

namespace hammerd {

// Mock internal method GenerateChallenge() to test PairManager itself.
class MockPairManager : public PairManager {
 public:
  MockPairManager() = default;

  MOCK_METHOD2(GenerateChallenge,
               void(PairChallengeRequest* request, uint8_t* private_key));
};

// Mock public method PairChallenge() used to inject into HammerUpdater.
class MockPairManagerInterface : public PairManagerInterface {
 public:
  MockPairManagerInterface() = default;
  MOCK_METHOD2(PairChallenge,
               ChallengeStatus(FirmwareUpdaterInterface* fw_updater,
                               DBusWrapperInterface* dbus_wrapper));
};

}  // namespace hammerd
#endif  // HAMMERD_MOCK_PAIR_UTILS_H_
