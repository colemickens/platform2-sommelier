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

constexpr char kManifestName[] = "imageloader.json";
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
    base::FilePath testdata_dir =
        base::FilePath(getenv("SRC")).Append("testdata");
    base::CopyFile(testdata_dir.Append(kFirstDlc).Append(kManifestName),
                   manifest_path_.Append(kFirstDlc).Append(kManifestName));
    base::CopyFile(testdata_dir.Append(kSecondDlc).Append(kManifestName),
                   manifest_path_.Append(kSecondDlc).Append(kManifestName));

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

class FakeShutdownDelegate : public DlcServiceDBusAdaptor::ShutdownDelegate {
 public:
  FakeShutdownDelegate() : shutdown_scheduled_(false) {}
  bool is_shutdown_scheduled() const { return shutdown_scheduled_; }
  void CancelShutdown() override { shutdown_scheduled_ = false; }
  void ScheduleShutdown() override { shutdown_scheduled_ = true; }

 private:
  bool shutdown_scheduled_;

  DISALLOW_COPY_AND_ASSIGN(FakeShutdownDelegate);
};

TEST_F(DlcServiceDBusAdaptorTest, GetInstalledTest) {
  auto mock_boot_device = std::make_unique<MockBootDevice>();
  ON_CALL(*(mock_boot_device.get()), GetBootDevice())
      .WillByDefault(testing::Return("/dev/sdb5"));
  ON_CALL(*(mock_boot_device.get()), IsRemovableDevice(testing::_))
      .WillByDefault(testing::Return(false));
  FakeShutdownDelegate fake_shutdown_delegate;

  DlcServiceDBusAdaptor dlc_service_dbus_adaptor(
      std::make_unique<org::chromium::ImageLoaderInterfaceProxyMock>(),
      std::make_unique<org::chromium::UpdateEngineInterfaceProxyMock>(),
      std::make_unique<BootSlot>(std::move(mock_boot_device)), manifest_path_,
      content_path_, &fake_shutdown_delegate);

  std::string dlc_module_list_str;
  EXPECT_TRUE(
      dlc_service_dbus_adaptor.GetInstalled(nullptr, &dlc_module_list_str));
  DlcModuleList dlc_module_list;
  EXPECT_TRUE(dlc_module_list.ParseFromString(dlc_module_list_str));
  EXPECT_EQ(dlc_module_list.dlc_module_infos_size(), 1);
  EXPECT_EQ(dlc_module_list.dlc_module_infos(0).dlc_id(), kFirstDlc);
  EXPECT_TRUE(fake_shutdown_delegate.is_shutdown_scheduled());
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
  FakeShutdownDelegate fake_shutdown_delegate;

  DlcServiceDBusAdaptor dlc_service_dbus_adaptor(
      std::move(mock_image_loader_proxy), std::move(mock_update_engine_proxy),
      std::make_unique<BootSlot>(std::make_unique<MockBootDevice>()),
      manifest_path_, content_path_, &fake_shutdown_delegate);
  EXPECT_TRUE(dlc_service_dbus_adaptor.Uninstall(nullptr, kFirstDlc));
  EXPECT_FALSE(base::PathExists(content_path_.Append(kFirstDlc)));
  EXPECT_TRUE(fake_shutdown_delegate.is_shutdown_scheduled());
}

TEST_F(DlcServiceDBusAdaptorTest, UninstallFailureTest) {
  FakeShutdownDelegate fake_shutdown_delegate;

  DlcServiceDBusAdaptor dlc_service_dbus_adaptor(
      std::make_unique<org::chromium::ImageLoaderInterfaceProxyMock>(),
      std::make_unique<org::chromium::UpdateEngineInterfaceProxyMock>(),
      std::make_unique<BootSlot>(std::make_unique<MockBootDevice>()),
      manifest_path_, content_path_, &fake_shutdown_delegate);

  EXPECT_FALSE(dlc_service_dbus_adaptor.Uninstall(nullptr, kSecondDlc));
  EXPECT_TRUE(fake_shutdown_delegate.is_shutdown_scheduled());
}

TEST_F(DlcServiceDBusAdaptorTest, UninstallUnmountFailureTest) {
  auto mock_image_loader_proxy =
      std::make_unique<org::chromium::ImageLoaderInterfaceProxyMock>();
  ON_CALL(*(mock_image_loader_proxy.get()),
          UnloadDlcImage(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(
          DoAll(testing::SetArgPointee<1>(false), testing::Return(true)));
  FakeShutdownDelegate fake_shutdown_delegate;

  DlcServiceDBusAdaptor dlc_service_dbus_adaptor(
      std::move(mock_image_loader_proxy),
      std::make_unique<org::chromium::UpdateEngineInterfaceProxyMock>(),
      std::make_unique<BootSlot>(std::make_unique<MockBootDevice>()),
      manifest_path_, content_path_, &fake_shutdown_delegate);
  EXPECT_FALSE(dlc_service_dbus_adaptor.Uninstall(nullptr, kFirstDlc));
  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
  EXPECT_TRUE(fake_shutdown_delegate.is_shutdown_scheduled());
}

TEST_F(DlcServiceDBusAdaptorTest, InstallTest) {
  auto mock_boot_device = std::make_unique<MockBootDevice>();
  ON_CALL(*(mock_boot_device.get()), GetBootDevice())
      .WillByDefault(testing::Return("/dev/sdb5"));
  ON_CALL(*(mock_boot_device.get()), IsRemovableDevice(testing::_))
      .WillByDefault(testing::Return(false));
  auto mock_image_loader_proxy =
      std::make_unique<org::chromium::ImageLoaderInterfaceProxyMock>();
  std::string mount_path_expected = "/run/imageloader/dlc-id";
  ON_CALL(
      *(mock_image_loader_proxy.get()),
      LoadDlcImage(testing::_, testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(DoAll(testing::SetArgPointee<2>(mount_path_expected),
                           testing::Return(true)));
  auto mock_update_engine_proxy =
      std::make_unique<org::chromium::UpdateEngineInterfaceProxyMock>();
  ON_CALL(*(mock_update_engine_proxy.get()),
          AttemptInstall(testing::_, testing::_, testing::_))
      .WillByDefault(testing::Return(true));
  std::string update_status_idle = update_engine::kUpdateStatusIdle;
  ON_CALL(*(mock_update_engine_proxy.get()),
          GetStatus(testing::_, testing::_, testing::_, testing::_, testing::_,
                    testing::_, testing::_))
      .WillByDefault(DoAll(testing::SetArgPointee<2>(update_status_idle),
                           testing::Return(true)));
  FakeShutdownDelegate fake_shutdown_delegate;

  DlcServiceDBusAdaptor dlc_service_dbus_adaptor(
      std::move(mock_image_loader_proxy), std::move(mock_update_engine_proxy),
      std::make_unique<BootSlot>(std::move(mock_boot_device)), manifest_path_,
      content_path_, &fake_shutdown_delegate);
  std::string dlc_root_path;
  EXPECT_TRUE(
      dlc_service_dbus_adaptor.Install(nullptr, kSecondDlc, &dlc_root_path));
  EXPECT_EQ(dlc_root_path,
            utils::GetDlcRootInModulePath(base::FilePath(mount_path_expected))
                .value());

  constexpr int expected_permissions = 0755;
  int permissions;
  base::FilePath module_path =
      utils::GetDlcModulePath(content_path_, kSecondDlc);
  base::GetPosixFilePermissions(module_path, &permissions);
  EXPECT_EQ(permissions, expected_permissions);
  base::FilePath image_a_path =
      utils::GetDlcModuleImagePath(content_path_, kSecondDlc, 0);
  base::GetPosixFilePermissions(image_a_path.DirName(), &permissions);
  EXPECT_EQ(permissions, expected_permissions);
  base::FilePath image_b_path =
      utils::GetDlcModuleImagePath(content_path_, kSecondDlc, 1);
  base::GetPosixFilePermissions(image_b_path.DirName(), &permissions);
  EXPECT_EQ(permissions, expected_permissions);
  EXPECT_TRUE(fake_shutdown_delegate.is_shutdown_scheduled());
}

}  // namespace dlcservice
