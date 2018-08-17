// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/interface_disable_labels.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace portier {

namespace {

class MockInterface : public InterfaceDisableLabels {
 public:
  MockInterface() = default;
  ~MockInterface() override = default;

  MOCK_METHOD0(OnEnabled, void());
  MOCK_METHOD0(OnDisabled, void());
};

}  // namespace

TEST(InterfaceDisableLabels, AlwaysEnable) {
  MockInterface interface;

  EXPECT_CALL(interface, OnEnabled()).Times(3);

  // These call should call the callbacks.
  EXPECT_TRUE(interface.TryEnable());
  EXPECT_TRUE(interface.ClearSoftLabels(true));
  interface.ClearAllLabels(true);

  // These calls should not.
  EXPECT_CALL(interface, OnEnabled()).Times(0);
  EXPECT_FALSE(interface.ClearSoftLabels(false));
  interface.ClearAllLabels(false);
}

TEST(InterfaceDisableLabels, CauseDisable) {
  MockInterface interface;

  EXPECT_CALL(interface, OnEnabled()).Times(0);
  EXPECT_CALL(interface, OnDisabled()).Times(1);
  EXPECT_TRUE(interface.MarkLoopDetected());

  EXPECT_FALSE(interface.IsMarkedSoftwareDisabled());
  EXPECT_FALSE(interface.IsMarkedLinkDown());
  EXPECT_TRUE(interface.IsMarkedLoopDetected());
  EXPECT_FALSE(interface.IsMarkedGroupless());

  // Multiple labels should only result in one call.
  EXPECT_CALL(interface, OnDisabled()).Times(0);
  EXPECT_FALSE(interface.MarkSoftwareDisabled());
  EXPECT_FALSE(interface.MarkGroupless());
  EXPECT_FALSE(interface.MarkLinkDown());

  EXPECT_TRUE(interface.IsMarkedSoftwareDisabled());
  EXPECT_TRUE(interface.IsMarkedLoopDetected());
  EXPECT_TRUE(interface.IsMarkedLinkDown());
  EXPECT_TRUE(interface.IsMarkedGroupless());

  // Clear 1 soft reason and 1 hard reason, nothing should change.
  EXPECT_FALSE(interface.ClearLoopDetected());
  EXPECT_FALSE(interface.ClearGroupless());

  EXPECT_TRUE(interface.IsMarkedSoftwareDisabled());
  EXPECT_FALSE(interface.IsMarkedLoopDetected());
  EXPECT_TRUE(interface.IsMarkedLinkDown());
  EXPECT_FALSE(interface.IsMarkedGroupless());

  // Clearing only soft reasons.  Should not call callback.
  EXPECT_FALSE(interface.ClearSoftLabels(true));

  // Verify soft reasons are cleared.
  EXPECT_FALSE(interface.IsMarkedSoftwareDisabled());
  EXPECT_FALSE(interface.IsMarkedLoopDetected());
  // Hard reasons are not.
  EXPECT_TRUE(interface.IsMarkedLinkDown());
  EXPECT_FALSE(interface.IsMarkedGroupless());

  // Clear all.
  EXPECT_CALL(interface, OnEnabled()).Times(1);
  interface.ClearAllLabels(true);
}

}  // namespace portier
