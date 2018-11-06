// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <dlcservice/proto_bindings/dlcservice.pb.h>
#include <gtest/gtest.h>
#include <imageloader/dbus-proxy-mocks.h>
#include <update_engine/dbus-constants.h>
#include <update_engine/dbus-proxy-mocks.h>

#include "dlcservice/boot_slot.h"
#include "dlcservice/dlc_service_dbus_adaptor.h"
#include "dlcservice/mock_boot_device.h"
#include "dlcservice/utils.h"

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

    // Create DLC manifest sub-directories.
    base::CreateDirectory(manifest_path_.Append(kFirstDlc));
    base::CreateDirectory(manifest_path_.Append(kSecondDlc));

    // Create DLC content sub-directories.
    base::FilePath image_a_path =
        utils::GetDlcModuleImagePath(content_path_, kFirstDlc, 0);
    base::CreateDirectory(image_a_path.DirName());
    // Create empty image files.
    base::File image_a(image_a_path,
                       base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ);
    base::FilePath image_b_path =
        utils::GetDlcModuleImagePath(content_path_, kFirstDlc, 1);
    base::CreateDirectory(image_b_path.DirName());
    base::File image_b(image_b_path,
                       base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ);
  }

 protected:
  base::ScopedTempDir scoped_temp_dir_;

  base::FilePath manifest_path_;
  base::FilePath content_path_;
};

TEST_F(DlcServiceDBusAdaptorTest, InitTest) {
  auto mock_image_loader_proxy =
      std::make_unique<org::chromium::ImageLoaderInterfaceProxyMock>();
  auto mock_boot_device = std::make_unique<MockBootDevice>();
  ON_CALL(*(mock_boot_device.get()), GetBootDevice())
      .WillByDefault(testing::Return("/dev/sdb5"));
  ON_CALL(*(mock_boot_device.get()), IsRemovableDevice(testing::_))
      .WillByDefault(testing::Return(false));

  EXPECT_CALL(
      *(mock_image_loader_proxy.get()),
      LoadDlcImage(kFirstDlc, "Dlc-B", testing::_, testing::_, testing::_))
      .Times(1);
  EXPECT_CALL(
      *(mock_image_loader_proxy.get()),
      LoadDlcImage(kSecondDlc, testing::_, testing::_, testing::_, testing::_))
      .Times(0);

  DlcServiceDBusAdaptor dlc_service_dbus_adaptor(
      std::move(mock_image_loader_proxy),
      std::make_unique<org::chromium::UpdateEngineInterfaceProxyMock>(),
      std::make_unique<BootSlot>(std::move(mock_boot_device)), manifest_path_,
      content_path_);
}

TEST_F(DlcServiceDBusAdaptorTest, GetInstalledTest) {
  auto mock_boot_device = std::make_unique<MockBootDevice>();
  ON_CALL(*(mock_boot_device.get()), GetBootDevice())
      .WillByDefault(testing::Return("/dev/sdb5"));
  ON_CALL(*(mock_boot_device.get()), IsRemovableDevice(testing::_))
      .WillByDefault(testing::Return(false));

  DlcServiceDBusAdaptor dlc_service_dbus_adaptor(
      std::make_unique<org::chromium::ImageLoaderInterfaceProxyMock>(),
      std::make_unique<org::chromium::UpdateEngineInterfaceProxyMock>(),
      std::make_unique<BootSlot>(std::move(mock_boot_device)), manifest_path_,
      content_path_);

  std::string dlc_module_list_str;
  EXPECT_TRUE(
      dlc_service_dbus_adaptor.GetInstalled(nullptr, &dlc_module_list_str));
  DlcModuleList dlc_module_list;
  EXPECT_TRUE(dlc_module_list.ParseFromString(dlc_module_list_str));
  EXPECT_EQ(dlc_module_list.dlc_module_infos_size(), 1);
  EXPECT_EQ(dlc_module_list.dlc_module_infos(0).dlc_id(), kFirstDlc);
}

TEST_F(DlcServiceDBusAdaptorTest, UninstallTest) {
  auto mock_image_loader_proxy =
      std::make_unique<org::chromium::ImageLoaderInterfaceProxyMock>();
  ON_CALL(*(mock_image_loader_proxy.get()),
          UnloadDlcImage(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(
          DoAll(testing::SetArgPointee<1>(true), testing::Return(true)));
  auto mock_update_engine_proxy =
      std::make_unique<org::chromium::UpdateEngineInterfaceProxyMock>();
  std::string update_status_idle = update_engine::kUpdateStatusIdle;
  ON_CALL(*(mock_update_engine_proxy.get()),
          GetStatus(testing::_, testing::_, testing::_, testing::_, testing::_,
                    testing::_, testing::_))
      .WillByDefault(DoAll(testing::SetArgPointee<2>(update_status_idle),
                           testing::Return(true)));

  DlcServiceDBusAdaptor dlc_service_dbus_adaptor(
      std::move(mock_image_loader_proxy), std::move(mock_update_engine_proxy),
      std::make_unique<BootSlot>(std::make_unique<MockBootDevice>()),
      manifest_path_, content_path_);
  EXPECT_TRUE(dlc_service_dbus_adaptor.Uninstall(nullptr, kFirstDlc));
  EXPECT_FALSE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceDBusAdaptorTest, UninstallFailureTest) {
  DlcServiceDBusAdaptor dlc_service_dbus_adaptor(
      std::make_unique<org::chromium::ImageLoaderInterfaceProxyMock>(),
      std::make_unique<org::chromium::UpdateEngineInterfaceProxyMock>(),
      std::make_unique<BootSlot>(std::make_unique<MockBootDevice>()),
      manifest_path_, content_path_);

  EXPECT_FALSE(dlc_service_dbus_adaptor.Uninstall(nullptr, kSecondDlc));
}

TEST_F(DlcServiceDBusAdaptorTest, UninstallUnmountFailureTest) {
  auto mock_image_loader_proxy =
      std::make_unique<org::chromium::ImageLoaderInterfaceProxyMock>();
  ON_CALL(*(mock_image_loader_proxy.get()),
          UnloadDlcImage(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(
          DoAll(testing::SetArgPointee<1>(false), testing::Return(true)));

  DlcServiceDBusAdaptor dlc_service_dbus_adaptor(
      std::move(mock_image_loader_proxy),
      std::make_unique<org::chromium::UpdateEngineInterfaceProxyMock>(),
      std::make_unique<BootSlot>(std::make_unique<MockBootDevice>()),
      manifest_path_, content_path_);
  EXPECT_FALSE(dlc_service_dbus_adaptor.Uninstall(nullptr, kFirstDlc));
  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
}

}  // namespace dlcservice
