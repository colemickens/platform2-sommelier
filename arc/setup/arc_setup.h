// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_SETUP_ARC_SETUP_H_
#define ARC_SETUP_ARC_SETUP_H_

#include <memory>
#include <string>
#include <utility>

#include <base/macros.h>

#include "arc_setup_util.h"  // NOLINT - TODO(b/32971714): fix it properly.

namespace arc {

struct ArcPaths;
class ArcSetupMetrics;

// This MUST be in sync with 'enum BootType' in metrics.mojom.
enum class ArcBootType {
  UNKNOWN = 0,
  // This is for the very first (opt-in) boot.
  FIRST_BOOT = 1,
  // This is for the first boot after Chrome OS update which also updates the
  // ARC++ image.
  FIRST_BOOT_AFTER_UPDATE = 2,
  // This is for regular boot.
  REGULAR_BOOT = 3,
};

// Enum that describes which native bridge mode is used to run arm binaries on
// x86.
enum class ArcBinaryTranslationType {
  NONE = 0,
  HOUDINI = 1,
  NDK_TRANSLATION = 2,
};

// A class that does the actual setup (and stop) operations.
class ArcSetup {
 public:
  ArcSetup();
  ~ArcSetup();

  // Does the setup or stop operations depending on the environment variable.
  // The return value is void because the function aborts when one or more of
  // the operations fail.
  void Run();

  void set_arc_mounter_for_testing(std::unique_ptr<ArcMounter> mounter) {
    arc_mounter_ = std::move(mounter);
  }
  const ArcMounter* arc_mounter_for_testing() const {
    return arc_mounter_.get();
  }

  void MountOnOnetimeSetupForTesting();
  void UnmountOnOnetimeStopForTesting();

 private:
  // Clears executables /data/app/*/oat and /data/dalvik-cache.
  void DeleteExecutableFilesInData(
      bool should_delete_data_dalvik_cache_directory,
      bool should_delete_data_app_executables);

  // Waits for the rt-limits job to start.
  void WaitForRtLimitsJob();

  // Waits for directories in /run the container depends on to be created.
  void WaitForArcDirectories();

  // Remounts /vendor
  void RemountVendorDirectory();

  // Sets up binfmt_misc handler to run arm binaries on x86
  void SetUpBinFmtMisc(ArcBinaryTranslationType bin_type);

  // Sets up a dummy android-data directory on tmpfs.
  void SetUpDummyAndroidDataOnTmpfs();

  // Sets up android-data directory.
  void SetUpAndroidData();

  // Sets up shared APK cache directory.
  void SetUpSharedApkDirectory();

  // Sets up android-data/data/dalvik-cache/<isa> directory for login screen.
  void SetUpDalvikCacheInternal(const base::FilePath& dalvik_cache_directory,
                                const base::FilePath& isa_relative);

  // Sets up android-data/data/dalvik-cache directory for login screen.
  void SetUpDalvikCache();

  // Creates directories needed by containers.
  void CreateContainerDirectories();

  // Detects and applies per-board hardware configurations.
  void ApplyPerBoardConfigurations();

  // Starts ureadahead-trace if the pack file doesn't exist.
  void MaybeStartUreadaheadInTracingMode();

  // Sets up shared tmpfs to share mount points for external storage.
  void SetUpSharedTmpfsForExternalStorage();

  // Sets up /mnt filesystem for arc-obb-mounter.
  void SetUpFilesystemForObbMounter();

  // Generates boot*.art files on host side and stores them in
  // |host_dalvik_cache_directory|. Returns true on success.
  bool GenerateHostSideCodeInternal(
      const base::FilePath& host_dalvik_cache_directory);

  // Calls GenerateHostSideCodeInternal(). If the internal function returns
  // false, deletes all files in |host_dalvik_cache_directory|.
  bool GenerateHostSideCode(const base::FilePath& host_dalvik_cache_directory);

  // Calls InstallHostSideCodeInternal() for each isa the device supports.
  bool InstallLinksToHostSideCode();

  // Installs links to *boot*.{art,oat} files to |dest_isa_directory|. Returns
  // false when |src_isa_directory| is empty.
  bool InstallLinksToHostSideCodeInternal(
      const base::FilePath& src_isa_directory,
      const base::FilePath& dest_isa_directory,
      const std::string& isa);

  // Provides some fake kernel command line arguments that are needed.
  void CreateAndroidCmdlineFile(bool for_login_screen,
                                bool is_dev_mode,
                                bool is_inside_vm,
                                bool is_debuggable,
                                ArcBinaryTranslationType bin_type,
                                ArcBootType boot_type);

  // Create fake procfs entries expected by android.
  void CreateFakeProcfsFiles();

  // Bind-mounts /sys/kernel/debug/tracing to
  // |arc_paths.debugfs_directory|/tracing.
  void SetUpMountPointForDebugFilesystem(bool is_dev_mode);

  // Sets up a mount point for arc-removable-media.
  void SetUpMountPointForRemovableMedia();

  // Cleans up mount points in |android_mutable_source|/data/dalvik-cache/.
  void CleanUpDalvikCache();

  // Cleans up mount points for arc-removable-media if any.
  void CleanUpStaleMountPoints();

  // Sets up a mount point for passing the user's /data and /cache to the
  // container.
  void SetUpSharedMountPoints(bool for_login_screen);

  // Restores the labels of files and directories. This has to be called before
  // MakeMountpointsReadOnly() makes the directories read-only.
  void RestoreContext();

  // Initializes SELinux contexts of SysFS entries necessary to enumerate
  // graphics hardware. Cannot be done using RestoreContext(), because
  // symbolic links are involved.
  void SetUpGraphicsSysfsContext();

  // TODO(b/68814859): Remove this function once WiFi works again.  Creates
  // /data/misc/ethernet/ipconfig.txt so that the container can set up
  // networking by itself.
  //
  // This file is populated with a static address and default gateway from the
  // ARC_CONTAINER_IPV4_ADDRESS and ARC_GATEWAY_IPV4_ADDRESS environment
  // variables respectively.  The Google Public DNS servers (8.8.8.8 and
  // 8.8.4.4) as set as the DNS resolvers.
  void SetUpNetwork();

  // Makes some mount points read-only.
  void MakeMountPointsReadOnly();

  // Sets up a subset property file for camera.
  void SetUpCameraProperty();

  // Cleans up binfmt_misc handlers that run arm binaries on x86.
  void CleanUpBinFmtMiscSetUp();

  // Unmounts files and directories that are not necessary after shutting down
  // the container.
  void UnmountOnStop();

  // Removes the pipe for 'bugreport'.
  void RemoveBugreportPipe();

  // Gets a fingerprint in the build.prop file. Since the file is in our system
  // image, the operation should never fail.
  std::string GetSystemImageFingerprint();

  // Returns the boot type.
  ArcBootType GetBootType();

  // Checks if arc-setup should clobber /data/dalvik-cache and /data/app/*/oat
  // before starting the container.
  void ShouldDeleteDataExecutables(
      ArcBootType boot_type,
      bool* out_should_delete_data_dalvik_cache_directory,
      bool* out_should_delete_data_app_executables);

  // Reads the 16-byte per-machine random salt. The salt is created once when
  // the machine is first used, and wiped/regenerated on powerwash/recovery.
  // When it's not available yet (which could happen only on OOBE boot), returns
  // an empty string.
  std::string GetSalt();

  // Returns a serial number for the user.
  std::string GetSerialNumber();

  // Bind-mounts the user's /data and /cache to the shared mount point to pass
  // them into the container.
  void MountSharedAndroidDirectories();

  // Unmounts the user's /data and /cache bind-mounted to the shared mount
  // point.
  void UnmountSharedAndroidDirectories();

  // Asks the (partially booted) container to turn itself into a fully
  // functional one.
  void ContinueContainerBoot(ArcBootType boot_type);

  // Ensures that directories that are necessary for starting a container
  // exist.
  void EnsureContainerDirectories();

  // Mounts image files that are necessary for container startup.
  void MountOnOnetimeSetup();

  // Unmounts image files that have been mounted in MountOnOnetimeSetup.
  void UnmountOnOnetimeStop();

  // Called when arc-setup is called with --setup or --setup-for-login-screen.
  void OnSetup(bool for_login_screen);

  // Called when arc-setup is called with ---boot-continue.
  void OnBootContinue();

  // Called when arc-setup is called with --stop.
  void OnStop();

  // Does a setup that is only needed once per Chrome OS boot.
  void OnOnetimeSetup();

  // Does a shutdown that is only needed once per Chrome OS boot.
  void OnOnetimeStop();

  // Called when arc-setup is called with --pre-chroot.
  void OnPreChroot();

  // Called when arc-setup is called with --read-ahead.
  void OnReadAhead();

  std::unique_ptr<ArcMounter> arc_mounter_;
  std::unique_ptr<ArcPaths> arc_paths_;
  std::unique_ptr<ArcSetupMetrics> arc_setup_metrics_;

  DISALLOW_COPY_AND_ASSIGN(ArcSetup);
};

}  // namespace arc

#endif  // ARC_SETUP_ARC_SETUP_H_
