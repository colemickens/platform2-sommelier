// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/optional.h>
#include <base/run_loop.h>
#include <brillo/message_loops/base_message_loop.h>
#include <brillo/message_loops/message_loop_utils.h>
#include <dlcservice/proto_bindings/dlcservice.pb.h>
#include <update_engine/proto_bindings/update_engine.pb.h>
#include <gtest/gtest.h>
#include <imageloader/dbus-proxy-mocks.h>
#include <update_engine/dbus-constants.h>
#include <update_engine/dbus-proxy-mocks.h>

#include "dlcservice/boot_slot.h"
#include "dlcservice/dlc_service.h"
#include "dlcservice/mock_boot_device.h"
#include "dlcservice/utils.h"

using std::move;
using std::string;
using std::vector;
using testing::_;
using testing::Return;
using testing::SetArgPointee;
using update_engine::Operation;
using update_engine::StatusResult;

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

DlcModuleList CreateDlcModuleList(const vector<string>& ids,
                                  const string& omaha_url = "") {
  DlcModuleList dlc_module_list;
  dlc_module_list.set_omaha_url(omaha_url);
  for (const string& id : ids) {
    DlcModuleInfo* dlc_info = dlc_module_list.add_dlc_module_infos();
    dlc_info->set_dlc_id(id);
  }
  return dlc_module_list;
}

class DlcServiceTestObserver : public DlcService::Observer {
 public:
  void SendInstallStatus(const InstallStatus& install_status) override {
    install_status_.emplace(install_status);
  }

  bool IsSendInstallStatusCalled() { return install_status_.has_value(); }

  InstallStatus GetInstallStatus() {
    EXPECT_TRUE(install_status_.has_value())
        << "SendInstallStatus() was not called.";
    base::Optional<InstallStatus> tmp;
    tmp.swap(install_status_);
    return *tmp;
  }

 private:
  base::Optional<InstallStatus> install_status_;
};

}  // namespace

class DlcServiceTest : public testing::Test {
 public:
  DlcServiceTest() {
    loop_.SetAsCurrent();

    // Initialize DLC path.
    CHECK(scoped_temp_dir_.CreateUniqueTempDir());
    manifest_path_ = scoped_temp_dir_.GetPath().Append("rootfs");
    content_path_ = scoped_temp_dir_.GetPath().Append("stateful");
    mount_path_ = scoped_temp_dir_.GetPath().Append("mount");
    base::FilePath mount_root_path = mount_path_.Append("root");
    base::CreateDirectory(manifest_path_);
    base::CreateDirectory(content_path_);
    base::CreateDirectory(mount_root_path);
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
            DoAll(SetArgPointee<3>(mount_path_.value()), Return(true)));
    ON_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
        .WillByDefault(DoAll(SetArgPointee<2>(true), Return(true)));

    mock_update_engine_proxy_ =
        std::make_unique<org::chromium::UpdateEngineInterfaceProxyMock>();
    mock_update_engine_proxy_ptr_ = mock_update_engine_proxy_.get();
    ON_CALL(*mock_update_engine_proxy_ptr_, AttemptInstall(_, _, _))
        .WillByDefault(Return(true));
    StatusResult status_result;
    status_result.set_current_operation(Operation::IDLE);
    ON_CALL(*mock_update_engine_proxy_ptr_, GetStatusAdvanced(_, _, _))
        .WillByDefault(DoAll(SetArgPointee<0>(status_result), Return(true)));

    // Use the mocks to create |DlcService|.
    dlc_service_ = std::make_unique<DlcService>(
        move(mock_image_loader_proxy_), move(mock_update_engine_proxy_),
        std::make_unique<BootSlot>(move(mock_boot_device_)), manifest_path_,
        content_path_);

    dlc_service_test_observer_ = std::make_unique<DlcServiceTestObserver>();
    dlc_service_->AddObserver(dlc_service_test_observer_.get());
  }

  void SetUp() override { dlc_service_->LoadDlcModuleImages(); }

  void SetMountPath(const string& mount_path_expected) {
    ON_CALL(*mock_image_loader_proxy_ptr_, LoadDlcImage(_, _, _, _, _, _))
        .WillByDefault(
            DoAll(SetArgPointee<3>(mount_path_expected), Return(true)));
  }

 protected:
  base::MessageLoopForIO base_loop_;
  brillo::BaseMessageLoop loop_{&base_loop_};

  base::ScopedTempDir scoped_temp_dir_;

  base::FilePath manifest_path_;
  base::FilePath content_path_;
  base::FilePath mount_path_;

  std::unique_ptr<MockBootDevice> mock_boot_device_;
  std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyMock>
      mock_image_loader_proxy_;
  org::chromium::ImageLoaderInterfaceProxyMock* mock_image_loader_proxy_ptr_;
  std::unique_ptr<org::chromium::UpdateEngineInterfaceProxyMock>
      mock_update_engine_proxy_;
  org::chromium::UpdateEngineInterfaceProxyMock* mock_update_engine_proxy_ptr_;
  std::unique_ptr<DlcService> dlc_service_;
  std::unique_ptr<DlcServiceTestObserver> dlc_service_test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DlcServiceTest);
};

class DlcServiceSkipLoadTest : public DlcServiceTest {
 public:
  void SetUp() override {
    // Need this to skip calling |LoadDlcModuleImages()|.
  }
};

TEST_F(DlcServiceSkipLoadTest, StartUpMountSuccessTest) {
  EXPECT_CALL(*mock_image_loader_proxy_ptr_, LoadDlcImage(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>("/good/mount"), Return(true)));

  dlc_service_->LoadDlcModuleImages();

  // Startup with failure to mount.
  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceSkipLoadTest, StartUpMountFailureTest) {
  EXPECT_CALL(*mock_image_loader_proxy_ptr_, LoadDlcImage(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(""), Return(true)));

  dlc_service_->LoadDlcModuleImages();

  // Startup with failure to mount.
  EXPECT_FALSE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceSkipLoadTest, StartUpImageLoaderFailureTest) {
  EXPECT_CALL(*mock_image_loader_proxy_ptr_, LoadDlcImage(_, _, _, _, _, _))
      .WillOnce(Return(false));

  dlc_service_->LoadDlcModuleImages();

  // Startup with image_loader failure.
  EXPECT_FALSE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceTest, GetInstalledTest) {
  DlcModuleList dlc_module_list;
  EXPECT_TRUE(dlc_service_->GetInstalled(&dlc_module_list, nullptr));
  EXPECT_EQ(dlc_module_list.dlc_module_infos_size(), 1);

  DlcModuleInfo dlc_module = dlc_module_list.dlc_module_infos(0);
  EXPECT_EQ(dlc_module.dlc_id(), kFirstDlc);
  EXPECT_FALSE(dlc_module.dlc_root().empty());
}

TEST_F(DlcServiceTest, UninstallTest) {
  ON_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
      .WillByDefault(DoAll(SetArgPointee<2>(true), Return(true)));

  EXPECT_TRUE(dlc_service_->Uninstall(kFirstDlc, nullptr));
  EXPECT_FALSE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceTest, UninstallNotInstalledIsValidTest) {
  EXPECT_TRUE(dlc_service_->Uninstall(kSecondDlc, nullptr));
}

TEST_F(DlcServiceTest, UninstallUnmountFailureTest) {
  ON_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
      .WillByDefault(DoAll(SetArgPointee<2>(false), Return(true)));

  EXPECT_FALSE(dlc_service_->Uninstall(kFirstDlc, nullptr));
  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceTest, UninstallImageLoaderFailureTest) {
  EXPECT_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
      .WillOnce(Return(false));

  // |ImageLoader| not avaiable.
  EXPECT_FALSE(dlc_service_->Uninstall(kFirstDlc, nullptr));
  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceTest, UninstallUpdateEngineBusyFailureTest) {
  StatusResult status_result;
  status_result.set_current_operation(Operation::CHECKING_FOR_UPDATE);
  ON_CALL(*mock_update_engine_proxy_ptr_, GetStatusAdvanced(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<0>(status_result), Return(true)));

  EXPECT_FALSE(dlc_service_->Uninstall(kFirstDlc, nullptr));
  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceTest, UninstallUpdatedNeedRebootSuccessTest) {
  StatusResult status_result;
  status_result.set_current_operation(Operation::UPDATED_NEED_REBOOT);
  ON_CALL(*mock_update_engine_proxy_ptr_, GetStatusAdvanced(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<0>(status_result), Return(true)));

  EXPECT_TRUE(dlc_service_->Uninstall(kFirstDlc, nullptr));
  EXPECT_FALSE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceTest, InstallEmptyDlcModuleListFailsTest) {
  EXPECT_FALSE(dlc_service_->Install({}, nullptr));
}

TEST_F(DlcServiceTest, InstallTest) {
  const string omaha_url_default = "";
  DlcModuleList dlc_module_list =
      CreateDlcModuleList({kSecondDlc}, omaha_url_default);

  SetMountPath(mount_path_.value());
  EXPECT_CALL(*mock_update_engine_proxy_ptr_,
              AttemptInstall(ProtoHasUrl(omaha_url_default), _, _))
      .Times(1);

  EXPECT_TRUE(dlc_service_->Install(dlc_module_list, nullptr));

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

TEST_F(DlcServiceTest, InstallAlreadyInstalledValid) {
  const string omaha_url_default = "";
  DlcModuleList dlc_module_list =
      CreateDlcModuleList({kFirstDlc}, omaha_url_default);

  SetMountPath(mount_path_.value());
  EXPECT_CALL(*mock_update_engine_proxy_ptr_,
              AttemptInstall(ProtoHasUrl(omaha_url_default), _, _))
      .Times(0);

  EXPECT_TRUE(dlc_service_->Install(dlc_module_list, nullptr));
  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
}

TEST_F(DlcServiceTest, InstallDuplicatesFail) {
  const string omaha_url_default = "";
  DlcModuleList dlc_module_list =
      CreateDlcModuleList({kSecondDlc, kSecondDlc}, omaha_url_default);

  SetMountPath(mount_path_.value());
  EXPECT_CALL(*mock_update_engine_proxy_ptr_,
              AttemptInstall(ProtoHasUrl(omaha_url_default), _, _))
      .Times(0);

  EXPECT_FALSE(dlc_service_->Install(dlc_module_list, nullptr));

  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
  EXPECT_FALSE(base::PathExists(content_path_.Append(kSecondDlc)));
}

TEST_F(DlcServiceTest, InstallAlreadyInstalledAndDuplicatesFail) {
  const string omaha_url_default = "";
  DlcModuleList dlc_module_list = CreateDlcModuleList(
      {kFirstDlc, kSecondDlc, kSecondDlc}, omaha_url_default);

  SetMountPath(mount_path_.value());
  EXPECT_CALL(*mock_update_engine_proxy_ptr_,
              AttemptInstall(ProtoHasUrl(omaha_url_default), _, _))
      .Times(0);

  EXPECT_FALSE(dlc_service_->Install(dlc_module_list, nullptr));

  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
  EXPECT_FALSE(base::PathExists(content_path_.Append(kSecondDlc)));
}

TEST_F(DlcServiceTest, InstallFailureCleansUp) {
  const string omaha_url_default = "";
  DlcModuleList dlc_module_list =
      CreateDlcModuleList({kSecondDlc, kThirdDlc}, omaha_url_default);

  SetMountPath(mount_path_.value());
  EXPECT_CALL(*mock_update_engine_proxy_ptr_,
              AttemptInstall(ProtoHasUrl(omaha_url_default), _, _))
      .WillOnce(Return(false));

  EXPECT_FALSE(dlc_service_->Install(dlc_module_list, nullptr));

  EXPECT_FALSE(base::PathExists(content_path_.Append(kSecondDlc)));
  EXPECT_FALSE(base::PathExists(content_path_.Append(kThirdDlc)));
}

TEST_F(DlcServiceTest, InstallUrlTest) {
  const string omaha_url_override = "http://random.url";
  DlcModuleList dlc_module_list =
      CreateDlcModuleList({kSecondDlc}, omaha_url_override);

  ON_CALL(*mock_update_engine_proxy_ptr_, AttemptInstall(_, _, _))
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_update_engine_proxy_ptr_,
              AttemptInstall(ProtoHasUrl(omaha_url_override), _, _))
      .Times(1);

  dlc_service_->Install(dlc_module_list, nullptr);
}

TEST_F(DlcServiceTest, OnStatusUpdateAdvancedSignalDlcRootTest) {
  const vector<string>& dlc_ids = {kSecondDlc, kThirdDlc};
  DlcModuleList dlc_module_list = CreateDlcModuleList(dlc_ids);

  EXPECT_TRUE(dlc_service_->Install(dlc_module_list, nullptr));

  EXPECT_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
      .Times(0);

  for (const string& dlc_id : dlc_ids)
    EXPECT_TRUE(base::PathExists(content_path_.Append(dlc_id)));

  StatusResult status_result;
  status_result.set_current_operation(Operation::IDLE);
  status_result.set_is_install(true);
  dlc_service_->OnStatusUpdateAdvancedSignal(status_result);

  for (const string& dlc_id : dlc_ids)
    EXPECT_TRUE(base::PathExists(content_path_.Append(dlc_id)));

  DlcModuleList dlc_module_list_after;
  EXPECT_TRUE(dlc_service_->GetInstalled(&dlc_module_list_after, nullptr));
  EXPECT_EQ(dlc_module_list_after.dlc_module_infos_size(), 3);

  for (const DlcModuleInfo& dlc_module :
       dlc_module_list_after.dlc_module_infos())
    EXPECT_FALSE(dlc_module.dlc_root().empty());
}

TEST_F(DlcServiceTest, OnStatusUpdateAdvancedSignalNoRemountTest) {
  const vector<string>& dlc_ids = {kFirstDlc, kSecondDlc};
  DlcModuleList dlc_module_list = CreateDlcModuleList(dlc_ids);

  EXPECT_TRUE(dlc_service_->Install(dlc_module_list, nullptr));

  EXPECT_CALL(*mock_image_loader_proxy_ptr_, LoadDlcImage(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>("/some/mount"), Return(true)));
  EXPECT_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
      .Times(0);

  for (const string& dlc_id : dlc_ids)
    EXPECT_TRUE(base::PathExists(content_path_.Append(dlc_id)));

  StatusResult status_result;
  status_result.set_current_operation(Operation::IDLE);
  status_result.set_is_install(true);
  dlc_service_->OnStatusUpdateAdvancedSignal(status_result);

  for (const string& dlc_id : dlc_ids)
    EXPECT_TRUE(base::PathExists(content_path_.Append(dlc_id)));
}

TEST_F(DlcServiceTest, OnStatusUpdateAdvancedSignalTest) {
  const vector<string>& dlc_ids = {kSecondDlc, kThirdDlc};
  DlcModuleList dlc_module_list = CreateDlcModuleList(dlc_ids);

  EXPECT_TRUE(dlc_service_->Install(dlc_module_list, nullptr));

  EXPECT_CALL(*mock_image_loader_proxy_ptr_, LoadDlcImage(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>("/some/mount"), Return(true)))
      .WillOnce(DoAll(SetArgPointee<3>(""), Return(true)));
  EXPECT_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
      .Times(2);

  for (const string& dlc_id : dlc_ids)
    EXPECT_TRUE(base::PathExists(content_path_.Append(dlc_id)));

  StatusResult status_result;
  status_result.set_current_operation(Operation::IDLE);
  status_result.set_is_install(true);
  dlc_service_->OnStatusUpdateAdvancedSignal(status_result);

  for (const string& dlc_id : dlc_ids)
    EXPECT_FALSE(base::PathExists(content_path_.Append(dlc_id)));
}

TEST_F(DlcServiceTest, ReportingFailureCleanupTest) {
  const vector<string>& dlc_ids = {kSecondDlc, kThirdDlc};
  DlcModuleList dlc_module_list = CreateDlcModuleList(dlc_ids);

  EXPECT_TRUE(dlc_service_->Install(dlc_module_list, nullptr));

  for (const string& dlc_id : dlc_ids)
    EXPECT_TRUE(base::PathExists(content_path_.Append(dlc_id)));

  StatusResult status_result;
  status_result.set_current_operation(Operation::REPORTING_ERROR_EVENT);
  status_result.set_is_install(true);
  dlc_service_->OnStatusUpdateAdvancedSignal(status_result);

  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
  for (const string& dlc_id : dlc_ids)
    EXPECT_FALSE(base::PathExists(content_path_.Append(dlc_id)));
}

TEST_F(DlcServiceTest, ReportingFailureSignalTest) {
  const vector<string>& dlc_ids = {kSecondDlc, kThirdDlc};
  DlcModuleList dlc_module_list = CreateDlcModuleList(dlc_ids);

  EXPECT_TRUE(dlc_service_->Install(dlc_module_list, nullptr));

  for (const string& dlc_id : dlc_ids)
    EXPECT_TRUE(base::PathExists(content_path_.Append(dlc_id)));

  StatusResult status_result;
  status_result.set_current_operation(Operation::REPORTING_ERROR_EVENT);
  status_result.set_is_install(true);
  dlc_service_->OnStatusUpdateAdvancedSignal(status_result);
  EXPECT_EQ(dlc_service_test_observer_->GetInstallStatus().status(),
            Status::FAILED);
}

TEST_F(DlcServiceTest, ProbableUpdateEngineRestartCleanupTest) {
  const vector<string>& dlc_ids = {kSecondDlc, kThirdDlc};
  DlcModuleList dlc_module_list = CreateDlcModuleList(dlc_ids);

  EXPECT_TRUE(dlc_service_->Install(dlc_module_list, nullptr));

  for (const string& dlc_id : dlc_ids)
    EXPECT_TRUE(base::PathExists(content_path_.Append(dlc_id)));

  StatusResult status_result;
  status_result.set_current_operation(Operation::IDLE);
  status_result.set_is_install(false);
  dlc_service_->OnStatusUpdateAdvancedSignal(status_result);

  EXPECT_TRUE(base::PathExists(content_path_.Append(kFirstDlc)));
  for (const string& dlc_id : dlc_ids)
    EXPECT_FALSE(base::PathExists(content_path_.Append(dlc_id)));
}

TEST_F(DlcServiceTest, UpdateEngineFailSafeTest) {
  EXPECT_CALL(*mock_update_engine_proxy_ptr_, GetStatusAdvanced(_, _, _))
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  const vector<string>& dlc_ids = {kSecondDlc};
  DlcModuleList dlc_module_list = CreateDlcModuleList(dlc_ids);

  EXPECT_TRUE(dlc_service_->Install(dlc_module_list, nullptr));
  for (const string& dlc_id : dlc_ids)
    EXPECT_TRUE(base::PathExists(content_path_.Append(dlc_id)));

  MessageLoopRunUntil(&loop_, base::TimeDelta::FromSeconds(kUECheckTimeout * 2),
                      base::Bind([]() { return false; }));

  for (const string& dlc_id : dlc_ids)
    EXPECT_FALSE(base::PathExists(content_path_.Append(dlc_id)));
}

TEST_F(DlcServiceTest, UpdateEngineFailAfterSignalsSafeTest) {
  EXPECT_CALL(*mock_update_engine_proxy_ptr_, GetStatusAdvanced(_, _, _))
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  const vector<string>& dlc_ids = {kSecondDlc};
  DlcModuleList dlc_module_list = CreateDlcModuleList(dlc_ids);

  EXPECT_TRUE(dlc_service_->Install(dlc_module_list, nullptr));
  for (const string& dlc_id : dlc_ids)
    EXPECT_TRUE(base::PathExists(content_path_.Append(dlc_id)));

  StatusResult status_result;
  status_result.set_current_operation(Operation::DOWNLOADING);
  status_result.set_is_install(true);
  dlc_service_->OnStatusUpdateAdvancedSignal(status_result);

  MessageLoopRunUntil(&loop_, base::TimeDelta::FromSeconds(kUECheckTimeout * 2),
                      base::Bind([]() { return false; }));

  for (const string& dlc_id : dlc_ids)
    EXPECT_FALSE(base::PathExists(content_path_.Append(dlc_id)));
}

TEST_F(DlcServiceTest, OnStatusUpdateAdvancedSignalDownloadProgressTest) {
  const vector<string>& dlc_ids = {kSecondDlc, kThirdDlc};
  DlcModuleList dlc_module_list = CreateDlcModuleList(dlc_ids);

  EXPECT_TRUE(dlc_service_->Install(dlc_module_list, nullptr));

  EXPECT_CALL(*mock_image_loader_proxy_ptr_, UnloadDlcImage(_, _, _, _, _))
      .Times(0);

  StatusResult status_result;
  status_result.set_is_install(true);

  vector<Operation> install_operation_sequence = {
      Operation::CHECKING_FOR_UPDATE, Operation::UPDATE_AVAILABLE,
      Operation::FINALIZING};

  for (const auto& op : install_operation_sequence) {
    status_result.set_current_operation(op);
    dlc_service_->OnStatusUpdateAdvancedSignal(status_result);
    EXPECT_FALSE(dlc_service_test_observer_->IsSendInstallStatusCalled());
  }

  status_result.set_current_operation(Operation::DOWNLOADING);
  dlc_service_->OnStatusUpdateAdvancedSignal(status_result);
  EXPECT_EQ(dlc_service_test_observer_->GetInstallStatus().status(),
            Status::RUNNING);

  status_result.set_current_operation(Operation::IDLE);
  dlc_service_->OnStatusUpdateAdvancedSignal(status_result);
  EXPECT_EQ(dlc_service_test_observer_->GetInstallStatus().status(),
            Status::COMPLETED);
}

}  // namespace dlcservice
