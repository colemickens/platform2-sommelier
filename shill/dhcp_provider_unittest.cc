// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_provider.h"

#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/stringprintf.h>

#include "shill/dhcp_config.h"
#include "shill/mock_control.h"
#include "shill/mock_glib.h"

using base::FilePath;
using base::ScopedTempDir;
using testing::Test;

namespace shill {

namespace {
const char kDeviceName[] = "testdevicename";
const char kHostName[] = "testhostname";
const char kStorageIdentifier[] = "teststorageidentifier";
const bool kArpGateway = false;
}  // namespace {}

class DHCPProviderTest : public Test {
 public:
  DHCPProviderTest() : provider_(DHCPProvider::GetInstance()) {
    provider_->glib_ = &glib_;
    provider_->control_interface_ = &control_;
    // DHCPProvider is a singleton, there is no guarentee that it is
    // not setup/used elsewhere, so reset its state before running our
    // tests.
    provider_->configs_.clear();
  }

 protected:
  MockControl control_;
  MockGLib glib_;
  DHCPProvider *provider_;
};

TEST_F(DHCPProviderTest, CreateConfig) {
  DHCPConfigRefPtr config = provider_->CreateConfig(kDeviceName,
                                                    kHostName,
                                                    kStorageIdentifier,
                                                    kArpGateway);
  EXPECT_TRUE(config.get());
  EXPECT_EQ(&glib_, config->glib_);
  EXPECT_EQ(kDeviceName, config->device_name());
  EXPECT_TRUE(provider_->configs_.empty());
}

TEST_F(DHCPProviderTest, DestroyLease) {
  ScopedTempDir temp_dir;
  FilePath lease_file;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  provider_->root_ = temp_dir.path();
  lease_file = provider_->root_.Append(base::StringPrintf(
      DHCPProvider::kDHCPCDPathFormatLease,
      kDeviceName));
  EXPECT_TRUE(file_util::CreateDirectory(lease_file.DirName()));
  EXPECT_EQ(0, file_util::WriteFile(lease_file, "", 0));
  EXPECT_TRUE(file_util::PathExists(lease_file));
  provider_->DestroyLease(kDeviceName);
  EXPECT_FALSE(file_util::PathExists(lease_file));
}

}  // namespace shill
