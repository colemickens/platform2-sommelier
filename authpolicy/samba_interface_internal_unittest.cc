// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "authpolicy/samba_interface.h"
#include "authpolicy/samba_interface_internal.h"

namespace ai = authpolicy::internal;

namespace authpolicy {

class SambaInterfaceTest : public ::testing::Test {
 public:
  SambaInterfaceTest() {}
  ~SambaInterfaceTest() override {}

 protected:
  // Helpers for ParseUserPrincipleName.
  std::string user_name_;
  std::string realm_;
  std::string workgroup_;
  std::string normalized_upn_;
  ErrorType error_ = ERROR_NONE;

  bool ParseUserPrincipalName(const char* user_principal_name_) {
    return ai::ParseUserPrincipalName(user_principal_name_, &user_name_,
                                      &realm_, &workgroup_, &normalized_upn_,
                                      &error_);
  }

  // Helpers for FindToken.
  std::string find_token_result_;

  bool FindToken(const char* in_str, char token_separator, const char* token) {
    return ai::FindToken(in_str, token_separator, token, &find_token_result_);
  }

  // Helpers for ParseGpoVersion
  unsigned int gpo_version_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SambaInterfaceTest);
};

// a@b.c succeeds.
TEST_F(SambaInterfaceTest, ParseUPNSuccess) {
  EXPECT_TRUE(ParseUserPrincipalName("usar@wokgroup.doomain"));
  EXPECT_EQ(user_name_, "usar");
  EXPECT_EQ(realm_, "WOKGROUP.DOOMAIN");
  EXPECT_EQ(workgroup_, "WOKGROUP");
  EXPECT_EQ(normalized_upn_, "usar@WOKGROUP.DOOMAIN");
  EXPECT_EQ(error_, ERROR_NONE);
}

// a@b.c.d.e succeeds.
TEST_F(SambaInterfaceTest, ParseUPNSuccess_Long) {
  EXPECT_TRUE(ParseUserPrincipalName("usar@wokgroup.doomain.company.com"));
  EXPECT_EQ(user_name_, "usar");
  EXPECT_EQ(realm_, "WOKGROUP.DOOMAIN.COMPANY.COM");
  EXPECT_EQ(workgroup_, "WOKGROUP");
  EXPECT_EQ(normalized_upn_, "usar@WOKGROUP.DOOMAIN.COMPANY.COM");
}

// Capitalization works as expected.
TEST_F(SambaInterfaceTest, ParseUPNSuccess_MixedCaps) {
  EXPECT_TRUE(ParseUserPrincipalName("UsAr@WoKgrOUP.DOOMain.com"));
  EXPECT_EQ(user_name_, "UsAr");
  EXPECT_EQ(realm_, "WOKGROUP.DOOMAIN.COM");
  EXPECT_EQ(workgroup_, "WOKGROUP");
  EXPECT_EQ(normalized_upn_, "UsAr@WOKGROUP.DOOMAIN.COM");
}

// a@b@c fails (missing .d).
TEST_F(SambaInterfaceTest, ParseUPNSuccess_AtAt) {
  EXPECT_FALSE(ParseUserPrincipalName("usar@wokgroup@doomain"));
  EXPECT_EQ(error_, ERROR_PARSE_UPN_FAILED);
}

// a@b@c.d succeeds, even though it is invalid (rejected by kinit).
TEST_F(SambaInterfaceTest, ParseUPNSuccess_AtAtDot) {
  EXPECT_TRUE(ParseUserPrincipalName("usar@wokgroup@doomain.com"));
  EXPECT_EQ(user_name_, "usar");
  EXPECT_EQ(realm_, "WOKGROUP@DOOMAIN.COM");
  EXPECT_EQ(workgroup_, "WOKGROUP@DOOMAIN");
  EXPECT_EQ(normalized_upn_, "usar@WOKGROUP@DOOMAIN.COM");
}

// a.b@c.d succeeds, even though it is invalid (rejected by kinit).
TEST_F(SambaInterfaceTest, ParseUPNSuccess_DotAtDot) {
  EXPECT_TRUE(ParseUserPrincipalName("usar.team@wokgroup.doomain"));
  EXPECT_EQ(user_name_, "usar.team");
  EXPECT_EQ(realm_, "WOKGROUP.DOOMAIN");
  EXPECT_EQ(workgroup_, "WOKGROUP");
  EXPECT_EQ(normalized_upn_, "usar.team@WOKGROUP.DOOMAIN");
}

// a@ fails (no workgroup.domain).
TEST_F(SambaInterfaceTest, ParseUPNFail_NoRealm) {
  EXPECT_FALSE(ParseUserPrincipalName("usar@"));
  EXPECT_EQ(error_, ERROR_PARSE_UPN_FAILED);
}

// a fails (no @workgroup.domain).
TEST_F(SambaInterfaceTest, ParseUPNFail_NoAtRealm) {
  EXPECT_FALSE(ParseUserPrincipalName("usar"));
  EXPECT_EQ(error_, ERROR_PARSE_UPN_FAILED);
}

// a. fails (no @workgroup.domain and trailing . is invalid, anyway).
TEST_F(SambaInterfaceTest, ParseUPNFail_NoAtRealmButDot) {
  EXPECT_FALSE(ParseUserPrincipalName("usar."));
  EXPECT_EQ(error_, ERROR_PARSE_UPN_FAILED);
}

// @b.c fails (empty user name).
TEST_F(SambaInterfaceTest, ParseUPNFail_NoUpn) {
  EXPECT_FALSE(ParseUserPrincipalName("@wokgroup.doomain"));
  EXPECT_EQ(error_, ERROR_PARSE_UPN_FAILED);
}

// b.c fails (no user name@).
TEST_F(SambaInterfaceTest, ParseUPNFail_NoUpnAt) {
  EXPECT_FALSE(ParseUserPrincipalName("wokgroup.doomain"));
  EXPECT_EQ(error_, ERROR_PARSE_UPN_FAILED);
}

// .b.c fails (no user name@ and initial . is invalid, anyway).
TEST_F(SambaInterfaceTest, ParseUPNFail_NoUpnAtButDot) {
  EXPECT_FALSE(ParseUserPrincipalName(".wokgroup.doomain"));
  EXPECT_EQ(error_, ERROR_PARSE_UPN_FAILED);
}

// a=b works.
TEST_F(SambaInterfaceTest, FindTokenSuccess) {
  EXPECT_TRUE(FindToken("tok=res", '=', "tok"));
  EXPECT_EQ(find_token_result_, "res");
}

// Multiple matches return the first match
TEST_F(SambaInterfaceTest, FindTokenSuccess_Multiple) {
  EXPECT_TRUE(FindToken("tok=res\ntok=res2", '=', "tok"));
  EXPECT_EQ(find_token_result_, "res");
}

// Different separators are ignored matches return the first match
TEST_F(SambaInterfaceTest, FindTokenSuccess_IgnoreInvalidSeparator) {
  EXPECT_TRUE(FindToken("tok:res\ntok=res2", '=', "tok"));
  EXPECT_EQ(find_token_result_, "res2");
}

// a=b=c returns b=c
TEST_F(SambaInterfaceTest, FindTokenSuccess_TwoSeparators) {
  EXPECT_TRUE(FindToken("tok = res = true", '=', "tok"));
  EXPECT_EQ(find_token_result_, "res = true");
}

// Trims leading and trailing whitespace
TEST_F(SambaInterfaceTest, FindTokenSuccess_TrimWhitespace) {
  EXPECT_TRUE(FindToken("\n   \n\n tok  =  res   \n\n", '=', "tok"));
  EXPECT_EQ(find_token_result_, "res");
}

// Empty input strings fail.
TEST_F(SambaInterfaceTest, FindTokenFail_Empty) {
  EXPECT_FALSE(FindToken("", '=', "tok"));
  EXPECT_FALSE(FindToken("\n", '=', "tok"));
  EXPECT_FALSE(FindToken("\n\n\n", '=', "tok"));
}

// Whitespace input strings fail.
TEST_F(SambaInterfaceTest, FindTokenFail_Whitespace) {
  EXPECT_FALSE(FindToken("    ", '=', "tok"));
  EXPECT_FALSE(FindToken("    \n   \n ", '=', "tok"));
  EXPECT_FALSE(FindToken("    \n\n \n   ", '=', "tok"));
}

// Parsing valid GPO version strings.
TEST_F(SambaInterfaceTest, ParseGpoVersionSuccess) {
  EXPECT_TRUE(ai::ParseGpoVersion("0 (0x0000)", &gpo_version_));
  EXPECT_EQ(gpo_version_, 0);
  EXPECT_TRUE(ai::ParseGpoVersion("1 (0x0001)", &gpo_version_));
  EXPECT_EQ(gpo_version_, 1);
  EXPECT_TRUE(ai::ParseGpoVersion("9 (0x0009)", &gpo_version_));
  EXPECT_EQ(gpo_version_, 9);
  EXPECT_TRUE(ai::ParseGpoVersion("15 (0x000f)", &gpo_version_));
  EXPECT_EQ(gpo_version_, 15);
  EXPECT_TRUE(ai::ParseGpoVersion("65535 (0xffff)", &gpo_version_));
  EXPECT_EQ(gpo_version_, 0xffff);
}

// Empty string
TEST_F(SambaInterfaceTest, ParseGpoVersionFail_EmptyString) {
  EXPECT_FALSE(ai::ParseGpoVersion("", &gpo_version_));
}

// Base-10 and Base-16 (hex) numbers not matching
TEST_F(SambaInterfaceTest, ParseGpoVersionFail_NotMatching) {
  EXPECT_FALSE(ai::ParseGpoVersion("15 (0x000e)", &gpo_version_));
}

// Non-numeric characters fail
TEST_F(SambaInterfaceTest, ParseGpoVersionFail_NonNumericCharacters) {
  EXPECT_FALSE(ai::ParseGpoVersion("15a (0x00f)", &gpo_version_));
  EXPECT_FALSE(ai::ParseGpoVersion("15 (0xg0f)", &gpo_version_));
  EXPECT_FALSE(ai::ParseGpoVersion("dead", &gpo_version_));
}

// Missing 0x in hex string fails
TEST_F(SambaInterfaceTest, ParseGpoVersionFail_Missing0x) {
  EXPECT_FALSE(ai::ParseGpoVersion("15 (000f)", &gpo_version_));
}

// Missing brackets in hex string fail
TEST_F(SambaInterfaceTest, ParseGpoVersionFail_MissingBrackets) {
  EXPECT_FALSE(ai::ParseGpoVersion("15 000f", &gpo_version_));
}

// Missing hex string fails
TEST_F(SambaInterfaceTest, ParseGpoVersionFail_MissingHex) {
  EXPECT_FALSE(ai::ParseGpoVersion("10", &gpo_version_));
}

// Only hex string fails
TEST_F(SambaInterfaceTest, ParseGpoVersionFail_HexOnly) {
  EXPECT_FALSE(ai::ParseGpoVersion("0x000f", &gpo_version_));
}

// Only hex string in brackets fails
TEST_F(SambaInterfaceTest, ParseGpoVersionFail_BracketsHexOnly) {
  EXPECT_FALSE(ai::ParseGpoVersion("(0x000f)", &gpo_version_));
}

}  // namespace authpolicy
