// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_SETUP_ARC_SETUP_H_
#define ARC_SETUP_ARC_SETUP_H_

#include <memory>
#include <string>
#include <utility>

#include <base/macros.h>

#include "arc/setup/android_sdk_version.h"
#include "arc/setup/arc_setup_util.h"

namespace base {

class FilePath;

}  // namespace base

namespace brillo {

class CrosConfigInterface;

}  // namespace brillo

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

  AndroidSdkVersion sdk_version() const { return sdk_version_; }

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

  // Clears unused cache directory that was used in ARC-N.
  void DeleteUnusedCacheDirectory();

  // Waits for the rt-limits job to start.
  void WaitForRtLimitsJob();

  // Waits for directories in /run the container depends on to be created.
  void WaitForArcDirectories();

  // Remounts /vendor.
  void RemountVendorDirectory();

  // Identifies type of binary translation.
  ArcBinaryTranslationType IdentifyBinaryTranslationType();

  // Sets up binfmt_misc handler to run arm binaries on x86.
  void SetUpBinFmtMisc(ArcBinaryTranslationType bin_type);

  // Sets up a dummy android-data directory on tmpfs.
  void SetUpDummyAndroidDataOnTmpfs();

  // Sets up android-data directory.
  void SetUpAndroidData();

  // Sets up packages cache. Returns true if cache was set and false otherwise.
  bool SetUpPackagesCache();

  // Sets up GMS Core cache. Requires packages cache be set.
  void SetUpGmsCoreCache();
  // Sets GServices Core cache. Requires packages cache be set.
  void SetUpGservicesCache();

  // Sets up shared APK cache directory.
  void SetUpSharedApkDirectory();

  // Sets up android-data/data/dalvik-cache/<isa> directory.
  void SetUpDalvikCacheInternal(const base::FilePath& dalvik_cache_directory,
                                const base::FilePath& isa_relative);

  // Sets up android-data/data/dalvik-cache directory.
  void SetUpDalvikCache();

  // Creates files and directories needed by the container.
  void CreateContainerFilesAndDirectories();

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
  void CreateAndroidCmdlineFile(bool is_dev_mode,
                                bool is_inside_vm,
                                bool is_debuggable);

  // Create fake procfs entries expected by android.
  void CreateFakeProcfsFiles();

  // Bind-mounts /sys/kernel/debug/tracing to
  // |arc_paths.debugfs_directory|/tracing.
  void SetUpMountPointForDebugFilesystem(bool is_dev_mode);

  // Sets up a mount point for arc-removable-media.
  void SetUpMountPointForRemovableMedia();

  // Sets up a mount point for arc-adbd.
  void SetUpMountPointForAdbd();

  // Cleans up mount points in |android_mutable_source|/data/dalvik-cache/.
  void CleanUpDalvikCache();

  // Cleans up mount points for arc-removable-media if any.
  void CleanUpStaleMountPoints();

  // Sets up a mount point for passing the user's /data and /cache to the
  // container.
  void SetUpSharedMountPoints();

  // Restores the labels of files and directories. This has to be called before
  // MakeMountpointsReadOnly() makes the directories read-only.
  void RestoreContext();

  // Initializes SELinux contexts of SysFS entries necessary to enumerate
  // graphics hardware. Cannot be done using RestoreContext(), because
  // symbolic links are involved.
  void SetUpGraphicsSysfsContext();

  // Initializes SELinux contexts of SysFS entries necessary to enumerate
  // power supplies (batteries, line power, etc.) As with graphics symbolic
  // links are involved.
  // TODO(ejcaruso, b/78300746): remove this when we can use genfs_contexts
  void SetUpPowerSysfsContext();

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

  // Removes the FIFO file for emulating /dev/kmsg.
  void RemoveAndroidKmsgFifo();

  // Gets a fingerprint in the build.prop file. Since the file is in our system
  // image, the operation should never fail.
  std::string GetSystemImageFingerprint();

  // Fills |out_boot_type| with the boot type.
  // If Android's packages.xml exists, fills |out_data_sdk_version| with the SDK
  // version for the internal storage found in the XML. If packages.xml doesn't
  // exist, fills it with AndroidSdkVersion::UNKNOWN.
  void GetBootTypeAndDataSdkVersion(ArcBootType* out_boot_type,
                                    AndroidSdkVersion* out_data_sdk_version);

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

  // Mounts pre-installed demo apps to the shared mount point that will be
  // passed into the container.
  // |demo_apps_image| - path to the image containing set of demo session apps.
  // |demo_apps_mount_directory| - the path to which demo apps should be
  // mounted.
  void MountDemoApps(const base::FilePath& demo_apps_image,
                     const base::FilePath& demo_apps_mount_directory);

  // Bind-mounts the user's /data and /cache to the shared mount point to pass
  // them into the container. For demo sessions, it additionally mounts demo
  // apps that should be loaded as pre-installed by the container.
  void MountSharedAndroidDirectories();

  // Unmounts the user's /data and /cache bind-mounted to the shared mount
  // point.
  void UnmountSharedAndroidDirectories();

  // Starts the adbd proxy if the system is running in dev mode, not in a VM,
  // and there is kernel support for it.
  void MaybeStartAdbdProxy(bool is_dev_mode,
                           bool is_inside_vm,
                           const std::string& serialnumber);

  // Asks the (partially booted) container to turn itself into a fully
  // functional one.
  void ContinueContainerBoot(ArcBootType boot_type,
                             const std::string& serialnumber);

  // Ensures that directories that are necessary for starting a container
  // exist.
  void EnsureContainerDirectories();

  // Creates model-specific build properties from shared unibuild templates.
  // Called during --onetime-setup.
  void CreateBuildProperties();

  // Expands a template Android property file into /run/arc/properties.
  void ExpandPropertyFile(const base::FilePath& input,
                          const base::FilePath& output,
                          brillo::CrosConfigInterface* config);

  // Mounts image files that are necessary for container startup.
  void MountOnOnetimeSetup();

  // Unmounts image files that have been mounted in MountOnOnetimeSetup.
  void UnmountOnOnetimeStop();

  // Various bind-mounts inside the container's mount namespace on pre-chroot
  // stage.
  void BindMountInContainerNamespaceOnPreChroot(
      const base::FilePath& rootfs,
      const ArcBinaryTranslationType binary_translation_type);

  // Restores SELinux contexts of files inside the container's mount namespace.
  void RestoreContextOnPreChroot(const base::FilePath& rootfs);

  // Creates /dev/.coldboot_done file in the container's mount namespace
  void CreateDevColdbootDoneOnPreChroot(const base::FilePath& rootfs);

  // Gets the image's SDK version. This function returns UNKNOWN when the system
  // image hasn't been mounted yet.
  AndroidSdkVersion GetSdkVersion();

  // Called when arc-setup is called with --setup
  void OnSetup();

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
  const AndroidSdkVersion sdk_version_;
  // Used to prevent multiple recalculation of system fingerprint. Once
  // system fingerprint is calculated it is cached in |system_fingerprint_|.
  std::string system_fingerprint_;

  DISALLOW_COPY_AND_ASSIGN(ArcSetup);
};

}  // namespace arc

#endif  // ARC_SETUP_ARC_SETUP_H_
