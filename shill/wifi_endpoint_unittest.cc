// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_endpoint.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/stl_util-inl.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/wpa_supplicant.h"

using std::map;
using std::set;
using std::string;
using std::vector;

using ::testing::Test;

namespace shill {

class WiFiEndpointTest : public Test {
 public:
  WiFiEndpointTest() {}
  virtual ~WiFiEndpointTest() {}

 protected:
  vector<string> make_string_vector1(const string &str1) {
    vector<string> strvec;
    strvec.push_back(str1);
    return strvec;
  }

  vector<string> make_string_vector2(const string &str1, const string &str2) {
    vector<string> strvec;
    strvec.push_back(str1);
    strvec.push_back(str2);
    return strvec;
  }

  map<string, ::DBus::Variant> make_key_management_args(
      vector<string> key_management_method_strings) {
    map<string, ::DBus::Variant> args;
    ::DBus::MessageIter writer;
    writer =
        args[wpa_supplicant::kSecurityMethodPropertyKeyManagement].writer();
    writer << key_management_method_strings;
    return args;
  }

  map<string, ::DBus::Variant> make_security_args(
      const string &security_protocol,
      const string &key_management_method) {
    map<string, ::DBus::Variant> args;
    ::DBus::MessageIter writer;
    writer = args[security_protocol].writer();
    writer <<
        make_key_management_args(make_string_vector1(key_management_method));
    return args;
  }

  const char *ParseSecurity(
    const map<string, ::DBus::Variant> &properties) {
    return WiFiEndpoint::ParseSecurity(properties);
  }
};

TEST_F(WiFiEndpointTest, ParseKeyManagementMethodsEAP) {
  set<WiFiEndpoint::KeyManagement> parsed_methods;
  WiFiEndpoint::ParseKeyManagementMethods(
      make_key_management_args(make_string_vector1("something-eap")),
      &parsed_methods);
  EXPECT_TRUE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagement802_1x));
  EXPECT_FALSE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagementPSK));
}

TEST_F(WiFiEndpointTest, ParseKeyManagementMethodsPSK) {
  set<WiFiEndpoint::KeyManagement> parsed_methods;
  WiFiEndpoint::ParseKeyManagementMethods(
      make_key_management_args(make_string_vector1("something-psk")),
      &parsed_methods);
  EXPECT_TRUE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagementPSK));
  EXPECT_FALSE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagement802_1x));
}

TEST_F(WiFiEndpointTest, ParseKeyManagementMethodsEAPAndPSK) {
  set<WiFiEndpoint::KeyManagement> parsed_methods;
  WiFiEndpoint::ParseKeyManagementMethods(
      make_key_management_args(
          make_string_vector2("something-eap", "something-psk")),
      &parsed_methods);
  EXPECT_TRUE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagement802_1x));
  EXPECT_TRUE(
      ContainsKey(parsed_methods, WiFiEndpoint::kKeyManagementPSK));
}

TEST_F(WiFiEndpointTest, ParseSecurityRSN802_1x) {
  EXPECT_STREQ(flimflam::kSecurity8021x,
               ParseSecurity(make_security_args("RSN", "something-eap")));
}

TEST_F(WiFiEndpointTest, ParseSecurityWPA802_1x) {
  EXPECT_STREQ(flimflam::kSecurity8021x,
               ParseSecurity(make_security_args("WPA", "something-eap")));
}

TEST_F(WiFiEndpointTest, ParseSecurityRSNPSK) {
  EXPECT_STREQ(flimflam::kSecurityRsn,
               ParseSecurity(make_security_args("RSN", "something-psk")));
}

TEST_F(WiFiEndpointTest, ParseSecurityWPAPSK) {
  EXPECT_STREQ(flimflam::kSecurityWpa,
               ParseSecurity(make_security_args("WPA", "something-psk")));
}

TEST_F(WiFiEndpointTest, ParseSecurityWEP) {
  map<string, ::DBus::Variant> top_params;
  top_params[wpa_supplicant::kPropertyPrivacy].writer().append_bool(true);
  EXPECT_STREQ(flimflam::kSecurityWep, ParseSecurity(top_params));
}

TEST_F(WiFiEndpointTest, ParseSecurityNone) {
  map<string, ::DBus::Variant> top_params;
  EXPECT_STREQ(flimflam::kSecurityNone, ParseSecurity(top_params));
}

}  // namespace shill
