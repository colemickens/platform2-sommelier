// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/gdm_device.h"

#include <gtest/gtest.h>

#include <chromeos/dbus/service_constants.h>

using base::DictionaryValue;
using std::string;

namespace wimax_manager {

class GdmDeviceTest : public testing::Test {
};

TEST_F(GdmDeviceTest, ConstructEAPParameters) {
  DictionaryValue connect_parameters;
  GCT_API_EAP_PARAM eap_parameters;
  connect_parameters.SetString(kEAPAnonymousIdentity, "anon");
  connect_parameters.SetString(kEAPUserIdentity, "user@test");
  connect_parameters.SetString(kEAPUserPassword, "testpass");
  EXPECT_TRUE(GdmDevice::ConstructEAPParameters(connect_parameters,
                                                &eap_parameters));
  EXPECT_STREQ("anon", reinterpret_cast<char *>(eap_parameters.anonymousId));
  EXPECT_STREQ("user@test", reinterpret_cast<char *>(eap_parameters.userId));
  EXPECT_STREQ("testpass", reinterpret_cast<char *>(eap_parameters.userIdPwd));
}

TEST_F(GdmDeviceTest, ConstructEAPParametersWithInvalidEAPParameters) {
  {
    // Non-string anonymous identity.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetInteger(kEAPAnonymousIdentity, 1);
    connect_parameters.SetString(kEAPUserIdentity, "user@test");
    connect_parameters.SetString(kEAPUserPassword, "testpass");
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(connect_parameters,
                                                   &eap_parameters));
  }
  {
    // Non-string user identity.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetString(kEAPAnonymousIdentity, "anon");
    connect_parameters.SetInteger(kEAPUserIdentity, 1);
    connect_parameters.SetString(kEAPUserPassword, "testpass");
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(connect_parameters,
                                                   &eap_parameters));
  }
  {
    // Non-string user password.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetString(kEAPAnonymousIdentity, "anon");
    connect_parameters.SetString(kEAPUserIdentity, "user@test");
    connect_parameters.SetInteger(kEAPUserPassword, 1);
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(connect_parameters,
                                                   &eap_parameters));
  }
  {
    // Anonymous identity too long.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetString(
        kEAPAnonymousIdentity, string(sizeof(eap_parameters.anonymousId), 'a'));
    connect_parameters.SetString(kEAPUserIdentity, "user@test");
    connect_parameters.SetString(kEAPUserPassword, "testpass");
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(connect_parameters,
                                                   &eap_parameters));
  }
  {
    // User identity too long.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetString(kEAPAnonymousIdentity, "anon");
    connect_parameters.SetString(
        kEAPUserIdentity, string(sizeof(eap_parameters.userId), 'a'));
    connect_parameters.SetString(kEAPUserPassword, "testpass");
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(connect_parameters,
                                                   &eap_parameters));
  }
  {
    // User password too long.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetString(kEAPAnonymousIdentity, "anon");
    connect_parameters.SetString(kEAPUserIdentity, "user@test");
    connect_parameters.SetString(
        kEAPUserPassword, string(sizeof(eap_parameters.userIdPwd), 'a'));
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(connect_parameters,
                                                   &eap_parameters));
  }
}

TEST_F(GdmDeviceTest, ConstructEAPParametersWithoutAnonymousIdentity) {
  DictionaryValue connect_parameters;
  GCT_API_EAP_PARAM eap_parameters;
  connect_parameters.SetString(kEAPUserIdentity, "user@test");
  connect_parameters.SetString(kEAPUserPassword, "testpass");
  EXPECT_TRUE(GdmDevice::ConstructEAPParameters(connect_parameters,
                                                &eap_parameters));
  EXPECT_STREQ("RANDOM@test",
               reinterpret_cast<char *>(eap_parameters.anonymousId));
  EXPECT_STREQ("user@test", reinterpret_cast<char *>(eap_parameters.userId));
  EXPECT_STREQ("testpass", reinterpret_cast<char *>(eap_parameters.userIdPwd));
}

TEST_F(GdmDeviceTest, ConstructEAPParametersWithoutEAPParameters) {
  DictionaryValue connect_parameters;
  GCT_API_EAP_PARAM eap_parameters;
  EXPECT_TRUE(GdmDevice::ConstructEAPParameters(connect_parameters,
                                                &eap_parameters));
  EXPECT_STREQ("", reinterpret_cast<char *>(eap_parameters.anonymousId));
  EXPECT_STREQ("", reinterpret_cast<char *>(eap_parameters.userId));
  EXPECT_STREQ("", reinterpret_cast<char *>(eap_parameters.userIdPwd));
}

}  // namespace shill
