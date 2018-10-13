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

    // Copy the public key to the oobe config directory.
    EXPECT_TRUE(base::CopyFile(public_key_file_, load_config_->pub_key_file_));
  }

  void TestLocateUsbDevice(const FilePath& private_key, bool expect_result) {
    FilePath dev_id1, dev_id2;
    EXPECT_TRUE(base::CreateTemporaryFile(&dev_id1));
    ScopedPathUnlinker unlinker1(dev_id1);
    EXPECT_TRUE(base::CreateTemporaryFile(&dev_id2));
    ScopedPathUnlinker unlinker2(dev_id2);

    FilePath dev_id1_sym = load_config_->device_ids_dir_.Append("dev_id1_sym");
    FilePath dev_id2_sym = load_config_->device_ids_dir_.Append("dev_id2_sym");
    EXPECT_TRUE(base::CreateSymbolicLink(dev_id1, dev_id1_sym));
    EXPECT_TRUE(base::CreateSymbolicLink(dev_id2, dev_id2_sym));

    EXPECT_TRUE(Sign(private_key, dev_id2_sym.value(),
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

  FilePath public_key_file_{"test.pub.key"};
  FilePath private_key_file_{"test.pri.key"};
  FilePath alt_private_key_file_{"test.inv.pri.key"};
};

TEST_F(LoadOobeConfigUsbTest, SimpleTest) {
  string config, enrollment_domain;
  EXPECT_FALSE(load_config_->GetOobeConfigJson(&config, &enrollment_domain));
}

TEST_F(LoadOobeConfigUsbTest, LocateUsbDevice) {
  TestLocateUsbDevice(private_key_file_, true);
}

TEST_F(LoadOobeConfigUsbTest, LocateUsbDeviceFail) {
  TestLocateUsbDevice(alt_private_key_file_, false);
}

}  // namespace oobe_config
