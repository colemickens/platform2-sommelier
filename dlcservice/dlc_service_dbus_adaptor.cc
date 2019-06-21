// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/dlc_service_dbus_adaptor.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/message_loop/message_loop.h>
#include <brillo/errors/error.h>
#include <brillo/errors/error_codes.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dlcservice/dbus-constants.h>
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

}  // namespace

DlcServiceDBusAdaptor::DlcServiceDBusAdaptor(
    std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
        image_loader_proxy,
    std::unique_ptr<org::chromium::UpdateEngineInterfaceProxyInterface>
        update_engine_proxy,
    std::unique_ptr<BootSlot> boot_slot,
    const base::FilePath& manifest_dir,
    const base::FilePath& content_dir)
    : org::chromium::DlcServiceInterfaceAdaptor(this),
      image_loader_proxy_(std::move(image_loader_proxy)),
      update_engine_proxy_(std::move(update_engine_proxy)),
      boot_slot_(std::move(boot_slot)),
      manifest_dir_(manifest_dir),
      content_dir_(content_dir),
      weak_ptr_factory_(this) {
  // Get current boot slot.
  std::string boot_disk_name;
  int num_slots = -1;
  int current_boot_slot = -1;
  if (!boot_slot_->GetCurrentSlot(&boot_disk_name, &num_slots,
                                  &current_boot_slot))
    LOG(FATAL) << "Can not get current boot slot.";

  current_boot_slot_name_ = current_boot_slot == 0 ? imageloader::kSlotNameA
                                                   : imageloader::kSlotNameB;

  // Register D-Bus signal callbacks.
  update_engine_proxy_->RegisterStatusUpdateSignalHandler(
      base::Bind(&DlcServiceDBusAdaptor::OnStatusUpdateSignal,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DlcServiceDBusAdaptor::OnStatusUpdateSignalConnected,
                 weak_ptr_factory_.GetWeakPtr()));
}

DlcServiceDBusAdaptor::~DlcServiceDBusAdaptor() {}

void DlcServiceDBusAdaptor::LoadDlcModuleImages() {
  // Initialize supported DLC module id list.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();

  // Load all installed DLC modules.
  for (const auto& dlc_module_id : dlc_module_ids) {
    std::string package = ScanDlcModulePackage(dlc_module_id);

    auto dlc_module_content_path =
        utils::GetDlcModulePackagePath(content_dir_, dlc_module_id, package);
    if (!base::PathExists(dlc_module_content_path))
      continue;
    // Mount the installed DLC image.
    std::string path;
    image_loader_proxy_->LoadDlcImage(dlc_module_id, package,
                                      current_boot_slot_name_, &path, nullptr);
    if (path.empty()) {
      LOG(ERROR) << "DLC image " << dlc_module_id << "/" << package
                 << " is corrupted.";
    } else {
      LOG(INFO) << "DLC image " << dlc_module_id << "/" << package
                << " is mounted at " << path;
    }
  }
}

bool DlcServiceDBusAdaptor::Install(brillo::ErrorPtr* err,
                                    const std::string& id_in,
                                    const std::string& omaha_url_in) {
  // TODO(xiaochu): change API to accept a list of DLC module ids.
  // https://crbug.com/905075
  // Initialize supported DLC module id list.
  //
  // TODO(ahassani): This is inefficient. We don't need to know about all DLCs
  // in order to install one of them. We need to add a function that checks the
  // existence of a DLC (in rootfs) given a DLC ID. Other solution is to read
  // and keep a list of DLCs on the start up and just add a function to look up
  // the DLC ID from there.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();
  if (std::find(dlc_module_ids.begin(), dlc_module_ids.end(), id_in) ==
      dlc_module_ids.end()) {
    LogAndSetError(err, "The DLC ID provided is invalid.");
    return false;
  }
  std::string package = ScanDlcModulePackage(id_in);

  // Create the DLC ID directory with correct permissions.
  base::FilePath module_path = utils::GetDlcModulePath(content_dir_, id_in);
  if (base::PathExists(module_path)) {
    LogAndSetError(err, "The DLC module is installed.");
    return false;
  }
  if (!CreateDirWithDlcPermissions(module_path)) {
    LogAndSetError(err, "Failed to create DLC ID directory.");
    return false;
  }

  // Create the DLC package directory with correct permissions.
  base::FilePath module_package_path =
      utils::GetDlcModulePackagePath(content_dir_, id_in, package);
  if (base::PathExists(module_package_path)) {
    LogAndSetError(err, "The DLC module is installed.");
    return false;
  }
  if (!CreateDirWithDlcPermissions(module_package_path)) {
    LogAndSetError(err, "Failed to create DLC Package directory.");
    return false;
  }

  // Creates DLC module storage.
  // TODO(xiaochu): Manifest currently returns a signed integer, which means
  // this will likely fail for modules >= 2 GiB in size.
  // https://crbug.com/904539
  imageloader::Manifest manifest;
  if (!dlcservice::utils::GetDlcManifest(manifest_dir_, id_in, package,
                                         &manifest)) {
    LogAndSetError(err, "Failed to get DLC module manifest.");
    return false;
  }
  int64_t image_size = manifest.preallocated_size();
  if (image_size <= 0) {
    LogAndSetError(err, "Preallocated size in manifest is illegal.");
    return false;
  }

  // Creates image A.
  base::FilePath image_a_path =
      utils::GetDlcModuleImagePath(content_dir_, id_in, package, 0);
  if (!CreateImageFile(image_a_path, image_size)) {
    LogAndSetError(err, "Failed to create slot A DLC image file");
    return false;
  }

  // Creates image B.
  base::FilePath image_b_path =
      utils::GetDlcModuleImagePath(content_dir_, id_in, package, 1);
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
  if (!update_engine_proxy_->AttemptInstall(dlc_parameters, nullptr)) {
    LogAndSetError(err, "Update Engine failed to schedule install operations.");
    return false;
  }

  dlc_id_being_installed_ = id_in;

  return true;
}

bool DlcServiceDBusAdaptor::Uninstall(brillo::ErrorPtr* err,
                                      const std::string& id_in) {
  // Initialize supported DLC module id list.
  //
  // TODO(ahassani): This is inefficient. We don't need to know about all DLCs
  // in order to uninstall one of them. We need to add a function that checks
  // the existence of a DLC (in rootfs) given a DLC ID. Other solution is to
  // read and keep a list of DLCs on the start up and just add a function to
  // look up the DLC ID from there.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();

  if (std::find(dlc_module_ids.begin(), dlc_module_ids.end(), id_in) ==
      dlc_module_ids.end()) {
    LogAndSetError(err, "The DLC ID provided is invalid.");
    return false;
  }
  std::string package = ScanDlcModulePackage(id_in);

  const base::FilePath dlc_module_content_path =
      utils::GetDlcModulePackagePath(content_dir_, id_in, package);
  if (!base::PathExists(dlc_module_content_path) ||
      !base::PathExists(
          utils::GetDlcModuleImagePath(content_dir_, id_in, package, 0)) ||
      !base::PathExists(
          utils::GetDlcModuleImagePath(content_dir_, id_in, package, 1))) {
    LogAndSetError(err, "The DLC module is not installed properly.");
    return false;
  }
  // Unmounts the DLC module image.
  bool success = false;
  if (!image_loader_proxy_->UnloadDlcImage(id_in, package, &success, nullptr)) {
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
  const base::FilePath dlc_module_path =
      utils::GetDlcModulePath(content_dir_, id_in);
  if (!base::DeleteFile(dlc_module_path, true)) {
    LogAndSetError(err, "DLC image folder could not be deleted.");
    return false;
  }
  return true;
}

bool DlcServiceDBusAdaptor::GetInstalled(brillo::ErrorPtr* err,
                                         DlcModuleList* dlc_module_list_out) {
  // Initialize supported DLC module ID list.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();

  // Find installed DLC modules.
  for (const auto& dlc_module_id : dlc_module_ids) {
    auto dlc_module_content_path =
        utils::GetDlcModulePath(content_dir_, dlc_module_id);
    if (base::PathExists(dlc_module_content_path)) {
      DlcModuleInfo* dlc_module_info =
          dlc_module_list_out->add_dlc_module_infos();
      dlc_module_info->set_dlc_id(dlc_module_id);
    }
  }
  return true;
}

std::vector<std::string> DlcServiceDBusAdaptor::ScanDlcModules() {
  return utils::ScanDirectory(manifest_dir_);
}

std::string DlcServiceDBusAdaptor::ScanDlcModulePackage(const std::string& id) {
  return utils::ScanDirectory(manifest_dir_.Append(id))[0];
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

void DlcServiceDBusAdaptor::SendOnInstalledSignal(
    const InstallResult& install_result) {
  org::chromium::DlcServiceInterfaceAdaptor::SendOnInstalledSignal(
      install_result);
}

void DlcServiceDBusAdaptor::OnStatusUpdateSignal(
    int64_t last_checked_time,
    double progress,
    const std::string& current_operation,
    const std::string& new_version,
    int64_t new_size) {
  // This signal is for DLC install only when have DLC modules being installed.
  if (dlc_id_being_installed_.empty())
    return;
  // Install is complete when we receive kUpdateStatusIdle signal.
  if (current_operation != update_engine::kUpdateStatusIdle)
    return;

  // At this point, update_engine finished installation of the requested DLC
  // module (failure or success).
  std::string dlc_id = dlc_id_being_installed_;
  dlc_id_being_installed_.clear();

  InstallResult install_result;
  install_result.set_success(false);
  install_result.set_dlc_id(dlc_id);

  std::string package = ScanDlcModulePackage(dlc_id);

  // Mount the installed DLC module image.
  std::string mount_point;
  if (!image_loader_proxy_->LoadDlcImage(
          dlc_id, package, current_boot_slot_name_, &mount_point, nullptr)) {
    LOG(ERROR) << "Imageloader is not available.";
    install_result.set_error_code(
        static_cast<int>(OnInstalledSignalErrorCode::kImageLoaderReturnsFalse));
    SendOnInstalledSignal(install_result);
    return;
  }
  if (mount_point.empty()) {
    LOG(ERROR) << "Imageloader LoadDlcImage failed.";
    install_result.set_error_code(
        static_cast<int>(OnInstalledSignalErrorCode::kMountFailure));
    SendOnInstalledSignal(install_result);
    return;
  }

  install_result.set_success(true);
  install_result.set_error_code(
      static_cast<int>(OnInstalledSignalErrorCode::kNone));
  install_result.set_dlc_root(
      utils::GetDlcRootInModulePath(base::FilePath(mount_point)).value());
  SendOnInstalledSignal(install_result);
}

void DlcServiceDBusAdaptor::OnStatusUpdateSignalConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to connect to update_engine's StatusUpdate signal.";
  }
}

}  // namespace dlcservice
