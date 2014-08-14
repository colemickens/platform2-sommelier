// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/boot_attributes.h"

#include <map>
#include <string>

#include <base/compiler_specific.h>
#include <chromeos/secure_blob.h>
#include <chromeos/utility.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cryptohome/mock_boot_lockbox.h"
#include "cryptohome/mock_crypto.h"
#include "cryptohome/mock_platform.h"
#include "cryptohome/mock_tpm.h"

#include "install_attributes.pb.h"  // NOLINT(build/include)

using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace cryptohome {
namespace {

class BootAttributesTest : public testing::Test {
 public:
  BootAttributesTest() : fake_signature_("fake signature") {
  }

  void SetUp() override {
    ON_CALL(mock_boot_lockbox_, Sign(_, _))
        .WillByDefault(DoAll(SetArgPointee<1>(fake_signature_), Return(true)));

    ON_CALL(mock_boot_lockbox_, Verify(_, _))
        .WillByDefault(Return(true));

    ON_CALL(mock_platform_, ReadFile(_, _))
        .WillByDefault(Invoke(this, &BootAttributesTest::FakeReadFile));

    ON_CALL(mock_platform_, WriteFile(_, _))
        .WillByDefault(Invoke(this, &BootAttributesTest::FakeWriteFile));

    CreateFakeFiles();

    boot_attributes_.reset(
        new BootAttributes(&mock_boot_lockbox_, &mock_platform_));
  }

  void CreateFakeFiles() {
    SerializedInstallAttributes message;
    message.set_version(1);
    SerializedInstallAttributes::Attribute* attr = message.add_attributes();
    attr->set_name("test1");
    attr->set_value("1234");

    chromeos::Blob blob(message.ByteSize());
    message.SerializeWithCachedSizesToArray(&blob[0]);
    files_[BootAttributes::kAttributeFile] = blob;

    blob.clear();
    files_[BootAttributes::kSignatureFile] = blob;
  }

  bool FakeReadFile(const std::string filename, chromeos::Blob* blob) {
    *blob = files_[filename];
    return true;
  }

  bool FakeWriteFile(const std::string filename, chromeos::Blob blob) {
    files_[filename] = blob;
    return true;
  }

 protected:
  const chromeos::SecureBlob fake_signature_;

  MockBootLockbox mock_boot_lockbox_;
  MockPlatform mock_platform_;

  scoped_ptr<BootAttributes> boot_attributes_;

  std::map<std::string, chromeos::Blob> files_;
};

TEST_F(BootAttributesTest, BasicOperations) {
  std::string value;

  // Empty initially.
  EXPECT_FALSE(boot_attributes_->Get("test1", &value));

  // Load values from the file.
  EXPECT_TRUE(boot_attributes_->Load());
  EXPECT_TRUE(boot_attributes_->Get("test1", &value));
  EXPECT_EQ("1234", value);

  // The value should still be unavailable after Set().
  boot_attributes_->Set("test2", "5678");
  EXPECT_FALSE(boot_attributes_->Get("test2", &value));

  // The value becomes available after FlushAndSign().
  EXPECT_TRUE(boot_attributes_->FlushAndSign());
  EXPECT_TRUE(boot_attributes_->Get("test2", &value));
  EXPECT_EQ("5678", value);

  // Overwrite a value
  boot_attributes_->Set("test1", "abcd");
  EXPECT_TRUE(boot_attributes_->FlushAndSign());
  EXPECT_TRUE(boot_attributes_->Get("test1", &value));
  EXPECT_EQ("abcd", value);

  // Verify the attribute file content.
  chromeos::Blob blob = files_[BootAttributes::kAttributeFile];
  SerializedInstallAttributes message;
  message.ParseFromString(
      std::string(reinterpret_cast<char*>(&blob[0]), blob.size()));
  EXPECT_EQ(BootAttributes::kAttributeFileVersion, message.version());

  EXPECT_EQ(2, message.attributes_size());
  bool has_test1 = false;
  bool has_test2 = false;
  for (int i = 0; i < message.attributes_size(); ++i) {
    const SerializedInstallAttributes::Attribute& attr = message.attributes(i);
    if (attr.name() == "test1") {
      has_test1 = true;
      EXPECT_EQ("abcd", attr.value());
    } else if (attr.name() == "test2") {
      has_test2 = true;
      EXPECT_EQ("5678", attr.value());
    } else {
      FAIL();
    }
  }
  EXPECT_TRUE(has_test1);
  EXPECT_TRUE(has_test2);

  // Verify the signature file content.
  blob = files_[BootAttributes::kSignatureFile];
  EXPECT_EQ("fake signature", std::string(
      reinterpret_cast<char*>(&blob[0]), blob.size()));
}

TEST_F(BootAttributesTest, SignFailed) {
  EXPECT_CALL(mock_boot_lockbox_, Sign(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_platform_, WriteFile(_, _))
      .Times(0);

  boot_attributes_->Set("test", "1234");
  EXPECT_FALSE(boot_attributes_->FlushAndSign());
}

TEST_F(BootAttributesTest, WriteFileFailed) {
  EXPECT_CALL(mock_platform_, WriteFile(_, _))
      .WillRepeatedly(Return(false));

  boot_attributes_->Set("test", "1234");
  EXPECT_FALSE(boot_attributes_->FlushAndSign());
}

TEST_F(BootAttributesTest, ReadFileFailed) {
  EXPECT_CALL(mock_platform_, ReadFile(_, _))
      .WillRepeatedly(Return(false));

  EXPECT_FALSE(boot_attributes_->Load());
}

TEST_F(BootAttributesTest, VerifyFailed) {
  EXPECT_CALL(mock_boot_lockbox_, Verify(_, _))
      .WillRepeatedly(Return(false));

  EXPECT_FALSE(boot_attributes_->Load());
}

}  // namespace
}  // namespace cryptohome
