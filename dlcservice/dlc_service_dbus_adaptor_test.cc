// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/run_loop.h>
#include <dlcservice/proto_bindings/dlcservice.pb.h>
#include <update_engine/proto_bindings/update_engine.pb.h>
#include <gtest/gtest.h>
#include <imageloader/dbus-proxy-mocks.h>
#include <update_engine/dbus-constants.h>
#include <update_engine/dbus-proxy-mocks.h>

#include "dlcservice/boot_slot.h"
#include "dlcservice/dlc_service_dbus_adaptor.h"
#include "dlcservice/mock_boot_device.h"
#include "dlcservice/utils.h"

using std::move;
using std::string;
using testing::_;
using testing::Return;
using testing::SetArgPointee;

namespace dlcservice {

namespace {

constexpr char kFirstDlc[] = "First-Dlc";
constexpr char kSecondDlc[] = "Second-Dlc";
constexpr char kThirdDlc[] = "Third-Dlc";
constexpr char kPackage[] = "Package";

constexpr char kManifestName[] = "imageloader.json";

MATCHER_P(ProtoHasUrl,
          url,
          string("The protobuf provided does not have url: ") + url) {
  return url == arg.omaha_url();
}

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
    base::FilePath testdata_dir =
        base::FilePath(getenv("SRC")).Append("testdata");

    // Create DLC manifest sub-directories.
    for (auto&& id : {kFirstDlc, kSecondDlc, kThirdDlc}) {
      base::CreateDirectory(manifest_path_.Append(id).Append(kPackage));
      base::CopyFile(
          testdata_dir.Append(id).Append(kPackage).Append(kManifestName),
          manifest_path_.Append(id).Append(kPackage).Append(kManifestName));
    }

    // Create DLC content sub-directories and empty images.
    base::FilePath image_a_path =
        utils::GetDlcModuleImagePath(content_path_, kFirstDlc, kPackage, 0);
    base::CreateDirectory(image_a_path.DirName());
    base::File image_a(image_a_path,
                       base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ);

    base::FilePath image_b_path =
        utils::GetDlcModuleImagePath(content_path_, kFirstDlc, kPackage, 1);
    base::CreateDirectory(image_b_path.DirName());
    base::File image_b(image_b_path,
                       base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ);

    // Create mocks with default behaviors.
    mock_boot_device_ = std::make_unique<MockBootDevice>();
    ON_CALL(*(mock_boot_device_.get()), GetBootDevice())
        .WillByDefault(Return("/dev/sdb5"));
    ON_CALL(*(mock_boot_device_.get()), IsRemovableDevice(_))
        .WillByDefault(Return(false));

    mock_image_loader_proxy_ =
        std::make_unique<org::chromium::ImageLoaderInterfaceProxyMock>();
    mock_image_loader_proxy_ptr_ = mock_image_loader_proxy_.get();
    ON_CALL(*mock_image_loader_proxy_ptr_, LoadDlcImage(_, _, _, _, _, _))
        .WillByDefault(
            DoAll(SetArgPointee<3>("/good/mount/path"), Return(true)));
    ON_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
        .WillByDefault(DoAll(SetArgPointee<2>(true), Return(true)));

    mock_update_engine_proxy_ =
        std::make_unique<org::chromium::UpdateEngineInterfaceProxyMock>();
    mock_update_engine_proxy_ptr_ = mock_update_engine_proxy_.get();
    ON_CALL(*mock_update_engine_proxy_ptr_, AttemptInstall(_, _, _))
        .WillByDefault(Return(true));
    ON_CALL(*mock_update_engine_proxy_ptr_, GetStatus(_, _, _, _, _, _, _))
        .WillByDefault(DoAll(SetArgPointee<2>(update_engine::kUpdateStatusIdle),
                             Return(true)));

    // Use the mocks to create |DlcServiceDBusAdaptor|.
    dlc_service_dbus_adaptor_ = std::make_unique<DlcServiceDBusAdaptor>(
        move(mock_image_loader_proxy_), move(mock_update_engine_proxy_),
        std::make_unique<BootSlot>(move(mock_boot_device_)), manifest_path_,
        content_path_);
  }

  void SetMountPath(const string& mount_path_expected);

 protected:
  base::ScopedTempDir scoped_temp_dir_;

  base::FilePath manifest_path_;
  base::FilePath content_path_;

  std::unique_ptr<MockBootDevice> mock_boot_device_;
  std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyMock>
      mock_image_loader_proxy_;
  org::chromium::ImageLoaderInterfaceProxyMock* mock_image_loader_proxy_ptr_;
  std::unique_ptr<org::chromium::UpdateEngineInterfaceProxyMock>
      mock_update_engine_proxy_;
  org::chromium::UpdateEngineInterfaceProxyMock* mock_update_engine_proxy_ptr_;
  std::unique_ptr<DlcServiceDBusAdaptor> dlc_service_dbus_adaptor_;

  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DlcServiceDBusAdaptorTest);
};

void DlcServiceDBusAdaptorTest::SetMountPath(
    const string& mount_path_expected) {
  ON_CALL(*mock_image_loader_proxy_ptr_, LoadDlcImage(_, _, _, _, _, _))
      .WillByDefault(
          DoAll(SetArgPointee<3>(mount_path_expected), Return(true)));
}

TEST_F(DlcServiceDBusAdaptorTest, GetInstalledTest) {
  DlcModuleList dlc_module_list;
  EXPECT_TRUE(
      dlc_service_dbus_adaptor_->GetInstalled(nullptr, &dlc_module_list));
  EXPECT_EQ(dlc_module_list.dlc_module_infos_size(), 1);
  EXPECT_EQ(dlc_module_list.dlc_module_infos(0).dlc_id(), kFirstDlc);
}

TEST_F(DlcServiceDBusAdaptorTest, UninstallTest) {
  ON_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
      .WillByDefault(DoAll(SetArgPointee<2>(true), Return(true)));
  string update_status_idle = update_engine::kUpdateStatusIdle;

  EXPECT_TRUE(dlc_service_dbus_adaptor_->Uninstall(nullptr, kFirstDlc));
  EXPECT_FALSE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceDBusAdaptorTest, UninstallFailureTest) {
  EXPECT_FALSE(dlc_service_dbus_adaptor_->Uninstall(nullptr, kSecondDlc));
}

TEST_F(DlcServiceDBusAdaptorTest, UninstallUnmountFailureTest) {
  ON_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
      .WillByDefault(DoAll(SetArgPointee<2>(false), Return(true)));

  EXPECT_FALSE(dlc_service_dbus_adaptor_->Uninstall(nullptr, kFirstDlc));
  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceDBusAdaptorTest, UninstallImageLoaderFailureTest) {
  EXPECT_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
      .WillOnce(Return(false));

  // |ImageLoader| not avaiable.
  EXPECT_FALSE(dlc_service_dbus_adaptor_->Uninstall(nullptr, kFirstDlc));
  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceDBusAdaptorTest, InstallEmptyDlcModuleListFailsTest) {
  EXPECT_FALSE(dlc_service_dbus_adaptor_->Install(nullptr, {}));
}

TEST_F(DlcServiceDBusAdaptorTest, InstallTest) {
  const string omaha_url_default = "";
  DlcModuleList dlc_module_list;
  DlcModuleInfo* dlc_info = dlc_module_list.add_dlc_module_infos();
  dlc_info->set_dlc_id(kSecondDlc);
  dlc_module_list.set_omaha_url(omaha_url_default);

  SetMountPath("/run/imageloader/dlc-id/package");
  EXPECT_CALL(*mock_update_engine_proxy_ptr_,
              AttemptInstall(ProtoHasUrl(omaha_url_default), _, _))
      .Times(1);

  EXPECT_TRUE(dlc_service_dbus_adaptor_->Install(nullptr, dlc_module_list));
  base::RunLoop().RunUntilIdle();

  constexpr int expected_permissions = 0755;
  int permissions;
  base::FilePath module_path =
      utils::GetDlcModulePackagePath(content_path_, kSecondDlc, kPackage);
  base::GetPosixFilePermissions(module_path, &permissions);
  EXPECT_EQ(permissions, expected_permissions);
  base::FilePath image_a_path =
      utils::GetDlcModuleImagePath(content_path_, kSecondDlc, kPackage, 0);
  base::GetPosixFilePermissions(image_a_path.DirName(), &permissions);
  EXPECT_EQ(permissions, expected_permissions);
  base::FilePath image_b_path =
      utils::GetDlcModuleImagePath(content_path_, kSecondDlc, kPackage, 1);
  base::GetPosixFilePermissions(image_b_path.DirName(), &permissions);
  EXPECT_EQ(permissions, expected_permissions);
}

TEST_F(DlcServiceDBusAdaptorTest, InstallFailureInstalledSticky) {
  const string omaha_url_default = "";
  DlcModuleList dlc_module_list;
  DlcModuleInfo* dlc_info = dlc_module_list.add_dlc_module_infos();
  dlc_info->set_dlc_id(kFirstDlc);

  SetMountPath("/run/imageloader/dlc-id/package");
  EXPECT_CALL(*mock_update_engine_proxy_ptr_,
              AttemptInstall(ProtoHasUrl(omaha_url_default), _, _))
      .Times(0);

  EXPECT_FALSE(dlc_service_dbus_adaptor_->Install(nullptr, dlc_module_list));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceDBusAdaptorTest, InstallFailureInstallingCleanup) {
  const string omaha_url_default = "";
  DlcModuleList dlc_module_list;
  for (const string& dlc_id : {kSecondDlc, kSecondDlc}) {
    DlcModuleInfo* dlc_info = dlc_module_list.add_dlc_module_infos();
    dlc_info->set_dlc_id(dlc_id);
  }

  SetMountPath("/run/imageloader/dlc-id/package");
  EXPECT_CALL(*mock_update_engine_proxy_ptr_,
              AttemptInstall(ProtoHasUrl(omaha_url_default), _, _))
      .Times(0);

  EXPECT_FALSE(dlc_service_dbus_adaptor_->Install(nullptr, dlc_module_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
  EXPECT_FALSE(base::PathExists(content_path_.Append(kSecondDlc)));
}

TEST_F(DlcServiceDBusAdaptorTest, InstallUrlTest) {
  const string omaha_url_override = "http://random.url";
  DlcModuleList dlc_module_list;
  DlcModuleInfo* dlc_info = dlc_module_list.add_dlc_module_infos();
  dlc_info->set_dlc_id(kSecondDlc);
  dlc_module_list.set_omaha_url(omaha_url_override);

  ON_CALL(*mock_update_engine_proxy_ptr_, AttemptInstall(_, _, _))
      .WillByDefault(Return(true));
  string update_status_idle = update_engine::kUpdateStatusIdle;
  ON_CALL(*mock_update_engine_proxy_ptr_, GetStatus(_, _, _, _, _, _, _))
      .WillByDefault(DoAll(SetArgPointee<2>(update_status_idle), Return(true)));
  EXPECT_CALL(*mock_update_engine_proxy_ptr_,
              AttemptInstall(ProtoHasUrl(omaha_url_override), _, _))
      .Times(1);

  dlc_service_dbus_adaptor_->Install(nullptr, dlc_module_list);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DlcServiceDBusAdaptorTest, OnStatusUpdateSignalTest) {
  DlcModuleList dlc_module_list;
  for (const string& dlc_id : {kSecondDlc, kThirdDlc}) {
    DlcModuleInfo* dlc_info = dlc_module_list.add_dlc_module_infos();
    dlc_info->set_dlc_id(dlc_id);
  }

  EXPECT_TRUE(dlc_service_dbus_adaptor_->Install(nullptr, dlc_module_list));

  EXPECT_CALL(*mock_image_loader_proxy_ptr_, LoadDlcImage(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>("/some/mount"), Return(true)))
      .WillOnce(DoAll(SetArgPointee<3>(""), Return(true)));
  EXPECT_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
      .Times(2);

  for (const string& dlc_id : {kSecondDlc, kThirdDlc})
    EXPECT_TRUE(base::PathExists(content_path_.Append(dlc_id)));

  dlc_service_dbus_adaptor_->OnStatusUpdateSignal(
      0, 0., update_engine::kUpdateStatusIdle, "", 0);

  for (const string& dlc_id : {kSecondDlc, kThirdDlc})
    EXPECT_FALSE(base::PathExists(content_path_.Append(dlc_id)));
}

}  // namespace dlcservice
