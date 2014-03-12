// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "boot_lockbox.h"

#include <map>
#include <string>

#include <base/stl_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_crypto.h"
#include "mock_platform.h"
#include "mock_tpm.h"

using testing::NiceMock;
using testing::WithArgs;
using testing::Return;

namespace cryptohome {

class BootLockboxTest : public testing::Test {
 public:
  BootLockboxTest() : is_fake_extended_(false), rsa_(NULL) {}
  virtual ~BootLockboxTest() {}

  void SetUp() {
    // Configure a fake TPM.
    ON_CALL(tpm_, Sign(_, _, _))
        .WillByDefault(WithArgs<1,2>(Invoke(this, &BootLockboxTest::FakeSign)));
    ON_CALL(tpm_, CreatePCRBoundKey(_, _, _, _))
        .WillByDefault(WithArgs<3>(Invoke(this, &BootLockboxTest::FakeCreate)));
    ON_CALL(tpm_, VerifyPCRBoundKey(_, _, _))
        .WillByDefault(Return(true));
    ON_CALL(tpm_, ExtendPCR(_, _))
        .WillByDefault(InvokeWithoutArgs(this, &BootLockboxTest::FakeExtend));
    ON_CALL(crypto_, EncryptWithTpm(_, _)).WillByDefault(Return(true));
    ON_CALL(crypto_, DecryptWithTpm(_, _)).WillByDefault(Return(true));
    // Configure a fake filesystem.
    ON_CALL(platform_, WriteStringToFile(_, _))
        .WillByDefault(Invoke(this, &BootLockboxTest::FakeWriteFile));
    ON_CALL(platform_, ReadFileToString(_, _))
        .WillByDefault(Invoke(this, &BootLockboxTest::FakeReadFile));
    lockbox_.reset(new BootLockbox(&tpm_, &platform_, &crypto_));
  }

  bool FakeSign(const chromeos::SecureBlob& der_encoded_input,
                chromeos::SecureBlob* signature) {
    if (is_fake_extended_)
      return false;
    unsigned char buffer[256];
    int length = RSA_private_encrypt(
          der_encoded_input.size(),
          const_cast<unsigned char*>(vector_as_array(&der_encoded_input)),
          buffer, rsa(), RSA_PKCS1_PADDING);
    chromeos::SecureBlob tmp(buffer, length);
    signature->swap(tmp);
    return true;
  }

  bool FakeCreate(chromeos::SecureBlob* public_key) {
    unsigned char* buffer = NULL;
    int length = i2d_RSAPublicKey(rsa(), &buffer);
    if (length <= 0)
      return false;
    chromeos::SecureBlob tmp(buffer, length);
    public_key->swap(tmp);
    OPENSSL_free(buffer);
    return true;
  }

  bool FakeExtend() {
    is_fake_extended_ = true;
    return true;
  }

  bool FakeWriteFile(const std::string& path, const std::string& db) {
    fake_files[path] = db;
    return true;
  }

  bool FakeReadFile(const std::string& path, std::string* db) {
    if (fake_files.count(path) == 0)
      return false;
    *db = fake_files[path];
    return true;
  }

 protected:
  NiceMock<MockTpm> tpm_;
  NiceMock<MockPlatform> platform_;
  NiceMock<MockCrypto> crypto_;
  scoped_ptr<BootLockbox> lockbox_;
  bool is_fake_extended_;
  std::map<std::string, std::string> fake_files;
  RSA* rsa_;  // Access with rsa().

  RSA* rsa() {
    if (!rsa_) {
      rsa_ = RSA_generate_key(2048, 65537, NULL, NULL);
      CHECK(rsa_);
    }
    return rsa_;
  }
};

TEST_F(BootLockboxTest, NormalUse) {
  chromeos::SecureBlob data(100);
  chromeos::SecureBlob signature;
  ASSERT_TRUE(lockbox_->Sign(data, &signature));
  EXPECT_LT(0, signature.size());
  ASSERT_TRUE(lockbox_->Verify(data, signature));
  lockbox_->FinalizeBoot();
  ASSERT_TRUE(lockbox_->Verify(data, signature));
}

TEST_F(BootLockboxTest, SignAfterFinalize) {
  chromeos::SecureBlob data(100);
  chromeos::SecureBlob signature;
  ASSERT_TRUE(lockbox_->Sign(data, &signature));
  lockbox_->FinalizeBoot();
  ASSERT_FALSE(lockbox_->Sign(data, &signature));
}

}  // namespace cryptohome
