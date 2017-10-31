// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libpasswordprovider/password.h"

#include <gtest/gtest.h>

namespace password_provider {

// Basic memory allocation should succeed.
TEST(Password, CreatePasswordWithMemoryAllocation) {
  Password password;
  EXPECT_TRUE(password.Init());

  // Expect Password buffer size to be 1 page minus 1 byte reserved for the null
  // terminator
  size_t page_size = sysconf(_SC_PAGESIZE);
  EXPECT_EQ(page_size - 1, password.max_size());

  EXPECT_EQ(0, password.size());

  EXPECT_TRUE(password.GetRaw());
}

// Creating a Password object without memory allocation should do nothing.
TEST(Password, CreatePasswordWithNoMemoryAllocation) {
  Password password;
  EXPECT_EQ(0, password.size());
  EXPECT_EQ(0, password.max_size());
  // Should not segfault due to freeing memory not allocated.
}

}  // namespace password_provider
