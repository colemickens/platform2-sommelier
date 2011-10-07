// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_service.h"

#include <map>
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
#include "shill/mock_wifi.h"
#include "shill/property_store_unittest.h"
#include "shill/shill_event.h"
#include "shill/wpa_supplicant.h"

using std::map;
using std::string;
using std::vector;

namespace shill {
using ::testing::NiceMock;

class WiFiServiceTest : public PropertyStoreTest {
 public:
  WiFiServiceTest() : wifi_(
      new NiceMock<MockWiFi>(
          control_interface(),
          dispatcher(),
          manager(),
          "wifi",
          fake_mac,
          0)) {}
  virtual ~WiFiServiceTest() {}

 protected:
  static const char fake_mac[];
  scoped_refptr<MockWiFi> wifi() { return wifi_; }

 private:
  scoped_refptr<MockWiFi> wifi_;
};

// static
const char WiFiServiceTest::fake_mac[] = "AaBBcCDDeeFF";

MATCHER(WPASecurityArgs, "") {
  return ContainsKey(arg, wpa_supplicant::kPropertySecurityProtocol) &&
      ContainsKey(arg, wpa_supplicant::kPropertyPreSharedKey);
}

TEST_F(WiFiServiceTest, StorageId) {
  vector<uint8_t> ssid(5, 0);
  ssid.push_back(0xff);

  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   "none");
  string id = wifi_service->GetStorageIdentifier();
  for (uint i = 0; i < id.length(); ++i) {
    EXPECT_TRUE(id[i] == '_' ||
                isxdigit(id[i]) ||
                (isalpha(id[i]) && islower(id[i])));
  }
  EXPECT_TRUE(wifi_service->TechnologyIs(Technology::kWifi));
  size_t mac_pos = id.find(StringToLowerASCII(string(fake_mac)));
  EXPECT_NE(mac_pos, string::npos);
  EXPECT_NE(id.find(string(flimflam::kModeManaged), mac_pos), string::npos);
}

TEST_F(WiFiServiceTest, NonUTF8SSID) {
  vector<uint8_t> ssid;

  ssid.push_back(0xff);  // not a valid UTF-8 byte-sequence
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   "none");
  map<string, ::DBus::Variant> properties;
  // if service doesn't propertly sanitize SSID, this will generate SIGABRT.
  DBusAdaptor::GetProperties(wifi_service->store(), &properties, NULL);
}

TEST_F(WiFiServiceTest, ConnectTaskWPA) {
  vector<uint8_t> ssid(5, 0);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityWpa);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WPASecurityArgs()));
  wifi_service->ConnectTask();
}

TEST_F(WiFiServiceTest, ConnectTaskRSN) {
  vector<uint8_t> ssid(5, 0);
  WiFiServiceRefPtr wifi_service = new WiFiService(control_interface(),
                                                   dispatcher(),
                                                   manager(),
                                                   wifi(),
                                                   ssid,
                                                   flimflam::kModeManaged,
                                                   flimflam::kSecurityRsn);
  EXPECT_CALL(*wifi(),
              ConnectTo(wifi_service.get(), WPASecurityArgs()));
  wifi_service->ConnectTask();
}

}  // namespace shill
