// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <utility>

#include "dlcservice/dlc_manager.h"

#include <base/files/file_util.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/message_loops/message_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dlcservice/dbus-constants.h>

#include "dlcservice/utils.h"

using base::Callback;
using base::File;
using base::FilePath;
using std::string;
using std::unique_ptr;

namespace dlcservice {

// Keep kDlcMetadataFilePingActive in sync with update_engine's.
const char kDlcMetadataFilePingActive[] = "active";
const char kDlcMetadataActiveValue[] = "1";

namespace {

// Permissions for DLC module directories.
constexpr mode_t kDlcModuleDirectoryPerms = 0755;

// Creates a directory with permissions required for DLC modules.
bool CreateDirWithDlcPermissions(const FilePath& path) {
  File::Error file_err;
  if (!base::CreateDirectoryAndGetError(path, &file_err)) {
    LOG(ERROR) << "Failed to create directory '" << path.value()
               << "': " << File::ErrorToString(file_err);
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
bool CreateImageFile(const FilePath& path, int64_t image_size) {
  if (!CreateDirWithDlcPermissions(path.DirName())) {
    return false;
  }
  constexpr uint32_t file_flags =
      File::FLAG_CREATE | File::FLAG_READ | File::FLAG_WRITE;
  File file(path, file_flags);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to create image file '" << path.value()
               << "': " << File::ErrorToString(file.error_details());
    return false;
  }
  if (!file.SetLength(image_size)) {
    LOG(ERROR) << "Failed to reserve backup file for DLC module image '"
               << path.value() << "'";
    return false;
  }
  return true;
}

}  // namespace

class DlcManager::DlcManagerImpl {
 public:
  DlcManagerImpl(
      std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
          image_loader_proxy,
      std::unique_ptr<BootSlot> boot_slot,
      const FilePath& manifest_dir,
      const FilePath& content_dir,
      const FilePath& metadata_dir)
      : image_loader_proxy_(std::move(image_loader_proxy)),
        manifest_dir_(manifest_dir),
        content_dir_(content_dir),
        metadata_dir_(metadata_dir) {
    string boot_disk_name;
    if (!boot_slot->GetCurrentSlot(&boot_disk_name, &current_boot_slot_))
      LOG(FATAL) << "Can not get current boot slot.";

    // Initialize supported DLC modules.
    supported_ = utils::ScanDirectory(manifest_dir_);
  }
  ~DlcManagerImpl() = default;

  bool IsInstalling() { return !installing_.empty(); }

  std::set<DlcId> GetSupported() {
    RefreshInstalled();
    return supported_;
  }

  DlcRootMap GetInstalled() {
    RefreshInstalled();
    return installed_;
  }

  bool InitInstall(const DlcRootMap& requested_install,
                   string* err_code,
                   string* err_msg) {
    CHECK(installing_.empty());
    RefreshInstalled();
    installing_ = requested_install;

    for (const auto& dlc : installing_) {
      const string& id = dlc.first;
      // If already installed, pick up the root.
      if (installed_.find(id) != installed_.end()) {
        installing_[id] = installed_[id];
      } else {
        string throwaway_err_code, throwaway_err_msg;
        if (!Create(id, err_code, err_msg)) {
          CancelInstall(&throwaway_err_code, &throwaway_err_msg);
          return false;
        }
        // Failure to set the metadata flags should not fail the install.
        if (!SetActive(id, &throwaway_err_code, &throwaway_err_msg)) {
          LOG(WARNING) << throwaway_err_msg;
        }
      }
    }
    return true;
  }

  DlcRootMap GetInstalling() {
    DlcRootMap required_installing;
    for (const auto& dlc : installing_)
      if (dlc.second.empty())
        required_installing[dlc.first];
    return required_installing;
  }

  bool FinishInstall(DlcRootMap* installed, string* err_code, string* err_msg) {
    *installed = installing_;

    utils::ScopedCleanups<base::Callback<void()>> scoped_cleanups;

    for (const auto& dlc : installing_) {
      const string& id = dlc.first;
      string cleanup_err_code, cleanup_err_msg;
      auto cleanup = base::Bind(
          [](Callback<bool()> unmounter, Callback<bool()> deleter,
             string* err_msg) {
            unmounter.Run();
            if (!deleter.Run())
              LOG(ERROR) << *err_msg;
          },
          base::Bind(&DlcManagerImpl::Unmount, base::Unretained(this), id,
                     &cleanup_err_code, &cleanup_err_msg),
          base::Bind(&DlcManagerImpl::Delete, base::Unretained(this), id,
                     &cleanup_err_code, &cleanup_err_msg),
          &cleanup_err_msg);
      scoped_cleanups.Insert(cleanup);
    }

    for (auto& dlc : installing_) {
      const string &id = dlc.first, root = dlc.second;
      if (!root.empty())
        continue;
      string mount_point;
      if (!Mount(id, &mount_point, err_code, err_msg))
        return false;
      dlc.second = mount_point;
    }

    scoped_cleanups.Cancel();

    for (const auto& dlc : installing_) {
      const string &id = dlc.first, root = dlc.second;
      installed_[id] = installed->operator[](id) = root;
    }

    installing_.clear();
    return true;
  }

  bool CancelInstall(string* err_code, string* err_msg) {
    bool ret = true;
    if (installing_.empty()) {
      LOG(WARNING) << "No install started to being with, nothing to cancel.";
      return ret;
    }
    for (const auto& dlc : installing_) {
      const string &id = dlc.first, root = dlc.second;
      if (!root.empty())
        continue;
      if (!Delete(id, err_code, err_msg)) {
        LOG(ERROR) << *err_msg;
        ret = false;
      }
    }
    installing_.clear();
    return ret;
  }

  bool Delete(const string& id, string* err_code, string* err_msg) {
    if (!DeleteInternal(id, err_code, err_msg))
      return false;
    installed_.erase(id);
    return true;
  }

  bool Mount(const string& id,
             string* mount_point,
             string* err_code,
             string* err_msg) {
    if (!image_loader_proxy_->LoadDlcImage(
            id, GetDlcPackage(id),
            current_boot_slot_ == BootSlot::Slot::A ? imageloader::kSlotNameA
                                                    : imageloader::kSlotNameB,
            mount_point, nullptr)) {
      *err_code = kErrorInternal;
      *err_msg = "Imageloader is unavailable.";
      return false;
    }
    if (mount_point->empty()) {
      *err_code = kErrorInternal;
      *err_msg = "Imageloader LoadDlcImage() call failed.";
      return false;
    }
    return true;
  }

  bool Unmount(const string& id, string* err_code, string* err_msg) {
    bool success = false;
    if (!image_loader_proxy_->UnloadDlcImage(id, GetDlcPackage(id), &success,
                                             nullptr)) {
      *err_code = kErrorInternal;
      *err_msg = "Imageloader is unavailable.";
      return false;
    }
    if (!success) {
      *err_code = kErrorInternal;
      *err_msg = "Imageloader UnloadDlcImage() call failed.";
      return false;
    }
    return true;
  }

 private:
  string GetDlcPackage(const string& id) {
    return *(utils::ScanDirectory(manifest_dir_.Append(id)).begin());
  }

  bool SetActive(const string& id, string* err_code, string* err_msg) {
    // Create the DLC ID metadata directory with correct permissions if it
    // doesn't exist.
    FilePath metadata_path_local = utils::GetDlcPath(metadata_dir_, id);
    if (!base::PathExists(metadata_path_local)) {
      if (!CreateDirWithDlcPermissions(metadata_path_local)) {
        *err_code = kErrorInternal;
        *err_msg = "Failed to create the DLC metadata directory for DLC:" + id;
        return false;
      }
    }
    FilePath active_metadata =
        utils::GetDlcPath(metadata_dir_, id)
            .Append(dlcservice::kDlcMetadataFilePingActive);
    base::ScopedFILE active_metadata_fp(base::OpenFile(active_metadata, "w"));
    if (active_metadata_fp == nullptr) {
      *err_code = kErrorInternal;
      *err_msg = "Failed to open 'active' metadata file for DLC:" + id;
      return false;
    }
    // Set 'active' value to true.
    if (!base::WriteFileDescriptor(fileno(active_metadata_fp.get()),
                                   dlcservice::kDlcMetadataActiveValue, 1)) {
      *err_code = kErrorInternal;
      *err_msg = "Failed to write into active metadata file for DLC:" + id;
      return false;
    }
    return true;
  }

  bool Create(const string& id, string* err_code, string* err_msg) {
    CHECK(err_code);
    CHECK(err_msg);

    if (supported_.find(id) == supported_.end()) {
      *err_code = kErrorInvalidDlc;
      *err_msg = "The DLC(" + id + ") provided is not supported.";
      return false;
    }

    const string& package = GetDlcPackage(id);
    FilePath content_path_local = utils::GetDlcPath(content_dir_, id);
    FilePath content_package_path =
        utils::GetDlcPackagePath(content_dir_, id, package);

    if (base::PathExists(content_path_local)) {
      *err_code = kErrorInternal;
      *err_msg = "The DLC(" + id + ") is installed or duplicate.";
      return false;
    }
    // Create the DLC ID directory with correct permissions.
    if (!CreateDirWithDlcPermissions(content_path_local)) {
      *err_code = kErrorInternal;
      *err_msg = "Failed to create DLC(" + id + ") directory";
      return false;
    }
    // Create the DLC package directory with correct permissions.
    if (!CreateDirWithDlcPermissions(content_package_path)) {
      *err_code = kErrorInternal;
      *err_msg = "Failed to create DLC(" + id + ") package directory";
      return false;
    }

    // Creates DLC module storage.
    // TODO(xiaochu): Manifest currently returns a signed integer, which means
    // it will likely fail for modules >= 2 GiB in size.
    // https://crbug.com/904539
    imageloader::Manifest manifest;
    if (!dlcservice::utils::GetDlcManifest(manifest_dir_, id, package,
                                           &manifest)) {
      *err_code = kErrorInternal;
      *err_msg = "Failed to create DLC(" + id + ") manifest.";
      return false;
    }
    int64_t image_size = manifest.preallocated_size();
    if (image_size <= 0) {
      *err_code = kErrorInternal;
      *err_msg = "Preallocated size in manifest is illegal: " +
                 base::Int64ToString(image_size);
      return false;
    }

    // Creates image A.
    FilePath image_a_path =
        utils::GetDlcImagePath(content_dir_, id, package, BootSlot::Slot::A);
    if (!CreateImageFile(image_a_path, image_size)) {
      *err_code = kErrorInternal;
      *err_msg = "Failed to create slot A DLC(" + id + ") image file.";
      return false;
    }

    // Creates image B.
    FilePath image_b_path =
        utils::GetDlcImagePath(content_dir_, id, package, BootSlot::Slot::B);
    if (!CreateImageFile(image_b_path, image_size)) {
      *err_code = kErrorInternal;
      *err_msg = "Failed to create slot B DLC(" + id + ") image file.";
      return false;
    }

    return true;
  }

  bool DeleteInternal(const string& id, string* err_code, string* err_msg) {
    for (const auto& path : {utils::GetDlcPath(content_dir_, id),
                             utils::GetDlcPath(metadata_dir_, id)}) {
      if (!base::DeleteFile(path, true)) {
        *err_code = kErrorInternal;
        *err_msg = base::StringPrintf("DLC folder(%s) could not be deleted.",
                                      path.value().c_str());
        return false;
      }
    }
    return true;
  }

  void RefreshInstalled() {
    // Recheck installed DLC modules.
    for (auto installed_dlc_id : utils::ScanDirectory(content_dir_))
      installed_[installed_dlc_id];

    for (auto installed_dlc_module_itr = installed_.begin();
         installed_dlc_module_itr != installed_.end();
         /* Don't increment here */) {
      const string& installed_dlc_module_id = installed_dlc_module_itr->first;
      string& installed_dlc_module_root = installed_dlc_module_itr->second;
      string err_code, err_msg;

      // Set the DLCs to active.
      if (!SetActive(installed_dlc_module_id, &err_code, &err_msg)) {
        LOG(WARNING) << err_msg;
      }

      if (base::PathExists(FilePath(installed_dlc_module_root))) {
        ++installed_dlc_module_itr;
        continue;
      }

      string mount_point;
      if (!Mount(installed_dlc_module_id, &mount_point, &err_code, &err_msg)) {
        LOG(ERROR) << "Failed to mount DLC module during refresh: "
                   << installed_dlc_module_id;
        if (!DeleteInternal(installed_dlc_module_id, &err_code, &err_msg)) {
          LOG(ERROR) << "Failed to delete an unmountable DLC module: "
                     << installed_dlc_module_id;
        }
        installed_.erase(installed_dlc_module_itr++);
      } else {
        installed_dlc_module_root =
            utils::GetDlcRootInModulePath(FilePath(mount_point)).value();
        ++installed_dlc_module_itr;
      }
    }
  }

  std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
      image_loader_proxy_;

  FilePath manifest_dir_;
  FilePath content_dir_;
  FilePath metadata_dir_;

  BootSlot::Slot current_boot_slot_;

  string installing_omaha_url_;
  DlcRootMap installing_;
  DlcRootMap installed_;
  std::set<DlcId> supported_;
};

DlcManager::DlcManager(
    unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
        image_loader_proxy,
    unique_ptr<BootSlot> boot_slot,
    const FilePath& manifest_dir,
    const FilePath& content_dir,
    const FilePath& metadata_dir) {
  impl_ = std::make_unique<DlcManagerImpl>(std::move(image_loader_proxy),
                                           std::move(boot_slot), manifest_dir,
                                           content_dir, metadata_dir);
}

DlcManager::~DlcManager() = default;

bool DlcManager::IsInstalling() {
  return impl_->IsInstalling();
}

DlcModuleList DlcManager::GetInstalled() {
  return utils::ToDlcModuleList(impl_->GetInstalled(),
                                [](DlcId, DlcRoot) { return true; });
}

bool DlcManager::InitInstall(const DlcModuleList& dlc_module_list,
                             string* err_code,
                             string* err_msg) {
  CHECK(err_code);
  CHECK(err_msg);
  DlcRootMap dlc_root_map =
      utils::ToDlcRootMap(dlc_module_list, [](DlcModuleInfo) { return true; });

  if (dlc_module_list.dlc_module_infos().empty()) {
    *err_code = kErrorInvalidDlc;
    *err_msg = "Must provide at lease one DLC to install.";
    return false;
  }

  if (dlc_root_map.size() != dlc_module_list.dlc_module_infos_size()) {
    *err_code = kErrorInvalidDlc;
    *err_msg = "Must not pass in duplice DLC(s) to install.";
    return false;
  }

  return impl_->InitInstall(dlc_root_map, err_code, err_msg);
}

DlcModuleList DlcManager::GetMissingInstalls() {
  // Only return the DLC(s) that aren't already installed.
  return utils::ToDlcModuleList(
      impl_->GetInstalling(), [](DlcId, DlcRoot root) { return root.empty(); });
}

bool DlcManager::FinishInstall(DlcModuleList* dlc_module_list,
                               string* err_code,
                               string* err_msg) {
  CHECK(dlc_module_list);
  CHECK(err_code);
  CHECK(err_msg);

  DlcRootMap dlc_root_map;
  if (!impl_->FinishInstall(&dlc_root_map, err_code, err_msg))
    return false;

  *dlc_module_list =
      utils::ToDlcModuleList(dlc_root_map, [](DlcId id, DlcRoot root) {
        CHECK(!id.empty());
        CHECK(!root.empty());
        return true;
      });
  return true;
}

bool DlcManager::CancelInstall(std::string* err_code, std::string* err_msg) {
  return impl_->CancelInstall(err_code, err_msg);
}

bool DlcManager::Delete(const string& id,
                        std::string* err_code,
                        std::string* err_msg) {
  CHECK(err_code);
  CHECK(err_msg);

  auto supported_dlcs = impl_->GetSupported();
  if (supported_dlcs.find(id) == supported_dlcs.end()) {
    *err_code = kErrorInvalidDlc;
    *err_msg = "Trying to delete DLC(" + id + ") which isn't supported.";
    return false;
  }
  auto installed_dlcs = impl_->GetInstalled();
  if (installed_dlcs.find(id) == installed_dlcs.end()) {
    LOG(WARNING) << "Uninstalling DLC(" << id << ") that's not installed.";
    return true;
  }
  if (!impl_->Unmount(id, err_code, err_msg))
    return false;
  if (!impl_->Delete(id, err_code, err_msg))
    return false;
  return true;
}

}  // namespace dlcservice
