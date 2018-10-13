// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/usb_utils.h"

#include <string>

#include <base/files/file_path.h>
#include <gtest/gtest.h>

using base::FilePath;
using std::string;

namespace oobe_config {

class UsbUtilsTest : public ::testing::Test {
 protected:
  void TestVerifySignature(const string& data_to_sign,
                           const string& data_to_verify,
                           const FilePath& private_key_file,
                           bool expected_result) {
    FilePath signature_file;
    EXPECT_TRUE(CreateTemporaryFile(&signature_file));
    ScopedPathUnlinker unlinker(signature_file);
    EXPECT_TRUE(Sign(private_key_file, data_to_sign, signature_file));
    string signature;
    EXPECT_TRUE(ReadFileToString(signature_file, &signature));
    EXPECT_TRUE(ReadPublicKey(public_key_file_, &public_key));
    EXPECT_EQ(VerifySignature(data_to_verify, signature, public_key),
              expected_result);
  }

  FilePath public_key_file_{"test.pub.key"};
  FilePath alt_public_key_file_{"test.inv.pub.key"};
  FilePath private_key_file_{"test.pri.key"};
  FilePath alt_private_key_file_{"test.inv.pri.key"};

  crypto::ScopedEVP_PKEY public_key;
};

TEST_F(UsbUtilsTest, ScopedPathUnlinker) {
  FilePath temp_file;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_file));
  {
    ScopedPathUnlinker unlinker(temp_file);
    EXPECT_TRUE(base::PathExists(temp_file));
  }
  EXPECT_FALSE(base::PathExists(temp_file));
}

TEST_F(UsbUtilsTest, ReadPublicKeyFail) {
  FilePath pubkey_file;
  crypto::ScopedEVP_PKEY pubkey;
  EXPECT_FALSE(ReadPublicKey(pubkey_file, &pubkey));
  EXPECT_FALSE(ReadPublicKey(alt_public_key_file_, &pubkey));
}

TEST_F(UsbUtilsTest, SignFailInvalidPrivateKeyPath) {
  FilePath signature_file;
  EXPECT_TRUE(base::CreateTemporaryFile(&signature_file));
  ScopedPathUnlinker unlinker(signature_file);
  FilePath priv_key;
  EXPECT_FALSE(Sign(priv_key, "blah blah", signature_file));
}

TEST_F(UsbUtilsTest, SignFailEmptyContent) {
  FilePath signature_file;
  EXPECT_TRUE(base::CreateTemporaryFile(&signature_file));
  ScopedPathUnlinker unlinker(signature_file);
  EXPECT_FALSE(Sign(private_key_file_, "", signature_file));
}

TEST_F(UsbUtilsTest, VerifySignature) {
  const string kData = "This is a test string!!!";
  TestVerifySignature(kData, kData, private_key_file_, true);
}

TEST_F(UsbUtilsTest, VerifySignatureFailKey) {
  const string kData = "This is a test string!!!";
  TestVerifySignature(kData, kData, alt_private_key_file_, false);
}

TEST_F(UsbUtilsTest, VerifySignatureFailData) {
  const string kData = "This is a test string!!!";
  const string kInvalidData = "This is an invalid test string!!!";
  TestVerifySignature(kData, kInvalidData, private_key_file_, false);
}

}  // namespace oobe_config
