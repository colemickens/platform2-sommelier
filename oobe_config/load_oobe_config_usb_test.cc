// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/load_oobe_config_usb.h"

#include <memory>
#include <string>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "oobe_config/usb_utils.h"

using base::FilePath;
using base::ScopedTempDir;
using std::string;
using std::unique_ptr;
using std::vector;

namespace {
// openssl ecparam -name prime256v1 -genkey -noout -out pri_key.pem
constexpr char kPrivateKey[] =
    R"""(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIF36VqEQX2EpA+vKd4WYI5qwI1iGD4RQ1v2NZ/iaSwvXoAoGCCqGSM49
AwEHoUQDQgAElQ4lW7nrvCnPVsv0GKScu6qQxIw0+NXOja+ks6UtQZB7+iolUsuh
OMl6fZkCkaz7MUs+T9Y62P7L9OcJQdC3Kw==
-----END EC PRIVATE KEY-----
)""";

// openssl ec -in pri_key.pem -pubout -out pub_key.pem
constexpr char kPublicKey[] =
    R"""(-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAElQ4lW7nrvCnPVsv0GKScu6qQxIw0
+NXOja+ks6UtQZB7+iolUsuhOMl6fZkCkaz7MUs+T9Y62P7L9OcJQdC3Kw==
-----END PUBLIC KEY-----
)""";

// And alternative private key used to create invalid signatures.
constexpr char kAltPrivateKey[] =
    R"""(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIHzVEL5EeLKgugnPxH2RLa0mmzT04OlDRnFSL2Mrgsy8oAoGCCqGSM49
AwEHoUQDQgAEpM/3Crk2cwY5kHbKxXeSR+8Bsn0XCE4h9DWx5yirrhj+ykKUG0ZC
o5Xqx5wpkR97aNrrkAldLb7kP3X2LS1ERw==
-----END EC PRIVATE KEY-----
)""";

}  // namespace

namespace oobe_config {

class LoadOobeConfigUsbTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(fake_stateful_.CreateUniqueTempDir());
    EXPECT_TRUE(fake_device_ids_dir_.CreateUniqueTempDir());
    load_config_ = std::make_unique<LoadOobeConfigUsb>(
        fake_stateful_.GetPath(), fake_device_ids_dir_.GetPath());
    EXPECT_TRUE(load_config_);
    EXPECT_TRUE(
        base::CreateDirectory(load_config_->unencrypted_oobe_config_dir_));

    // Write keypairs into files so we can use for verification.
    EXPECT_EQ(base::WriteFile(load_config_->pub_key_file_, kPublicKey,
                              sizeof(kPublicKey)),
              sizeof(kPublicKey));

    EXPECT_TRUE(CreateTemporaryFile(&private_key_file_));
    EXPECT_EQ(
        base::WriteFile(private_key_file_, kPrivateKey, sizeof(kPrivateKey)),
        sizeof(kPrivateKey));

    EXPECT_TRUE(CreateTemporaryFile(&alt_private_key_file_));
    EXPECT_EQ(base::WriteFile(alt_private_key_file_, kAltPrivateKey,
                              sizeof(kAltPrivateKey)),
              sizeof(kAltPrivateKey));
  }

  void TestVerifySignature(const string& data_to_sign,
                           const string& data_to_verify,
                           const FilePath& private_key_file,
                           bool expected_result) {
    FilePath data_file, signature_file;
    EXPECT_TRUE(CreateTemporaryFile(&data_file));
    EXPECT_TRUE(CreateTemporaryFile(&signature_file));
    EXPECT_EQ(WriteFile(data_file, data_to_sign.c_str(), data_to_sign.length()),
              data_to_sign.length());
    EXPECT_TRUE(SignFile(private_key_file, data_file, signature_file));
    string signature;
    EXPECT_TRUE(ReadFileToString(signature_file, &signature));
    EXPECT_TRUE(load_config_->ReadPublicKey());
    EXPECT_EQ(load_config_->VerifySignature(data_to_verify, signature),
              expected_result);
  }

  void TestLocateUsbDevice(const FilePath& private_key, bool expect_result) {
    FilePath dev_id1, dev_id2;
    EXPECT_TRUE(base::CreateTemporaryFile(&dev_id1));
    EXPECT_TRUE(base::CreateTemporaryFile(&dev_id2));

    FilePath dev_id1_sym = load_config_->device_ids_dir_.Append("dev_id1_sym");
    FilePath dev_id2_sym = load_config_->device_ids_dir_.Append("dev_id2_sym");
    EXPECT_TRUE(base::CreateSymbolicLink(dev_id1, dev_id1_sym));
    EXPECT_TRUE(base::CreateSymbolicLink(dev_id2, dev_id2_sym));

    FilePath temp_link;
    EXPECT_TRUE(base::CreateTemporaryFile(&temp_link));
    EXPECT_EQ(base::WriteFile(temp_link, dev_id2_sym.value().c_str(),
                              dev_id2_sym.value().size()),
              dev_id2_sym.value().size());
    EXPECT_TRUE(SignFile(private_key, temp_link,
                         load_config_->usb_device_path_signature_file_));

    EXPECT_TRUE(load_config_->ReadFiles(true /* ignore_errors */));
    FilePath found_usb_device;
    EXPECT_EQ(load_config_->LocateUsbDevice(&found_usb_device), expect_result);
    EXPECT_EQ(base::MakeAbsoluteFilePath(dev_id2) == found_usb_device,
              expect_result);
  }

  ScopedTempDir fake_stateful_;
  ScopedTempDir fake_device_ids_dir_;
  unique_ptr<LoadOobeConfigUsb> load_config_;

  FilePath private_key_file_;
  FilePath alt_private_key_file_;
};

TEST_F(LoadOobeConfigUsbTest, SimpleTest) {
  string config, enrollment_domain;
  EXPECT_FALSE(load_config_->GetOobeConfigJson(&config, &enrollment_domain));
}

TEST_F(LoadOobeConfigUsbTest, VerifySignature) {
  const string kData = "This is a test string!!!";
  TestVerifySignature(kData, kData, private_key_file_, true);
}

TEST_F(LoadOobeConfigUsbTest, VerifySignatureFailKey) {
  const string kData = "This is a test string!!!";
  TestVerifySignature(kData, kData, alt_private_key_file_, false);
}

TEST_F(LoadOobeConfigUsbTest, VerifySignatureFailData) {
  const string kData = "This is a test string!!!";
  const string kInvalidData = "This is an invalid test string!!!";
  TestVerifySignature(kData, kInvalidData, private_key_file_, false);
}

TEST_F(LoadOobeConfigUsbTest, LocateUsbDevice) {
  TestLocateUsbDevice(private_key_file_, true);
}

TEST_F(LoadOobeConfigUsbTest, LocateUsbDeviceFail) {
  TestLocateUsbDevice(alt_private_key_file_, false);
}

}  // namespace oobe_config
