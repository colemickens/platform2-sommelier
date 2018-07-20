// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>
#include <imageloader/dbus-proxy-mocks.h>

#include "dlcservice/dlc_service_dbus_adaptor.h"

namespace dlcservice {

namespace {
constexpr char kFirstDlc[] = "First-Dlc";
constexpr char kSecondDlc[] = "Second-Dlc";
}  // namespace

class DlcServiceDBusAdaptorTest : public testing::Test {
 public:
  DlcServiceDBusAdaptorTest() {
    // Initialize DLC path.
    CHECK(scoped_temp_dir_.CreateUniqueTempDir());
    manifest_path_ = scoped_temp_dir_.GetPath().Append("rootfs");
    content_path_ = scoped_temp_dir_.GetPath().Append("stateful");
    base::CreateDirectory(manifest_path_);
    base::CreateDirectory(content_path_);
    // Create DLC sub-folders.
    base::CreateDirectory(manifest_path_.Append(kFirstDlc));
    base::CreateDirectory(manifest_path_.Append(kSecondDlc));
    base::CreateDirectory(content_path_.Append(kFirstDlc));
  }

 protected:
  base::ScopedTempDir scoped_temp_dir_;

  base::FilePath manifest_path_;
  base::FilePath content_path_;
};

TEST_F(DlcServiceDBusAdaptorTest, LoadDlcImages) {
  auto mock_image_loader_proxy =
      std::make_unique<org::chromium::ImageLoaderInterfaceProxyMock>();

  EXPECT_CALL(
      *(mock_image_loader_proxy.get()),
      LoadDlcImage(kFirstDlc, testing::_, testing::_, testing::_, testing::_))
      .Times(1);
  EXPECT_CALL(
      *(mock_image_loader_proxy.get()),
      LoadDlcImage(kSecondDlc, testing::_, testing::_, testing::_, testing::_))
      .Times(0);

  DlcServiceDBusAdaptor dlc_service_dbus_adaptor(
      std::move(mock_image_loader_proxy), manifest_path_, content_path_);
}

}  // namespace dlcservice
