// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/gdm_device.h"

#include <gtest/gtest.h>

#include <chromeos/dbus/service_constants.h>

#include "wimax_manager/proto_bindings/eap_parameters.pb.h"

using base::DictionaryValue;
using std::string;

namespace wimax_manager {

namespace {

const char kTestEAPAnonymousIdentity[] = "anon";
const char kTestEAPAnonymousIdentityWithRealmTag[] = "anon@${realm}";
const char kTestEAPAnonymousIdentityWithRealmTagReplaced[] = "anon@test";
const char kTestEAPUserIdentity[] = "user@test";
const char kTestEAPUserPassword[] = "testpass";

}  // namespace

class GdmDeviceTest : public testing::Test {
 protected:
  void ExpectEAPParameters(const GCT_API_EAP_PARAM& eap_parameters,
                           GCT_API_EAP_TYPE eap_type,
                           const char* anonymous_identity,
                           const char* user_identity,
                           const char* user_password,
                           int device_certificate_null,
                           int ca_certificate_null) {
    EXPECT_EQ(eap_type, eap_parameters.type);
    EXPECT_STREQ(anonymous_identity,
                 reinterpret_cast<const char*>(eap_parameters.anonymousId));
    EXPECT_STREQ(user_identity,
                 reinterpret_cast<const char*>(eap_parameters.userId));
    EXPECT_STREQ(user_password,
                 reinterpret_cast<const char*>(eap_parameters.userIdPwd));
    EXPECT_EQ(device_certificate_null, eap_parameters.devCertNULL);
    EXPECT_EQ(ca_certificate_null, eap_parameters.caCertNULL);
  }
};

TEST_F(GdmDeviceTest, ConstructEAPParametersUsingConnectParameters) {
  DictionaryValue connect_parameters;
  connect_parameters.SetString(kEAPAnonymousIdentity,
                               kTestEAPAnonymousIdentity);
  connect_parameters.SetString(kEAPUserIdentity, kTestEAPUserIdentity);
  connect_parameters.SetString(kEAPUserPassword, kTestEAPUserPassword);

  EAPParameters operator_eap_parameters;
  {
    // No network operator info provided.
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP,
                        kTestEAPAnonymousIdentity, kTestEAPUserIdentity,
                        kTestEAPUserPassword, 0, 0);
  }
  {
    // Network operator has an anonymous identity, user identity, and user
    // password specified.
    operator_eap_parameters.set_anonymous_identity("1");
    operator_eap_parameters.set_user_identity("2");
    operator_eap_parameters.set_user_password("3");
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP,
                        kTestEAPAnonymousIdentity, kTestEAPUserIdentity,
                        kTestEAPUserPassword, 0, 0);
  }
}

TEST_F(GdmDeviceTest, ConstructEAPParametersUsingOperatorEAPParameters) {
  // No parameter is specified through Connect().
  DictionaryValue connect_parameters;

  {
    // Default network operator EAP parameters.
    EAPParameters operator_eap_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP, "", "", "", 0, 0);
  }
  {
    // Network operator's EAP type is NONE.
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_type(EAP_TYPE_NONE);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP, "", "", "", 0, 0);
  }
  {
    // Network operator's EAP type is TLS.
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_type(EAP_TYPE_TLS);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_EAP_TLS, "", "", "", 0, 0);
  }
  {
    // Network operator's EAP type is TTLS/MD5.
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_type(EAP_TYPE_TTLS_MD5);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_EAP_TTLS_MD5, "", "", "", 0,
                        0);
  }
  {
    // Network operator's EAP type is TTLS/MSCHAPv2.
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_type(EAP_TYPE_TTLS_MSCHAPV2);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_EAP_TTLS_MSCHAPV2, "", "", "",
                        0, 0);
  }
  {
    // Network operator's EAP type is TTLS/CHAP.
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_type(EAP_TYPE_TTLS_CHAP);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_EAP_TTLS_CHAP, "", "", "", 0,
                        0);
  }
  {
    // Network operator's EAP type is AKA.
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_type(EAP_TYPE_AKA);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_EAP_AKA, "", "", "", 0, 0);
  }
  {
    // Network operator has an anonymous identity specified.
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_anonymous_identity(kTestEAPAnonymousIdentity);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP,
                        kTestEAPAnonymousIdentity, "", "", 0, 0);
  }
  {
    // Network operator has a user identity specified.
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_user_identity(kTestEAPUserIdentity);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP, "",
                        kTestEAPUserIdentity, "", 0, 0);
  }
  {
    // Network operator has a user password specified.
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_user_password(kTestEAPUserPassword);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP, "", "",
                        kTestEAPUserPassword, 0, 0);
  }
  {
    // Network operator bypasses any device certificate.
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_bypass_device_certificate(true);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP, "", "", "", 1, 0);
  }
  {
    // Network operator bypasses any CA certificate.
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_bypass_ca_certificate(true);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP, "", "", "", 0, 1);
  }
}

TEST_F(GdmDeviceTest, ConstructEAPParametersWithInvalidEAPParameters) {
  EAPParameters operator_eap_parameters;
  {
    // Non-string anonymous identity.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetInteger(kEAPAnonymousIdentity, 1);
    connect_parameters.SetString(kEAPUserIdentity, kTestEAPUserIdentity);
    connect_parameters.SetString(kEAPUserPassword, kTestEAPUserPassword);
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
  }
  {
    // Non-string user identity.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetString(kEAPAnonymousIdentity,
                                 kTestEAPAnonymousIdentity);
    connect_parameters.SetInteger(kEAPUserIdentity, 1);
    connect_parameters.SetString(kEAPUserPassword, kTestEAPUserPassword);
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
  }
  {
    // Non-string user password.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetString(kEAPAnonymousIdentity,
                                 kTestEAPAnonymousIdentity);
    connect_parameters.SetString(kEAPUserIdentity, kTestEAPUserIdentity);
    connect_parameters.SetInteger(kEAPUserPassword, 1);
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
  }
  {
    // Anonymous identity too long.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetString(
        kEAPAnonymousIdentity, string(sizeof(eap_parameters.anonymousId), 'a'));
    connect_parameters.SetString(kEAPUserIdentity, kTestEAPUserIdentity);
    connect_parameters.SetString(kEAPUserPassword, kTestEAPUserPassword);
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
  }
  {
    // User identity too long.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetString(kEAPAnonymousIdentity,
                                 kTestEAPAnonymousIdentity);
    connect_parameters.SetString(kEAPUserIdentity,
                                 string(sizeof(eap_parameters.userId), 'a'));
    connect_parameters.SetString(kEAPUserPassword, kTestEAPUserPassword);
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
  }
  {
    // User password too long.
    DictionaryValue connect_parameters;
    GCT_API_EAP_PARAM eap_parameters;
    connect_parameters.SetString(kEAPAnonymousIdentity,
                                 kTestEAPAnonymousIdentity);
    connect_parameters.SetString(kEAPUserIdentity, kTestEAPUserIdentity);
    connect_parameters.SetString(kEAPUserPassword,
                                 string(sizeof(eap_parameters.userIdPwd), 'a'));
    EXPECT_FALSE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
  }
}

TEST_F(GdmDeviceTest, ConstructEAPParametersWithAnonymousIdentityUpdated) {
  DictionaryValue connect_parameters;
  connect_parameters.SetString(kEAPUserIdentity, kTestEAPUserIdentity);

  {
    EAPParameters operator_eap_parameters;
    operator_eap_parameters.set_anonymous_identity(
        kTestEAPAnonymousIdentityWithRealmTag);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP,
                        kTestEAPAnonymousIdentityWithRealmTagReplaced,
                        kTestEAPUserIdentity, "", 0, 0);
  }
  {
    EAPParameters operator_eap_parameters;
    connect_parameters.SetString(kEAPAnonymousIdentity,
                                 kTestEAPAnonymousIdentityWithRealmTag);
    GCT_API_EAP_PARAM eap_parameters;
    EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
        connect_parameters, operator_eap_parameters, &eap_parameters));
    ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP,
                        kTestEAPAnonymousIdentityWithRealmTagReplaced,
                        kTestEAPUserIdentity, "", 0, 0);
  }
}

TEST_F(GdmDeviceTest, ConstructEAPParametersWithoutEAPParameters) {
  DictionaryValue connect_parameters;
  EAPParameters operator_eap_parameters;
  GCT_API_EAP_PARAM eap_parameters;
  EXPECT_TRUE(GdmDevice::ConstructEAPParameters(
      connect_parameters, operator_eap_parameters, &eap_parameters));
  ExpectEAPParameters(eap_parameters, GCT_WIMAX_NO_EAP, "", "", "", 0, 0);
}

}  // namespace wimax_manager
