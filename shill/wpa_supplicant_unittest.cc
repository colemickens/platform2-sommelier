// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wpa_supplicant.h"

#include <base/file_path.h>
#include <base/stl_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/eap_credentials.h"
#include "shill/mock_certificate_file.h"
#include "shill/mock_log.h"
#include "shill/mock_nss.h"

using base::FilePath;
using std::map;
using std::string;
using std::vector;
using testing::_;
using testing::EndsWith;
using testing::Return;

namespace shill {

class WPASupplicantTest : public testing::Test {
 public:
  WPASupplicantTest() {}
  virtual ~WPASupplicantTest() {}

 protected:
  typedef std::map<std::string, DBus::Variant> PropertyMap;

  void Populate() {
    WPASupplicant::Populate8021xProperties(eap_, &certificate_file_,
                                           &nss_, nss_identifier_, &params_);
  }

  EapCredentials eap_;
  MockCertificateFile certificate_file_;
  MockNSS nss_;
  vector<char> nss_identifier_;
  map<string, ::DBus::Variant> params_;
};

TEST_F(WPASupplicantTest, Populate8021x) {
  eap_.identity = "testidentity";
  eap_.pin = "xxxx";
  Populate();
  // Test that only non-empty 802.1x properties are populated.
  EXPECT_TRUE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEapIdentity));
  EXPECT_FALSE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEapKeyId));
  EXPECT_FALSE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEapCaCert));

  // Test that CA path is set by default.
  EXPECT_TRUE(ContainsKey(params_, WPASupplicant::kNetworkPropertyCaPath));

  // Test that hardware-backed security arguments are not set.
  EXPECT_FALSE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEapPin));
  EXPECT_FALSE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEngine));
  EXPECT_FALSE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEngineId));
}

TEST_F(WPASupplicantTest, Populate8021xNoSystemCAs) {
  eap_.identity = "testidentity";
  eap_.use_system_cas = false;
  Populate();
  // Test that CA path is not set if use_system_cas is explicitly false.
  EXPECT_FALSE(ContainsKey(params_, WPASupplicant::kNetworkPropertyCaPath));
}

TEST_F(WPASupplicantTest, Populate8021xUsingHardwareAuth) {
  eap_.identity = "testidentity";
  eap_.key_id = "key_id";
  eap_.pin = "xxxx";
  Populate();
  // Test that EAP engine parameters set if key_id is set.
  EXPECT_TRUE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEapPin));
  EXPECT_TRUE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEapKeyId));
  EXPECT_TRUE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEngine));
  EXPECT_TRUE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEngineId));
}

TEST_F(WPASupplicantTest, Populate8021xNSS) {
  eap_.ca_cert_nss = "nss_nickname";
  const string kNSSCertfile("/tmp/nss-cert");
  FilePath nss_cert(kNSSCertfile);
  nss_identifier_ = vector<char>(1, 'a');
  EXPECT_CALL(nss_, GetDERCertfile(eap_.ca_cert_nss, nss_identifier_))
      .WillOnce(Return(nss_cert));
  Populate();
  EXPECT_TRUE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEapCaCert));
  if (ContainsKey(params_, WPASupplicant::kNetworkPropertyEapCaCert)) {
    EXPECT_EQ(kNSSCertfile, params_[WPASupplicant::kNetworkPropertyEapCaCert]
              .reader().get_string());
  }
}

TEST_F(WPASupplicantTest, Populate8021xPEM) {
  eap_.ca_cert_pem = "-pem-certificate-here-";
  const string kPEMCertfile("/tmp/pem-cert");
  FilePath pem_cert(kPEMCertfile);
  EXPECT_CALL(certificate_file_, CreateDERFromString(eap_.ca_cert_pem))
      .WillOnce(Return(pem_cert));

  Populate();
  EXPECT_TRUE(ContainsKey(params_, WPASupplicant::kNetworkPropertyEapCaCert));
  if (ContainsKey(params_, WPASupplicant::kNetworkPropertyEapCaCert)) {
    EXPECT_EQ(kPEMCertfile, params_[WPASupplicant::kNetworkPropertyEapCaCert]
              .reader().get_string());
  }
}

TEST_F(WPASupplicantTest, ExtractRemoteCertificationEmpty) {
  string subject;
  uint32 depth = 0;
  ScopedMockLog log;
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _, EndsWith("no depth parameter.")));
  EXPECT_FALSE(WPASupplicant::ExtractRemoteCertification(
      PropertyMap(), &subject, &depth));
  EXPECT_EQ("", subject);
  EXPECT_EQ(0, depth);
}

TEST_F(WPASupplicantTest, ExtractRemoteCertificationDepthOnly) {
  string subject;
  const uint32 kDepthValue = 100;
  uint32 depth = kDepthValue - 1;
  PropertyMap property_map;
  property_map[WPASupplicant::kInterfacePropertyDepth]
      .writer().append_uint32(kDepthValue);
  ScopedMockLog log;
  EXPECT_CALL(log,
              Log(logging::LOG_ERROR, _, EndsWith("no subject parameter.")));
  EXPECT_FALSE(WPASupplicant::ExtractRemoteCertification(
      property_map, &subject, &depth));
  EXPECT_EQ("", subject);
  EXPECT_NE(kDepthValue, depth);
}

TEST_F(WPASupplicantTest, ExtractRemoteCertificationSubjectOnly) {
  const char kSubjectName[] = "subject-name";
  string subject;
  uint32 depth = 0;
  PropertyMap property_map;
  property_map[WPASupplicant::kInterfacePropertySubject]
      .writer().append_string(kSubjectName);
  ScopedMockLog log;
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _, EndsWith("no depth parameter.")));
  EXPECT_FALSE(WPASupplicant::ExtractRemoteCertification(
      property_map, &subject, &depth));
  EXPECT_EQ("", subject);
  EXPECT_EQ(0, depth);
}

TEST_F(WPASupplicantTest, ExtractRemoteCertificationSubjectAndDepth) {
  const char kSubjectName[] = "subject-name";
  string subject;
  const uint32 kDepthValue = 100;
  uint32 depth = 0;
  PropertyMap property_map;
  property_map[WPASupplicant::kInterfacePropertySubject]
      .writer().append_string(kSubjectName);
  property_map[WPASupplicant::kInterfacePropertyDepth]
      .writer().append_uint32(kDepthValue);
  EXPECT_TRUE(WPASupplicant::ExtractRemoteCertification(
      property_map, &subject, &depth));
  EXPECT_EQ(kSubjectName, subject);
  EXPECT_EQ(kDepthValue, depth);
}

}  // namespace shill
