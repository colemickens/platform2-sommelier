// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for Service

#include "service.h"

#include <gtest/gtest.h>

#include "mock_mount.h"

namespace cryptohome {
using ::testing::Return;
using ::testing::_;

TEST(ServiceInterfaceTests, CheckKeySuccessTest) {
  MockMount mount;
  EXPECT_CALL(mount, Init())
      .WillOnce(Return(true));
  EXPECT_CALL(mount, TestCredentials(_))
      .WillOnce(Return(true));

  Service service;
  service.set_mount(&mount);
  service.set_initialize_tpm(false);
  service.Initialize();
  gboolean out = FALSE;
  GError *error = NULL;

  char user[] = "chromeos-user";
  char key[] = "274146c6e8886a843ddfea373e2dc71b";
  EXPECT_EQ(TRUE, service.CheckKey(user, key, &out, &error));
  EXPECT_EQ(TRUE, out);
}

// TODO(wad) setup test fixture to create a temp dir
//           touch files on Mount/Unmount/IsMounted and
//           check for goodness.

}  // namespace cryptohome
