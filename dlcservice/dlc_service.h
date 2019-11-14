// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DLCSERVICE_DLC_SERVICE_H_
#define DLCSERVICE_DLC_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/memory/weak_ptr.h>
#include <dlcservice/proto_bindings/dlcservice.pb.h>
#include <imageloader/dbus-proxies.h>
#include <update_engine/proto_bindings/update_engine.pb.h>
#include <update_engine/dbus-proxies.h>

#include "dlcservice/boot_slot.h"
#include "dlcservice/types.h"

namespace dlcservice {

// DlcService manages life-cycles of DLCs (Downloadable Content) and provides an
// API for the rest of the system to install/uninstall DLCs.
class DlcService {
 public:
  // |DlcService| calls the registered implementation of this class when a
  // |StatusResult| signal needs to be propagated.
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void SendInstallStatus(const InstallStatus& status) = 0;
  };

  DlcService(std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
                 image_loader_proxy,
             std::unique_ptr<org::chromium::UpdateEngineInterfaceProxyInterface>
                 update_engine_proxy,
             std::unique_ptr<BootSlot> boot_slot,
             const base::FilePath& manifest_dir,
             const base::FilePath& content_dir);
  ~DlcService() = default;

  // Loads installed DLC module images.
  void LoadDlcModuleImages();

  bool Install(const DlcModuleList& dlc_module_list_in, brillo::ErrorPtr* err);
  bool Uninstall(const std::string& id_in, brillo::ErrorPtr* err);
  bool GetInstalled(DlcModuleList* dlc_module_list_out, brillo::ErrorPtr* err);

  // Adds a new observer to report install result status changes.
  void AddObserver(Observer* observer);
  // Called on receiving update_engine's |StatusUpdate| signal.
  void OnStatusUpdateAdvancedSignal(
      const update_engine::StatusResult& status_result);

 private:
  // Sends a signal indicating failure to install and cleans up prepped DLC(s).
  void SendFailedSignalAndCleanup();

  // Handles necessary actions prior to update_engine's install completion, but
  // when update_engine's install is complete it will return true.
  bool HandleStatusResult(const update_engine::StatusResult& status_result);

  // Creates the necessary directories and images for DLC installation. Will set
  // |path| to the top DLC directory for cleanup scoping.
  bool CreateDlc(const std::string& id,
                 base::FilePath* path,
                 brillo::ErrorPtr* err);

  // Deletes the DLC installation.
  bool DeleteDlc(const std::string& id, brillo::ErrorPtr* err);

  // Tries to mount DLC images.
  bool MountDlc(const std::string& id,
                std::string* mount_point,
                brillo::ErrorPtr* err);

  // Tries to unmount DLC images.
  bool UnmountDlc(const std::string& id, brillo::ErrorPtr* err);

  // Scans a specific DLC |id| to discover all its packages. Currently, we only
  // support one package per DLC. If at some point in the future we decided to
  // support multiple packages, then appropriate changes to this function is
  // granted.
  std::string ScanDlcModulePackage(const std::string& id);

  // Gets update_engine's operation status.
  bool GetUpdateEngineStatus(update_engine::Operation* operation);

  // Send |OnInstallStatus| D-Bus signal.
  void SendOnInstallStatusSignal(const InstallStatus& install_status);

  // Called on being connected to update_engine's |StatusUpdate| signal.
  void OnStatusUpdateAdvancedSignalConnected(const std::string& interface_name,
                                             const std::string& signal_name,
                                             bool success);

  std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
      image_loader_proxy_;
  std::unique_ptr<org::chromium::UpdateEngineInterfaceProxyInterface>
      update_engine_proxy_;
  std::unique_ptr<BootSlot> boot_slot_;

  base::FilePath manifest_dir_;
  base::FilePath content_dir_;

  // DLC modules being installed. An empty module infos signifies no install.
  DlcModuleList dlc_modules_being_installed_;

  std::string current_boot_slot_name_;

  // Map of currently existing DLC modules with it's corresponding DLC root.
  DlcRootMap installed_dlc_modules_;

  // List of allowed DLC modules based on manifest. Use a set in order to handle
  // repeats implicitly.
  std::set<DlcId> supported_dlc_modules_;

  // The list of observers that will be called when a new status is ready.
  std::vector<Observer*> observers_;

  base::WeakPtrFactory<DlcService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DlcService);
};

}  // namespace dlcservice

#endif  // DLCSERVICE_DLC_SERVICE_H_
