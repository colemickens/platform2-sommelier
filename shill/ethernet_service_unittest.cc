// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet_service.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_ethernet.h"
#include "shill/property_store_unittest.h"
#include "shill/refptr_types.h"

using ::testing::NiceMock;

namespace shill {

class EthernetServiceTest : public PropertyStoreTest {
 public:
  EthernetServiceTest()
      : ethernet_(
            new NiceMock<MockEthernet>(control_interface(),
                                       dispatcher(),
                                       metrics(),
                                       manager(),
                                       "ethernet",
                                       fake_mac,
                                       0)),
        service_(
            new EthernetService(control_interface(),
                                dispatcher(),
                                metrics(),
                                manager(),
                                ethernet_)) {}
  virtual ~EthernetServiceTest() {}

 protected:
  static const char fake_mac[];

  bool GetAutoConnect() {
    return service_->GetAutoConnect(NULL);
  }

  void SetAutoConnect(const bool connect, Error *error) {
    return service_->SetAutoConnect(connect, error);
  }

  scoped_refptr<MockEthernet> ethernet_;
  EthernetServiceRefPtr service_;
};

// static
const char EthernetServiceTest::fake_mac[] = "AaBBcCDDeeFF";

TEST_F(EthernetServiceTest, AutoConnect) {
  EXPECT_TRUE(service_->IsAutoConnectByDefault());
  EXPECT_TRUE(GetAutoConnect());
  {
    Error error;
    SetAutoConnect(false, &error);
    EXPECT_FALSE(error.IsSuccess());
  }
  EXPECT_TRUE(GetAutoConnect());
  {
    Error error;
    SetAutoConnect(true, &error);
    EXPECT_TRUE(error.IsSuccess());
  }
  EXPECT_TRUE(GetAutoConnect());
}

}  // namespace shill
