// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/macros.h>
#include <gtest/gtest.h>

#include "smbprovider/mount_tracker.h"

namespace smbprovider {

class MountTrackerTest : public testing::Test {
 public:
  MountTrackerTest() = default;
  ~MountTrackerTest() override = default;

  DISALLOW_COPY_AND_ASSIGN(MountTrackerTest);
};

}  // namespace smbprovider
