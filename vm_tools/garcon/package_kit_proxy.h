// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_GARCON_PACKAGE_KIT_PROXY_H_
#define VM_TOOLS_GARCON_PACKAGE_KIT_PROXY_H_

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/sequence_checker.h>
#include <dbus/bus.h>
#include <dbus/object_proxy.h>

#include "container_host.grpc.pb.h"  // NOLINT(build/include)

namespace vm_tools {
namespace garcon {

// Proxy for communicating with the PackageKit daemon over D-Bus. This is used
// for handling software installation/removal.
class PackageKitProxy {
 public:
  class PackageKitObserver {
   public:
    virtual ~PackageKitObserver() {}
    virtual void OnInstallCompletion(bool success,
                                     const std::string& failure_reason) = 0;
    virtual void OnInstallProgress(
        vm_tools::container::InstallLinuxPackageProgressInfo::Status status,
        uint32_t percent_progress) = 0;
  };
  struct LinuxPackageInfo {
    std::string package_id;
    std::string license;
    std::string description;
    std::string project_url;
    uint64_t size;
    std::string summary;
  };

  // Creates an instance of PackageKitProxy that will use the calling thread for
  // its message loop for D-Bus communication. Returns nullptr if there was a
  // failure.
  static std::unique_ptr<PackageKitProxy> Create(
      base::WeakPtr<PackageKitObserver> observer);

  ~PackageKitProxy();

  // Gets the information about a local Linux package file located at
  // |file_path| and populates |out_pkg_info| with the details on success.
  // Returns true on success, and false otherwise. On failure, |out_error| will
  // be populated with error details.
  bool GetLinuxPackageInfo(const base::FilePath& file_path,
                           std::shared_ptr<LinuxPackageInfo> out_pkg_info,
                           std::string* out_error);

  // Requests that installation of the Linux package located at |file_path| be
  // performed. Returns the result which corresponds to the
  // InstallLinuxPackageResponse::Status enum. |out_error| will be set in the
  // case of failure.
  int InstallLinuxPackage(const base::FilePath& file_path,
                          std::string* out_error);

 private:
  struct PackageKitTransactionProperties;
  // This is used to hold various objects/data that we pass around through
  // callbacks when querying for package information. It's in a struct to avoid
  // huge argument lists for the method calls.
  struct PackageInfoTransactionData {
    PackageInfoTransactionData(const base::FilePath& file_path_in,
                               std::shared_ptr<LinuxPackageInfo> pkg_info_in);
    dbus::ObjectPath info_transaction_path;
    const base::FilePath file_path;
    base::WaitableEvent event;
    bool result;
    std::shared_ptr<LinuxPackageInfo> pkg_info;
    std::string error;
  };

  explicit PackageKitProxy(base::WeakPtr<PackageKitObserver> observer);
  bool Init();
  void GetLinuxPackageInfoOnDBusThread(
      std::shared_ptr<PackageInfoTransactionData> data);
  void InstallLinuxPackageOnDBusThread(const base::FilePath& file_path,
                                       base::WaitableEvent* event,
                                       int* status,
                                       std::string* out_error);

  // Called to refresh the package lists from the remote repositories.
  void RefreshCacheOnDBusThread();

  // Called to get the list of packages that have updates that are managed by
  // us. If anything is updatable it will then initiate the upgrade.
  void GetUpdatableManagedPackagesOnDBusThread();

  // Attempts to perform upgrades on the passed in package IDs.
  void UpgradePackagesOnDBusThread(
      std::shared_ptr<std::vector<std::string>> package_ids);

  // Calbacks for when D-Bus signals are connected for an install request.
  void OnErrorSignalConnectedForInstall(dbus::ObjectProxy* transaction_proxy,
                                        const base::FilePath& file_path,
                                        const std::string& interface_name,
                                        const std::string& signal_name,
                                        bool is_connected);
  void OnFinishedSignalConnectedForInstall(dbus::ObjectProxy* transaction_proxy,
                                           const base::FilePath& file_path,
                                           const std::string& interface_name,
                                           const std::string& signal_name,
                                           bool is_connected);

  // Callbacks for when D-Bus signals are connected for an info request.
  void OnErrorSignalConnectedForInfo(
      dbus::ObjectProxy* transaction_proxy,
      std::shared_ptr<PackageInfoTransactionData> data,
      const std::string& interface_name,
      const std::string& signal_name,
      bool is_connected);
  void OnFinishedSignalConnectedForInfo(
      dbus::ObjectProxy* transaction_proxy,
      std::shared_ptr<PackageInfoTransactionData> data,
      const std::string& interface_name,
      const std::string& signal_name,
      bool is_connected);
  void OnDetailsSignalConnectedForInfo(
      dbus::ObjectProxy* transaction_proxy,
      std::shared_ptr<PackageInfoTransactionData> data,
      const std::string& interface_name,
      const std::string& signal_name,
      bool is_connected);

  // Callbacks for when D-Bus signals are connected for a cache refresh.
  void OnErrorSignalConnectedForRefresh(
      dbus::ObjectProxy* transaction_proxy,
      const dbus::ObjectPath& transaction_path,
      const std::string& interface_name,
      const std::string& signal_name,
      bool is_connected);
  void OnFinishedSignalConnectedForRefresh(dbus::ObjectProxy* transaction_proxy,
                                           const std::string& interface_name,
                                           const std::string& signal_name,
                                           bool is_connected);

  // Callbacks for when D-Bus signals are connected for a GetUpdates.
  void OnErrorSignalConnectedForGetUpdates(
      dbus::ObjectProxy* transaction_proxy,
      const dbus::ObjectPath& transaction_path,
      const std::string& interface_name,
      const std::string& signal_name,
      bool is_connected);
  void OnFinishedSignalConnectedForGetUpdates(
      dbus::ObjectProxy* transaction_proxy,
      std::shared_ptr<std::vector<std::string>> package_ids,
      const std::string& interface_name,
      const std::string& signal_name,
      bool is_connected);
  void OnPackageSignalConnectedForGetUpdates(
      dbus::ObjectProxy* transaction_proxy,
      const std::string& interface_name,
      const std::string& signal_name,
      bool is_connected);

  // Callback for when D-Bus signals are connected for an upgrade.
  void OnErrorSignalConnectedForUpgrade(
      dbus::ObjectProxy* transaction_proxy,
      const dbus::ObjectPath& transaction_path,
      std::shared_ptr<std::vector<std::string>> package_ids,
      const std::string& interface_name,
      const std::string& signal_name,
      bool is_connected);
  void OnFinishedSignalConnectedForUpgrade(
      dbus::ObjectProxy* transaction_proxy,
      std::shared_ptr<std::vector<std::string>> package_ids,
      const std::string& interface_name,
      const std::string& signal_name,
      bool is_connected);

  // Callback for the D-Bus signals for ErrorCode, Finished and Changed during
  // installs.
  void OnPackageKitInstallError(dbus::Signal* signal);
  void OnPackageKitInstallFinished(dbus::Signal* signal);
  void OnPackageKitPropertyChanged(const std::string& name);

  // Callback for the D-Bus signal for ErrorCode, Finished and Details during
  // info requests.
  void OnPackageKitInfoError(std::shared_ptr<PackageInfoTransactionData> data,
                             dbus::Signal* signal);
  void OnPackageKitInfoFinished(
      std::shared_ptr<PackageInfoTransactionData> data, dbus::Signal* signal);
  void OnPackageKitInfoDetails(std::shared_ptr<PackageInfoTransactionData> data,
                               dbus::Signal* signal);

  // Callback for the D-Bus signals for ErrorCode and Finished during refreshes.
  void OnPackageKitRefreshError(dbus::Signal* signal);
  void OnPackageKitRefreshFinished(const dbus::ObjectPath& transaction_path,
                                   dbus::Signal* signal);

  // Callback for the D-Bus signals for ErrorCode, Finished and Package during
  // GetUpdates.
  void OnPackageKitGetUpdatesError(dbus::Signal* signal);
  void OnPackageKitGetUpdatesPackage(
      std::shared_ptr<std::vector<std::string>> package_ids,
      dbus::Signal* signal);
  void OnPackageKitGetUpdatesFinished(
      const dbus::ObjectPath& transaction_path,
      std::shared_ptr<std::vector<std::string>> package_ids,
      dbus::Signal* signal);

  // Callback for the D-Bus signal for ErrorCode Finished during upgrades.
  void OnPackageKitUpgradeError(dbus::Signal* signal);
  void OnPackageKitUpgradeFinished(const dbus::ObjectPath& transaction_path,
                                   dbus::Signal* signal);

  // Called to clear local state for an install operation and make a call to the
  // observer with the result.
  void HandleInstallCompletion(bool success, const std::string& failure_reason);

  // Called to schedule the next refresh of the package cache.
  void ScheduleNextCacheRefresh();

  // Callback for ownership change of PackageKit service, used to detect if it
  // crashes while we are waiting on something that doesn't have a timeout.
  void OnPackageKitNameOwnerChanged(const std::string& old_owner,
                                    const std::string& new_owner);

  // Callback for PackageKit service availability, this needs to be called in
  // order for name ownership change events to come through.
  void OnPackageKitServiceAvailable(bool service_is_available);

  // Used to cleanup transaction ObjectProxy objects on the D-Bus thread.
  void RemoveObjectProxyOnDBusThread(const dbus::ObjectPath& object_path);

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* packagekit_service_proxy_;  // Owned by |bus_|.
  dbus::ObjectPath install_transaction_path_;

  base::WeakPtr<PackageKitObserver> observer_;
  std::unique_ptr<PackageKitTransactionProperties> transaction_properties_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Ensure calls are made on the right thread.
  base::SequenceChecker sequence_checker_;

  base::WeakPtrFactory<PackageKitProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PackageKitProxy);
};

}  // namespace garcon
}  // namespace vm_tools

#endif  // VM_TOOLS_GARCON_PACKAGE_KIT_PROXY_H_
