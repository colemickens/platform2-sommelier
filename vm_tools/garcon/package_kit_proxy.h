// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_GARCON_PACKAGE_KIT_PROXY_H_
#define VM_TOOLS_GARCON_PACKAGE_KIT_PROXY_H_

#include <memory>
#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/observer_list.h>
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

  typedef base::Callback<void(bool success,
                              bool pkg_found,
                              const LinuxPackageInfo& pkg_info,
                              const std::string& error)>
      PackageSearchCallback;

  // Creates an instance of PackageKitProxy that will use the calling thread for
  // its message loop for D-Bus communication. Returns nullptr if there was a
  // failure.
  static std::unique_ptr<PackageKitProxy> Create(
      base::WeakPtr<PackageKitObserver> observer);

  ~PackageKitProxy();

  // Returns a WeakPtr reference to this object.
  base::WeakPtr<PackageKitProxy> GetWeakPtr();

  // Gets the information about a local Linux package file located at
  // |file_path| and populates |out_pkg_info| with the details on success.
  // Returns true on success, and false otherwise. On failure, |out_error| will
  // be populated with error details.
  bool GetLinuxPackageInfo(const base::FilePath& file_path,
                           std::shared_ptr<LinuxPackageInfo> out_pkg_info,
                           std::string* out_error);

  // Gets information about the Linux package (if any) which owns the file
  // located at |file_path|. Once the transaction is complete, |callback| will
  // be called. If a package which owns the file is found, |success| and
  // |pkg_found| will be true and |pkg_info| will be filled in with some package
  // details. If there is no such package, |pkg_found| will be set
  // to false, |pkg_info| will be empty, but |success| will be true --
  // this is not an error. On error, |success| will be false and |error| will be
  // populated with the error details. Regardless, |callback| will be called
  // only once.
  //
  // The returned LinuxPackageInfo will only have package_id and summary filled
  // in.
  //
  // Only installed packages are considered. This function is intended for use
  // by uninstallers and similar systems that care only about .desktop files
  // that are on the local files system, so we don't care about uninstalled
  // packages.
  //
  // Should only be called on D-Bus thread. Caller must not block D-Bus thread.
  void SearchLinuxPackagesForFile(const base::FilePath& file_path,
                                  PackageSearchCallback callback);

  // Requests that installation of the Linux package located at |file_path| be
  // performed. Returns the result which corresponds to the
  // InstallLinuxPackageResponse::Status enum. |out_error| will be set in the
  // case of failure.
  int InstallLinuxPackage(const base::FilePath& file_path,
                          std::string* out_error);

  // For use by this implementation only, these are public because helper
  // classes also utilize them.
  struct PackageInfoTransactionData {
    PackageInfoTransactionData(const base::FilePath& file_path_in,
                               std::shared_ptr<LinuxPackageInfo> pkg_info_in);
    const base::FilePath file_path;
    base::WaitableEvent event;
    bool result;
    std::shared_ptr<LinuxPackageInfo> pkg_info;
    std::string error;
  };
  class PackageKitDeathObserver {
   public:
    virtual ~PackageKitDeathObserver() {}
    // Invoked when the name owner changed signal is received indicating loss
    // of ownership.
    virtual void OnPackageKitDeath() = 0;
  };
  void AddPackageKitDeathObserver(PackageKitDeathObserver* observer);
  void RemovePackageKitDeathObserver(PackageKitDeathObserver* observer);

 private:
  explicit PackageKitProxy(base::WeakPtr<PackageKitObserver> observer);
  bool Init();
  void GetLinuxPackageInfoOnDBusThread(
      std::shared_ptr<PackageInfoTransactionData> data);
  void InstallLinuxPackageOnDBusThread(const base::FilePath& file_path,
                                       base::WaitableEvent* event,
                                       int* status,
                                       std::string* out_error);

  // Callback for ownership change of PackageKit service, used to detect if it
  // crashes while we are waiting on something that doesn't have a timeout.
  void OnPackageKitNameOwnerChanged(const std::string& old_owner,
                                    const std::string& new_owner);

  // Callback for PackageKit service availability, this needs to be called in
  // order for name ownership change events to come through.
  void OnPackageKitServiceAvailable(bool service_is_available);

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* packagekit_service_proxy_;  // Owned by |bus_|.

  base::WeakPtr<PackageKitObserver> observer_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Ensure calls are made on the right thread.
  base::SequenceChecker sequence_checker_;

  base::ObserverList<PackageKitDeathObserver> death_observers_;

  base::WeakPtrFactory<PackageKitProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PackageKitProxy);
};

}  // namespace garcon
}  // namespace vm_tools

#endif  // VM_TOOLS_GARCON_PACKAGE_KIT_PROXY_H_
