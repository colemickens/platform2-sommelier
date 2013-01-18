// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_provider.h"

#include <gtest/gtest.h>

#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/nice_mock_control.h"

namespace shill {

class WiFiProviderTest : public testing::Test {
 public:
  WiFiProviderTest()
      : manager_(&control_, NULL, &metrics_, NULL),
        provider_(&control_, NULL, &metrics_, &manager_) {}

  virtual ~WiFiProviderTest() {}

 protected:
  NiceMockControl control_;
  MockMetrics metrics_;
  MockManager manager_;
  WiFiProvider provider_;
};

TEST_F(WiFiProviderTest, Constructor) {
}

}  // namespace shill
