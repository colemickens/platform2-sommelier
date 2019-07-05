// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/krb5_interface_impl.h"

#include <string>

#include <gtest/gtest.h>

namespace kerberos {

namespace {

constexpr char kValidConfig[] = "";
constexpr char kBadKrb5conf[] = "\n\n[libdefaults";

}  // namespace

class Krb5InterfaceImplTest : public ::testing::Test {
 public:
  Krb5InterfaceImplTest() {}
  ~Krb5InterfaceImplTest() override = default;

 protected:
  Krb5InterfaceImpl krb5_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Krb5InterfaceImplTest);
};

// Can't test terribly much here since the actual krb5 functionality involves
// network operations. The validation can be tested, though.

// Tests config validation with a valid config.
TEST_F(Krb5InterfaceImplTest, ValidateConfigSuccess) {
  ConfigErrorInfo error_info;
  ErrorType error = krb5_.ValidateConfig(kValidConfig, &error_info);
  EXPECT_EQ(ERROR_NONE, error);
  EXPECT_TRUE(error_info.has_code());
  EXPECT_EQ(CONFIG_ERROR_NONE, error_info.code());
  EXPECT_FALSE(error_info.has_line_index());
}

// Tests config validation with a bad config.
TEST_F(Krb5InterfaceImplTest, ValidateConfigFailure) {
  ConfigErrorInfo error_info;
  ErrorType error = krb5_.ValidateConfig(kBadKrb5conf, &error_info);
  EXPECT_EQ(ERROR_BAD_CONFIG, error);
  EXPECT_EQ(CONFIG_ERROR_SECTION_SYNTAX, error_info.code());
  EXPECT_TRUE(error_info.has_line_index());
  EXPECT_EQ(2, error_info.line_index());
}

// Tests the krb5-part of config validation.
TEST_F(Krb5InterfaceImplTest, ValidateConfigViaKrb5Failure) {
  // I didn't see a way to make the krb5-part fail without making the
  // ConfigValidator-part fail, so just disable the ConfigValidator-part.
  krb5_.DisableConfigValidatorForTesting();

  ConfigErrorInfo error_info;
  ErrorType error = krb5_.ValidateConfig(kBadKrb5conf, &error_info);
  EXPECT_EQ(ERROR_BAD_CONFIG, error);
  EXPECT_EQ(CONFIG_ERROR_KRB5_FAILED_TO_PARSE, error_info.code());
  EXPECT_FALSE(error_info.has_line_index());
}

}  // namespace kerberos
