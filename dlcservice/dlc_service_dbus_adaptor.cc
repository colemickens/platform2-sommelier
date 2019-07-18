// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/dlc_service_dbus_adaptor.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/message_loop/message_loop.h>
#include <brillo/errors/error.h>
#include <brillo/errors/error_codes.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dlcservice/dbus-constants.h>
#include <update_engine/dbus-constants.h>

#include "dlcservice/boot_slot.h"
#include "dlcservice/utils.h"

using std::string;
using std::vector;

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
void LogAndSetError(brillo::ErrorPtr* err, const string& msg) {
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
  string boot_disk_name;
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

  // Initalize installed DLC modules.
  installed_dlc_modules_ = utils::ScanDirectory(content_dir_);

  // Initialize supported DLC modules.
  supported_dlc_modules_ = utils::ScanDirectory(manifest_dir_);
}

DlcServiceDBusAdaptor::~DlcServiceDBusAdaptor() {}

void DlcServiceDBusAdaptor::LoadDlcModuleImages() {
  // Load all installed DLC modules.
  for (const auto& dlc_module_id : installed_dlc_modules_) {
    std::string package = ScanDlcModulePackage(dlc_module_id);
    auto dlc_module_content_path =
        utils::GetDlcModulePackagePath(content_dir_, dlc_module_id, package);

    // Mount the installed DLC image.
    string path;
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
                                    const DlcModuleList& dlc_module_list_in) {
  const auto& dlc_modules = dlc_module_list_in.dlc_module_infos();
  if (dlc_modules.empty()) {
    LogAndSetError(err, "Must provide at least one DLC to install");
    return false;
  }

  // Note: this holds the list of directories that were created and need to be
  // freed in case an error happens.
  vector<std::unique_ptr<base::ScopedTempDir>> scoped_paths;

  for (const DlcModuleInfo& dlc_module : dlc_modules) {
    base::FilePath path;
    const string& id = dlc_module.dlc_id();
    auto scoped_path = std::make_unique<base::ScopedTempDir>();

    if (!CreateDlc(err, id, &path)) {
      return false;
    }
    if (!scoped_path->Set(path)) {
      LOG(ERROR) << "Failed when scoping path during install: " << path.value();
      return false;
    }

    scoped_paths.emplace_back(std::move(scoped_path));
  }

  if (!CheckForUpdateEngineStatus({update_engine::kUpdateStatusIdle})) {
    LogAndSetError(
        err, "Update Engine is performing operations or a reboot is pending.");
    return false;
  }

  // Invokes update_engine to install the DLC module.
  if (!update_engine_proxy_->AttemptInstall(dlc_module_list_in, nullptr)) {
    LogAndSetError(err, "Update Engine failed to schedule install operations.");
    return false;
  }

  dlc_modules_being_installed_ = dlc_module_list_in;
  // Note: Do NOT add to installed indication. Let |OnStatusUpdateSignaled()|
  // handle since hat's truly when the DLC(s) are installed.

  // Safely take ownership of scoped paths for them not to be freed.
  for (const auto& scoped_path : scoped_paths)
    scoped_path->Take();

  return true;
}

// TODO(crbug/986391): Need to take a protobuf as argument and not a single DLC.
bool DlcServiceDBusAdaptor::Uninstall(brillo::ErrorPtr* err,
                                      const string& id_in) {
  if (installed_dlc_modules_.find(id_in) == installed_dlc_modules_.end()) {
    LogAndSetError(err, "The DLC ID provided is not installed");
    return false;
  }
  string package = ScanDlcModulePackage(id_in);

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

  LOG(INFO) << "Uninstalling DLC id:" << id_in;
  installed_dlc_modules_.erase(id_in);
  return true;
}

bool DlcServiceDBusAdaptor::GetInstalled(brillo::ErrorPtr* err,
                                         DlcModuleList* dlc_module_list_out) {
  const auto InsertIntoDlcModuleListOut =
      [dlc_module_list_out](const string& dlc_module_id) {
        DlcModuleInfo* dlc_module_info =
            dlc_module_list_out->add_dlc_module_infos();
        dlc_module_info->set_dlc_id(dlc_module_id);
      };
  for_each(begin(installed_dlc_modules_), end(installed_dlc_modules_),
           InsertIntoDlcModuleListOut);
  return true;
}

bool DlcServiceDBusAdaptor::CreateDlc(brillo::ErrorPtr* err,
                                      const string& id,
                                      base::FilePath* path) {
  path->clear();
  if (supported_dlc_modules_.find(id) == supported_dlc_modules_.end()) {
    LogAndSetError(err, "The DLC ID provided is not supported.");
    return false;
  }

  const string& package = ScanDlcModulePackage(id);
  base::FilePath module_path = utils::GetDlcModulePath(content_dir_, id);
  base::FilePath module_package_path =
      utils::GetDlcModulePackagePath(content_dir_, id, package);

  if (base::PathExists(module_path)) {
    LogAndSetError(err, "The DLC module is installed or duplicate.");
    return false;
  }
  // Create the DLC ID directory with correct permissions.
  if (!CreateDirWithDlcPermissions(module_path)) {
    LogAndSetError(err, "Failed to create DLC ID directory");
    return false;
  }
  // Create the DLC package directory with correct permissions.
  if (!CreateDirWithDlcPermissions(module_package_path)) {
    LogAndSetError(err, "Failed to create DLC ID package directory");
    return false;
  }

  // Creates DLC module storage.
  // TODO(xiaochu): Manifest currently returns a signed integer, which means
  // will likely fail for modules >= 2 GiB in size. https://crbug.com/904539
  imageloader::Manifest manifest;
  if (!dlcservice::utils::GetDlcManifest(manifest_dir_, id, package,
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
      utils::GetDlcModuleImagePath(content_dir_, id, package, 0);
  if (!CreateImageFile(image_a_path, image_size)) {
    LogAndSetError(err, "Failed to create slot A DLC image file");
    return false;
  }

  // Creates image B.
  base::FilePath image_b_path =
      utils::GetDlcModuleImagePath(content_dir_, id, package, 1);
  if (!CreateImageFile(image_b_path, image_size)) {
    LogAndSetError(err, "Failed to create slot B image file");
    return false;
  }

  *path = module_path;
  return true;
}

string DlcServiceDBusAdaptor::ScanDlcModulePackage(const string& id) {
  return *(utils::ScanDirectory(manifest_dir_.Append(id)).begin());
}

bool DlcServiceDBusAdaptor::CheckForUpdateEngineStatus(
    const vector<string>& status_list) {
  int64_t last_checked_time = 0;
  double progress = 0;
  string current_operation;
  string new_version;
  int64_t new_size = 0;
  if (!update_engine_proxy_->GetStatus(&last_checked_time, &progress,
                                       &current_operation, &new_version,
                                       &new_size, nullptr)) {
    LOG(ERROR) << "Update Engine is not available.";
    return false;
  }
  if (!std::any_of(status_list.begin(), status_list.end(),
                   [&current_operation](const string& status) {
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
    const string& current_operation,
    const string& new_version,
    int64_t new_size) {
  // This signal is for DLC install only when have DLC modules being installed.
  if (dlc_modules_being_installed_.dlc_module_infos_size() == 0)
    return;
  // Install is complete when we receive kUpdateStatusIdle signal.
  if (current_operation != update_engine::kUpdateStatusIdle)
    return;

  // At this point, update_engine finished installation of the requested DLC
  // modules (failure or success).
  DlcModuleList dlc_module_list = dlc_modules_being_installed_;
  dlc_modules_being_installed_.clear_dlc_module_infos();

  InstallResult install_result;
  install_result.set_success(false);
  install_result.mutable_dlc_module_list()->CopyFrom(dlc_module_list);

  // Mount the installed DLC module images.
  for (auto& dlc_module :
       *install_result.mutable_dlc_module_list()->mutable_dlc_module_infos()) {
    const string& dlc_id = dlc_module.dlc_id();
    string package = ScanDlcModulePackage(dlc_id);
    string mount_point;
    if (!image_loader_proxy_->LoadDlcImage(
            dlc_id, package, current_boot_slot_name_, &mount_point, nullptr)) {
      LOG(ERROR) << "Imageloader is not available.";
      install_result.set_error_code(static_cast<int>(
          OnInstalledSignalErrorCode::kImageLoaderReturnsFalse));
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
    dlc_module.set_dlc_root(
        utils::GetDlcRootInModulePath(base::FilePath(mount_point)).value());
  }

  // Install was a success so keep track.
  for (const DlcModuleInfo& dlc_module : dlc_module_list.dlc_module_infos())
    installed_dlc_modules_.insert(dlc_module.dlc_id());

  install_result.set_success(true);
  install_result.set_error_code(
      static_cast<int>(OnInstalledSignalErrorCode::kNone));
  SendOnInstalledSignal(install_result);
}

void DlcServiceDBusAdaptor::OnStatusUpdateSignalConnected(
    const string& interface_name, const string& signal_name, bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to connect to update_engine's StatusUpdate signal.";
  }
}

}  // namespace dlcservice
