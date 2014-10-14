// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/manager.h"

#include <gtest/gtest.h>

namespace apmanager {

class ManagerTest : public testing::Test {
 public:
  ManagerTest() {}

 protected:
  Manager manager_;
};

TEST_F(ManagerTest, CreateService) {
}

TEST_F(ManagerTest, RemoveService) {
}

}  // namespace apmanager
