// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/key_value_store.h"
#include "shill/profile.h"
#include "shill/service.h"

namespace shill {

class ProviderTest : public testing::Test {
 protected:
  Provider provider_;
};

TEST_F(ProviderTest, Stubs) {
  // Expect that we do not crash.
  provider_.CreateServicesFromProfile(ProfileRefPtr());

  KeyValueStore args;

  {
     Error error;
     EXPECT_EQ(NULL, provider_.FindSimilarService(args, &error).get());
     EXPECT_EQ(Error::kInternalError, error.type());
  }
  {
     Error error;
     EXPECT_EQ(NULL, provider_.GetService(args, &error).get());
     EXPECT_EQ(Error::kInternalError, error.type());
  }
}

}  // namespace shill
