// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/load_oobe_config_usb.h"

#include <memory>
#include <string>

#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

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
  }

  ScopedTempDir fake_stateful_;
  ScopedTempDir fake_device_ids_dir_;
  unique_ptr<LoadOobeConfigUsb> load_config_;
};

TEST_F(LoadOobeConfigUsbTest, SimpleTest) {
  string config, enrollment_domain;
  EXPECT_FALSE(load_config_->GetOobeConfigJson(&config, &enrollment_domain));
}

}  // namespace oobe_config
