// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_dbus_adaptor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/property_store_test.h"

namespace shill {

class ChromeosDBusAdaptorTest : public PropertyStoreTest {
 public:
  ChromeosDBusAdaptorTest() = default;

  ~ChromeosDBusAdaptorTest() override = default;
};

TEST_F(ChromeosDBusAdaptorTest, SanitizePathElement) {
  EXPECT_EQ("0Ab_y_Z_9_",
            ChromeosDBusAdaptor::SanitizePathElement("0Ab/y:Z`9{"));
  EXPECT_EQ("aB_f_0_Y_z",
            ChromeosDBusAdaptor::SanitizePathElement("aB-f/0@Y[z"));
}

}  // namespace shill
