// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <glib.h>

#include <base/callback_old.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/device_info.h"

namespace shill {
using ::testing::Test;
using ::testing::_;

class DeviceInfoTest : public Test {
 public:
  DeviceInfoTest()
    : device_info_(&dispatcher_),
      factory_(this) {
  }
  int DeviceInfoFlags() { return device_info_.request_flags_; }
protected:
  DeviceInfo device_info_;
  EventDispatcher dispatcher_;
  ScopedRunnableMethodFactory<DeviceInfoTest> factory_;
};

TEST_F(DeviceInfoTest, DeviceEnumeration) {
  // TODO(cmasone): Make this unit test use message loop primitives.

  // Start our own private device_info and make sure request flags clear
  device_info_.Start();
  EXPECT_NE(DeviceInfoFlags(), 0);

  // Crank the glib main loop a few times
  for (int main_loop_count = 0;
       main_loop_count < 6 && DeviceInfoFlags() != 0;
       ++main_loop_count)
    g_main_context_iteration(NULL, TRUE);

  EXPECT_EQ(DeviceInfoFlags(), 0);
}

}  // namespace shill
