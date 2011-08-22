// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_service.h"

#include <string>
#include <vector>

#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/manager.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"
#include "shill/mock_service.h"
#include "shill/mock_store.h"
#include "shill/property_store_unittest.h"
#include "shill/shill_event.h"
#include "shill/wifi.h"

using std::string;
using std::vector;

namespace shill {

class WiFiServiceTest : public PropertyStoreTest {
 public:
  WiFiServiceTest() {}
  virtual ~WiFiServiceTest() {}
};

TEST_F(WiFiServiceTest, StorageId) {
  vector<uint8_t> ssid(5, 0);
  ssid.push_back(0xff);

  WiFiServiceRefPtr wifi = new WiFiService(&control_interface_,
                                           &dispatcher_,
                                           &manager_,
                                           NULL,
                                           ssid,
                                           flimflam::kModeManaged,
                                           "none");
  static const char fake_mac[] = "AaBBcCDDeeFF";
  string id = wifi->GetStorageIdentifier(fake_mac);
  for (uint i = 0; i < id.length(); ++i) {
    EXPECT_TRUE(id[i] == '_' ||
                isxdigit(id[i]) ||
                (isalpha(id[i]) && islower(id[i])));
  }
  size_t mac_pos = id.find(StringToLowerASCII(string(fake_mac)));
  EXPECT_NE(mac_pos, string::npos);
  EXPECT_NE(id.find(string(flimflam::kModeManaged), mac_pos), string::npos);
}

}  // namespace shill
