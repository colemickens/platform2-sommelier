// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_SETUP_ARC_SETUP_H_
#define ARC_SETUP_ARC_SETUP_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include <base/macros.h>

#include "arc/setup/android_sdk_version.h"
#include "arc/setup/arc_setup_metrics.h"
#include "arc/setup/arc_setup_util.h"
#include "arc/setup/config.h"

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

// Enum that describes what action ArcSetup will perform.
enum class Mode {
  SETUP = 0,
  BOOT_CONTINUE,
  STOP,
  ONETIME_SETUP,

  // None of our scripts for production uses ONETIME_STOP. Only our development
  // script does to unmount system/vendor images before pushing new ones.
  ONETIME_STOP,

  PRE_CHROOT,
  READ_AHEAD,
  REMOVE_DATA,
  MOUNT_SDCARD,
  UNMOUNT_SDCARD,
  UPDATE_RESTORECON_LAST,
  UNKNOWN,
};

// A class that does the actual setup (and stop) operations.
class ArcSetup {
 public:
  ArcSetup(Mode mode, const base::FilePath& config_json);
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
  enum class PlayStoreAutoUpdate { kDefault, kOn, kOff };

  // Converts |PlayStoreAutoUpdate| to Android string param.
  static std::string GetPlayStoreAutoUpdateParam(
      PlayStoreAutoUpdate play_store_auto_update);

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

  // Sets up shared APK cache directory.
  void SetUpSharedApkDirectory();

  // Sets up sdcard mount, if required.
  void SetUpSdcard();

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
      const base::FilePath& host_dalvik_cache_directory,
      ArcCodeRelocationResult* result);

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
                                bool is_debuggable,
                                PlayStoreAutoUpdate play_store_auto_update,
                                bool disable_system_default_app);

  // Create fake procfs entries expected by android.
  void CreateFakeProcfsFiles();

  // Bind-mounts /sys/kernel/debug/tracing to
  // |arc_paths.debugfs_directory|/tracing.
  void SetUpMountPointForDebugFilesystem(bool is_dev_mode);

  // Sets up media mount points such as arc-removable-media and arc-myfiles.
  void SetUpMountPointsForMedia();

  // Sets up a mount point for arc-adbd.
  void SetUpMountPointForAdbd();

  // Sets up a mount point for arc-adbd UNIX domain sockets.
  void SetUpMountPointForAdbdUnixSocket();

  // Cleans up mount points for arc-removable-media if any.
  void CleanUpStaleMountPoints();

  // Sets up a mount point for passing the user's /data and /cache to the
  // container.
  void SetUpSharedMountPoints();

  // Sets up the correct ownership for <configfs>/sdcardfs, if the directory
  // exists.
  void SetUpOwnershipForSdcardConfigfs();

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

  // Initialize the testharness directory and key file inside for testharness.
  void SetUpTestharness(bool is_dev_mode);

  // Notifies networking service that the container is starting.
  void StartNetworking();

  // Notifies networking service that the container is stopping.
  void StopNetworking();

  // Makes some mount points read-only.
  void MakeMountPointsReadOnly();

  // Sets up a subset property file for camera.
  void SetUpCameraProperty();

  // Sets up a default apps.
  void SetUpDefaultApps();

  // Cleans up binfmt_misc handlers that run arm binaries on x86.
  void CleanUpBinFmtMiscSetUp();

  // Cleanup sdcard mount.
  void UnmountSdcard();

  // Unmounts files and directories that are not necessary after shutting down
  // the container.
  void UnmountOnStop();

  // Removes the pipe for 'bugreport'.
  void RemoveBugreportPipe();

  // Removes the FIFO file for emulating /dev/kmsg.
  void RemoveAndroidKmsgFifo();

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
  // Called during --mode=onetime-setup.
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

  // Converts |version_str| to the enum.
  AndroidSdkVersion SdkVersionFromString(const std::string& version_str);

  // Gets the image's SDK version. This function returns UNKNOWN when the system
  // image hasn't been mounted yet.
  AndroidSdkVersion GetSdkVersion();

  // Called when arc-setup is called with --mode=setup
  void OnSetup();

  // Called when arc-setup is called with --mode=boot-continue.
  void OnBootContinue();

  // Called when arc-setup is called with --mode=stop.
  void OnStop();

  // Does a setup that is only needed once per Chrome OS boot (i.e.
  // --mode=onetime-setup).
  void OnOnetimeSetup();

  // Called when arc-setup is called with --mode=onetime-stop.
  void OnOnetimeStop();

  // Called when arc-setup is called with --mode=pre-chroot.
  void OnPreChroot();

  // Called when arc-setup is called with --mode=read-ahead.
  void OnReadAhead();

  // Called when arc-setup is called with --mode=remove-data.
  void OnRemoveData();

  // Called when arc-setup is called with --mode=mount-sdcard.
  void OnMountSdcard();

  // Called when arc-setup is called with --mode=unmount-sdcard.
  void OnUnmountSdcard();

  // Called when arc-setup is called with --mode=update-restorecon-last.
  void OnUpdateRestoreconLast();

  // Returns system build property.
  std::string GetSystemBuildPropertyOrDie(const std::string& name);

  const Mode mode_;
  const Config config_;
  std::unique_ptr<ArcMounter> arc_mounter_;
  std::unique_ptr<ArcPaths> arc_paths_;
  std::unique_ptr<ArcSetupMetrics> arc_setup_metrics_;

  // Used to prevent multiple reading system properties file. Once this file is
  // read, it's content is stored here. If map is empty this indicates that
  // properties file was never read.
  std::map<std::string, std::string> system_properties_;

  DISALLOW_COPY_AND_ASSIGN(ArcSetup);
};

}  // namespace arc

#endif  // ARC_SETUP_ARC_SETUP_H_
