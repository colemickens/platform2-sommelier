// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/dlc_service_dbus_adaptor.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <brillo/errors/error.h>
#include <brillo/errors/error_codes.h>
#include <chromeos/dbus/service_constants.h>
#include <dlcservice/proto_bindings/dlcservice.pb.h>
#include <update_engine/dbus-constants.h>

#include "dlcservice/boot_slot.h"
#include "dlcservice/utils.h"

namespace dlcservice {

namespace {

// Permissions for DLC module directories.
constexpr mode_t kDlcModuleDirectoryPerms = 0755;

// Creates a directory with permissions required for DLC modules.
bool CreateDirWithDlcPermissions(const base::FilePath& path) {
  base::File::Error file_err;
  if (!base::CreateDirectoryAndGetError(path, &file_err)) {
    LOG(ERROR) << "Failed to create directory '" << path.value()
               << "': " << base::File::ErrorToString(file_err);
    return false;
  }
  if (!base::SetPosixFilePermissions(path, kDlcModuleDirectoryPerms)) {
    LOG(ERROR) << "Failed to set directory permissions for '" << path.value()
               << "'";
    return false;
  }
  return true;
}

// Creates a directory with an empty image file and resizes it to the given
// size.
bool CreateImageFile(const base::FilePath& path, int64_t image_size) {
  if (!CreateDirWithDlcPermissions(path.DirName())) {
    return false;
  }
  constexpr uint32_t file_flags =
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE;
  base::File file(path, file_flags);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to create image file '" << path.value()
               << "': " << base::File::ErrorToString(file.error_details());
    return false;
  }
  if (!file.SetLength(image_size)) {
    LOG(ERROR) << "Failed to reserve backup file for DLC module image '"
               << path.value() << "'";
    return false;
  }
  return true;
}

// Sets the D-Bus error object and logs the error message.
void LogAndSetError(brillo::ErrorPtr* err, const std::string& msg) {
  if (err)
    *err = brillo::Error::Create(FROM_HERE, "dlcservice", "INTERNAL", msg);
  LOG(ERROR) << msg;
}

// The time interval we check for update_engine's progress.
constexpr base::TimeDelta kCheckInterval = base::TimeDelta::FromSeconds(1);
// The retry times to get update_engine's progress before giving up.
constexpr int kRetryCount = 10;

}  // namespace

DlcServiceDBusAdaptor::DlcServiceDBusAdaptor(
    std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
        image_loader_proxy,
    std::unique_ptr<org::chromium::UpdateEngineInterfaceProxyInterface>
        update_engine_proxy,
    std::unique_ptr<BootSlot> boot_slot,
    const base::FilePath& manifest_dir,
    const base::FilePath& content_dir,
    ShutdownDelegate* shutdown_delegate)
    : org::chromium::DlcServiceInterfaceAdaptor(this),
      image_loader_proxy_(std::move(image_loader_proxy)),
      update_engine_proxy_(std::move(update_engine_proxy)),
      boot_slot_(std::move(boot_slot)),
      manifest_dir_(manifest_dir),
      content_dir_(content_dir),
      shutdown_delegate_(shutdown_delegate) {}

DlcServiceDBusAdaptor::~DlcServiceDBusAdaptor() {}

void DlcServiceDBusAdaptor::LoadDlcModuleImages() {
  // Initialize supported DLC module id list.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();

  std::string boot_disk_name;
  int num_slots = -1;
  int current_slot = -1;
  if (!boot_slot_->GetCurrentSlot(&boot_disk_name, &num_slots, &current_slot)) {
    LOG(ERROR) << "Can not get current boot slot.";
    return;
  }
  // Load all installed DLC modules.
  for (const auto& dlc_module_id : dlc_module_ids) {
    auto dlc_module_content_path =
        utils::GetDlcModulePath(content_dir_, dlc_module_id);
    if (!base::PathExists(dlc_module_content_path))
      continue;
    // Mount the installed DLC image.
    std::string path;
    image_loader_proxy_->LoadDlcImage(
        dlc_module_id,
        current_slot == 0 ? imageloader::kSlotNameA : imageloader::kSlotNameB,
        &path, nullptr);
    if (path.empty()) {
      LOG(ERROR) << "DLC image " << dlc_module_id << " is corrupted.";
    } else {
      LOG(INFO) << "DLC image " << dlc_module_id << " is mounted at " << path;
    }
  }
}

bool DlcServiceDBusAdaptor::Install(brillo::ErrorPtr* err,
                                    const std::string& id_in,
                                    const std::string& omaha_url_in,
                                    std::string* dlc_root_out) {
  // TODO(xiaochu): change API to accept a list of DLC module ids.
  // https://crbug.com/905075
  ScopedShutdown scoped_shutdown(shutdown_delegate_);

  // Initialize supported DLC module id list.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();
  if (std::find(dlc_module_ids.begin(), dlc_module_ids.end(), id_in) ==
      dlc_module_ids.end()) {
    LogAndSetError(err, "The DLC ID provided is invalid.");
    return false;
  }

  // TODO(xiaochu): may detect corrupted DLC modules and recover.
  // https://crbug.com/903432
  base::FilePath module_path = utils::GetDlcModulePath(content_dir_, id_in);
  if (base::PathExists(module_path)) {
    LogAndSetError(err, "The DLC module is installed.");
    return false;
  }

  // Create module directory
  if (!CreateDirWithDlcPermissions(module_path)) {
    LogAndSetError(err, "Failed to create DLC module directory");
    return false;
  }

  // Creates DLC module storage.
  // TODO(xiaochu): Manifest currently returns a signed integer, which means
  // this will likely fail for modules >= 2 GiB in size.
  // https://crbug.com/904539
  imageloader::Manifest manifest;
  if (!dlcservice::utils::GetDlcManifest(manifest_dir_, id_in, &manifest)) {
    LogAndSetError(err, "Failed to get DLC module manifest.");
    return false;
  }
  int64_t image_size = manifest.preallocated_size();
  if (image_size <= 0) {
    LogAndSetError(err, "Preallocated size  in manifest is illegal.");
    return false;
  }

  // Creates image A.
  base::FilePath image_a_path =
      utils::GetDlcModuleImagePath(content_dir_, id_in, 0);
  if (!CreateImageFile(image_a_path, image_size)) {
    LogAndSetError(err, "Failed to create slot A DLC image file");
    return false;
  }

  // Creates image B.
  base::FilePath image_b_path =
      utils::GetDlcModuleImagePath(content_dir_, id_in, 1);
  if (!CreateImageFile(image_b_path, image_size)) {
    LogAndSetError(err, "Failed to create slot B image file");
    return false;
  }

  if (!CheckForUpdateEngineStatus({update_engine::kUpdateStatusIdle})) {
    LogAndSetError(
        err, "Update Engine is performing operations or a reboot is pending.");
    return false;
  }

  // Invokes update_engine to install the DLC module.
  dlcservice::DlcModuleList dlc_parameters;
  dlc_parameters.set_omaha_url(omaha_url_in);
  dlcservice::DlcModuleInfo* dlc_info = dlc_parameters.add_dlc_module_infos();
  dlc_info->set_dlc_id(id_in);
  std::string dlc_request;
  if (!dlc_parameters.SerializeToString(&dlc_request)) {
    LogAndSetError(err, "protobuf can not be serialized.");
    return false;
  }
  if (!update_engine_proxy_->AttemptInstall(dlc_request, nullptr)) {
    LogAndSetError(err, "Update Engine failed to schedule install operations.");
    return false;
  }

  // TODO(xiaochu): make the API asynchronous.
  // https://crbug.com/905071
  // Code below this point should be moved to a callback (triggered after
  // update_engine finishes install) and the function returns true immediately
  // to unblock the caller. Currently, we make the assumption that a DLC install
  // completes in 10 seconds which is more than enough for downloading (reading)
  // a local file on DUT.
  if (!WaitForUpdateEngineIdle()) {
    LogAndSetError(err, "Failed waiting for update_engine to become idle");
    return false;
  }

  // Mount the installed DLC module image.
  std::string boot_disk_name;
  int num_slots = -1;
  int current_slot = -1;
  if (!boot_slot_->GetCurrentSlot(&boot_disk_name, &num_slots, &current_slot)) {
    LogAndSetError(err, "Can not get current boot slot.");
    return false;
  }

  std::string mount_point;
  if (!image_loader_proxy_->LoadDlcImage(
          id_in,
          current_slot == 0 ? imageloader::kSlotNameA : imageloader::kSlotNameB,
          &mount_point, nullptr)) {
    LogAndSetError(err, "Imageloader is not available.");
    return false;
  }
  if (mount_point.empty()) {
    LogAndSetError(err, "Imageloader LoadDlcImage failed.");
    return false;
  }

  *dlc_root_out =
      utils::GetDlcRootInModulePath(base::FilePath(mount_point)).value();

  return true;
}

bool DlcServiceDBusAdaptor::Uninstall(brillo::ErrorPtr* err,
                                      const std::string& id_in) {
  ScopedShutdown scoped_shutdown(shutdown_delegate_);

  // Initialize supported DLC module id list.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();

  if (std::find(dlc_module_ids.begin(), dlc_module_ids.end(), id_in) ==
      dlc_module_ids.end()) {
    LogAndSetError(err, "The DLC ID provided is invalid.");
    return false;
  }

  const base::FilePath dlc_module_content_path =
      utils::GetDlcModulePath(content_dir_, id_in);
  if (!base::PathExists(dlc_module_content_path) ||
      !base::PathExists(utils::GetDlcModuleImagePath(content_dir_, id_in, 0)) ||
      !base::PathExists(utils::GetDlcModuleImagePath(content_dir_, id_in, 1))) {
    LogAndSetError(err, "The DLC module is not installed properly.");
    return false;
  }
  // Unmounts the DLC module image.
  bool success = false;
  if (!image_loader_proxy_->UnloadDlcImage(id_in, &success, nullptr)) {
    LogAndSetError(err, "Imageloader is not available.");
    return false;
  }
  if (!success) {
    LogAndSetError(err, "Imageloader UnloadDlcImage failed.");
    return false;
  }

  if (!CheckForUpdateEngineStatus(
          {update_engine::kUpdateStatusIdle,
           update_engine::kUpdateStatusUpdatedNeedReboot})) {
    LogAndSetError(err, "Update Engine is performing operations.");
    return false;
  }

  // Deletes the DLC module images.
  if (!base::DeleteFile(dlc_module_content_path, true)) {
    LogAndSetError(err, "DLC image folder could not be deleted.");
    return false;
  }
  return true;
}

bool DlcServiceDBusAdaptor::GetInstalled(brillo::ErrorPtr* err,
                                         std::string* dlc_module_list_out) {
  ScopedShutdown scoped_shutdown(shutdown_delegate_);

  // Initialize supported DLC module id list.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();

  // Find installed DLC modules.
  DlcModuleList dlc_module_list;
  for (const auto& dlc_module_id : dlc_module_ids) {
    auto dlc_module_content_path =
        utils::GetDlcModulePath(content_dir_, dlc_module_id);
    if (base::PathExists(dlc_module_content_path)) {
      DlcModuleInfo* dlc_module_info = dlc_module_list.add_dlc_module_infos();
      dlc_module_info->set_dlc_id(dlc_module_id);
    }
  }
  if (!dlc_module_list.SerializeToString(dlc_module_list_out)) {
    LogAndSetError(err, "Failed to serialize the protobuf.");
    return false;
  }
  return true;
}

std::vector<std::string> DlcServiceDBusAdaptor::ScanDlcModules() {
  std::vector<std::string> dlc_module_ids;
  base::FileEnumerator file_enumerator(manifest_dir_, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath dir_path = file_enumerator.Next(); !dir_path.empty();
       dir_path = file_enumerator.Next()) {
    dlc_module_ids.emplace_back(dir_path.BaseName().value());
  }
  return dlc_module_ids;
}

bool DlcServiceDBusAdaptor::WaitForUpdateEngineIdle() {
  for (int i = 0; i < kRetryCount; i++) {
    if (CheckForUpdateEngineStatus({update_engine::kUpdateStatusIdle})) {
      return true;
    }
    base::PlatformThread::Sleep(kCheckInterval);
  }
  return false;
}

bool DlcServiceDBusAdaptor::CheckForUpdateEngineStatus(
    const std::vector<std::string>& status_list) {
  int64_t last_checked_time = 0;
  double progress = 0;
  std::string current_operation;
  std::string new_version;
  int64_t new_size = 0;
  if (!update_engine_proxy_->GetStatus(&last_checked_time, &progress,
                                       &current_operation, &new_version,
                                       &new_size, nullptr)) {
    LOG(ERROR) << "Update Engine is not available.";
    return false;
  }
  if (!std::any_of(status_list.begin(), status_list.end(),
                   [&current_operation](const std::string& status) {
                     return current_operation == status;
                   })) {
    return false;
  }
  return true;
}

}  // namespace dlcservice
