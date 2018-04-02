// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/setup/arc_setup.h"

#include <fcntl.h>
#include <linux/magic.h>
#include <sched.h>
#include <selinux/selinux.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/environment.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <base/sys_info.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <base/timer/elapsed_timer.h>
#include <chromeos-config/libcros_config/cros_config.h>
#include <metrics/metrics_library.h>

#include "arc/setup/arc_read_ahead.h"
#include "arc/setup/arc_setup_metrics.h"
#include "arc/setup/art_container.h"

#define EXIT_IF(f)                            \
  do {                                        \
    LOG(INFO) << "Running " << (#f) << "..."; \
    CHECK(!(f));                              \
  } while (false)

#define IGNORE_ERRORS(f)                                 \
  do {                                                   \
    LOG(INFO) << "Running " << (#f) << "...";            \
    LOG_IF(INFO, !(f)) << "Ignoring failures: " << (#f); \
  } while (false)

// TODO(yusukes): use android_filesystem_config.h.
#define AID_ROOT 0        /* traditional unix root user */
#define AID_SYSTEM 1000   /* system server */
#define AID_LOG 1007      /* log devices */
#define AID_MEDIA_RW 1023 /* internal media storage write access */
#define AID_SHELL 2000    /* adb and debug shell user */
#define AID_CACHE 2001    /* cache access */

namespace arc {

namespace {

// Lexicographically sorted. Usually you don't have to use these constants
// directly. Prefer base::FilePath variables in ArcPaths instead.
constexpr char kAdbdMountDirectory[] = "/run/arc/adbd";
constexpr char kAndroidCmdline[] = "/run/arc/cmdline.android";
constexpr char kAndroidGeneratedPropertiesDirectory[] = "/run/arc/properties";
constexpr char kAndroidKmsgFifo[] = "/run/arc/android.kmsg.fifo";
constexpr char kAndroidMutableSource[] =
    "/opt/google/containers/android/rootfs/android-data";
constexpr char kAndroidRootfsDirectory[] =
    "/opt/google/containers/android/rootfs/root";
constexpr char kOldApkCacheDir[] =
    "/mnt/stateful_partition/unencrypted/cache/apk";
constexpr char kApkCacheDir[] = "/mnt/stateful_partition/unencrypted/apkcache";
constexpr char kArcBridgeSocketContext[] = "u:object_r:arc_bridge_socket:s0";
constexpr char kArcBridgeSocketPath[] = "/run/chrome/arc_bridge.sock";
constexpr char kBinFmtMiscDirectory[] = "/proc/sys/fs/binfmt_misc";
constexpr char kCameraProfileDir[] =
    "/mnt/stateful_partition/encrypted/var/cache/camera";
constexpr char kCrasSocketDirectory[] = "/run/cras";
constexpr char kDebugfsDirectory[] = "/run/arc/debugfs";
constexpr char kFakeKptrRestrict[] = "/run/arc/fake_kptr_restrict";
constexpr char kFakeMmapRndBits[] = "/run/arc/fake_mmap_rnd_bits";
constexpr char kFakeMmapRndCompatBits[] = "/run/arc/fake_mmap_rnd_compat_bits";
constexpr char kHostSideDalvikCacheDirectoryInContainer[] =
    "/var/run/arc/dalvik-cache";
constexpr char kMediaDestDirectory[] = "/run/arc/media/removable";
constexpr char kMediaMountDirectory[] = "/run/arc/media";
constexpr char kMediaProfileFile[] = "media_profiles.xml";
constexpr char kMediaRootfsDirectory[] =
    "/opt/google/containers/arc-removable-media/mountpoints/container-root";
constexpr char kObbMountDirectory[] = "/run/arc/obb";
constexpr char kObbRootfsDirectory[] =
    "/opt/google/containers/arc-obb-mounter/mountpoints/container-root";
constexpr char kObbRootfsImage[] =
    "/opt/google/containers/arc-obb-mounter/rootfs.squashfs";
constexpr char kOemMountDirectory[] = "/run/arc/oem";
constexpr char kPassthroughImage[] =
    "/usr/share/mount-passthrough/rootfs.squashfs";
constexpr char kPlatformXmlFileRelative[] = "etc/permissions/platform.xml";
constexpr char kRestoreconWhitelistSync[] = "/sys/kernel/debug/sync";
constexpr char kSdcardMountDirectory[] = "/run/arc/sdcard";
constexpr char kSdcardRootfsDirectory[] =
    "/opt/google/containers/arc-sdcard/mountpoints/container-root";
constexpr char kSdcardRootfsImage[] =
    "/opt/google/containers/arc-sdcard/rootfs.squashfs";
constexpr char kSharedMountDirectory[] = "/run/arc/shared_mounts";
constexpr char kSysfsCpu[] = "/sys/devices/system/cpu";
constexpr char kSysfsTracing[] = "/sys/kernel/debug/tracing";
constexpr char kSystemLibArmDirectoryRelative[] = "system/lib/arm";
constexpr char kSystemImage[] = "/opt/google/containers/android/system.raw.img";
constexpr char kUsbDevicesDirectory[] = "/dev/bus/usb";

// Names for possible binfmt_misc entries.
constexpr const char* kBinFmtMiscEntryNames[] = {"arm_dyn", "arm_exe",
                                                 "arm64_dyn", "arm64_exe"};

constexpr uid_t kHostRootUid = 0;
constexpr gid_t kHostRootGid = 0;

constexpr uid_t kShiftUid = 655360;
constexpr gid_t kShiftGid = 655360;
constexpr uid_t kRootUid = AID_ROOT + kShiftUid;
constexpr gid_t kRootGid = AID_ROOT + kShiftGid;
constexpr uid_t kSystemUid = AID_SYSTEM + kShiftUid;
constexpr gid_t kSystemGid = AID_SYSTEM + kShiftGid;
constexpr uid_t kMediaUid = AID_MEDIA_RW + kShiftUid;
constexpr gid_t kMediaGid = AID_MEDIA_RW + kShiftGid;
constexpr uid_t kShellUid = AID_SHELL + kShiftUid;
constexpr uid_t kShellGid = AID_SHELL + kShiftGid;
constexpr gid_t kCacheGid = AID_CACHE + kShiftGid;
constexpr gid_t kLogGid = AID_LOG + kShiftGid;

constexpr char kSwitchSetup[] = "setup";
constexpr char kSwitchBootContinue[] = "boot-continue";
constexpr char kSwitchStop[] = "stop";
constexpr char kSwitchOnetimeSetup[] = "onetime-setup";
constexpr char kSwitchOnetimeStop[] = "onetime-stop";
constexpr char kSwitchPreChroot[] = "pre-chroot";
constexpr char kSwitchReadAhead[] = "read-ahead";

// The maximum time arc::EmulateArcUreadahead() can spend.
constexpr base::TimeDelta kReadAheadTimeout = base::TimeDelta::FromSeconds(7);

enum class Mode {
  SETUP = 0,
  BOOT_CONTINUE,
  STOP,
  ONETIME_SETUP,
  ONETIME_STOP,
  PRE_CHROOT,
  READ_AHEAD,
  UNKNOWN,
};

Mode GetMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kSwitchSetup))
    return Mode::SETUP;
  if (command_line->HasSwitch(kSwitchBootContinue))
    return Mode::BOOT_CONTINUE;
  if (command_line->HasSwitch(kSwitchStop))
    return Mode::STOP;
  if (command_line->HasSwitch(kSwitchOnetimeSetup))
    return Mode::ONETIME_SETUP;
  if (command_line->HasSwitch(kSwitchOnetimeStop))
    return Mode::ONETIME_STOP;
  if (command_line->HasSwitch(kSwitchPreChroot))
    return Mode::PRE_CHROOT;
  if (command_line->HasSwitch(kSwitchReadAhead))
    return Mode::READ_AHEAD;
  CHECK(false) << "Missing mode";
  return Mode::UNKNOWN;
}

bool RegisterAllBinFmtMiscEntries(ArcMounter* mounter,
                                  const base::FilePath& entry_directory,
                                  const base::FilePath& binfmt_misc_directory) {
  std::unique_ptr<ScopedMount> binfmt_misc_mount =
      ScopedMount::CreateScopedMount(mounter, "binfmt_misc",
                                     binfmt_misc_directory, "binfmt_misc",
                                     MS_NOSUID | MS_NODEV | MS_NOEXEC, nullptr);
  if (!binfmt_misc_mount)
    return false;

  const base::FilePath binfmt_misc_register_path =
      binfmt_misc_directory.Append("register");
  for (auto entry_name : kBinFmtMiscEntryNames) {
    const base::FilePath entry_path = entry_directory.Append(entry_name);
    // arm64_{dyn,exe} are only available on some boards/configurations. Only
    // install them if they are present.
    if (!base::PathExists(entry_path))
      continue;
    if (!base::CopyFile(entry_path, binfmt_misc_register_path)) {
      PLOG(ERROR) << "Failed to register " << entry_path.value();
      return false;
    }
  }

  return true;
}

void UnregisterBinFmtMiscEntry(const base::FilePath& entry_path) {
  // This function is for Mode::STOP. Ignore errors to make sure to run all
  // clean up code.
  base::File entry(entry_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  if (!entry.IsValid()) {
    PLOG(INFO) << "Ignoring failure: Failed to open " << entry_path.value();
    return;
  }
  constexpr char kBinfmtMiscUnregister[] = "-1";
  IGNORE_ERRORS(
      entry.Write(0, kBinfmtMiscUnregister, sizeof(kBinfmtMiscUnregister) - 1));
}

// Prepends |path_to_prepend| to each element in [first, last), and returns the
// result as a vector.
template <typename It>
std::vector<base::FilePath> PrependPath(It first,
                                        It last,
                                        const base::FilePath& path_to_prepend) {
  std::vector<base::FilePath> result;
  std::transform(first, last, std::back_inserter(result),
                 [&path_to_prepend](const char* path) {
                   return path_to_prepend.Append(path);
                 });
  return result;
}

}  // namespace

struct ArcPaths {
  std::unique_ptr<base::Environment> env{base::Environment::Create()};
  const Mode mode{GetMode()};

  // Lexicographically sorted.
  const base::FilePath adbd_mount_directory{kAdbdMountDirectory};
  const base::FilePath android_cmdline{kAndroidCmdline};
  const base::FilePath android_generated_properties_directory{
      kAndroidGeneratedPropertiesDirectory};
  const base::FilePath android_kmsg_fifo{kAndroidKmsgFifo};
  const base::FilePath android_mutable_source{kAndroidMutableSource};
  const base::FilePath android_rootfs_directory{kAndroidRootfsDirectory};
  const base::FilePath arc_bridge_socket_path{kArcBridgeSocketPath};
  const base::FilePath old_apk_cache_dir{kOldApkCacheDir};
  const base::FilePath apk_cache_dir{kApkCacheDir};
  const base::FilePath art_dalvik_cache_directory{kArtDalvikCacheDirectory};
  const base::FilePath binfmt_misc_directory{kBinFmtMiscDirectory};
  const base::FilePath camera_profile_dir{kCameraProfileDir};
  const base::FilePath cras_socket_directory{kCrasSocketDirectory};
  const base::FilePath debugfs_directory{kDebugfsDirectory};
  const base::FilePath fake_kptr_restrict{kFakeKptrRestrict};
  const base::FilePath fake_mmap_rnd_bits{kFakeMmapRndBits};
  const base::FilePath fake_mmap_rnd_compat_bits{kFakeMmapRndCompatBits};
  const base::FilePath host_side_dalvik_cache_directory_in_container{
      kHostSideDalvikCacheDirectoryInContainer};
  const base::FilePath media_dest_directory{kMediaDestDirectory};
  const base::FilePath media_mount_directory{kMediaMountDirectory};
  const base::FilePath media_profile_file{kMediaProfileFile};
  const base::FilePath media_rootfs_directory{kMediaRootfsDirectory};
  const base::FilePath obb_mount_directory{kObbMountDirectory};
  const base::FilePath obb_rootfs_directory{kObbRootfsDirectory};
  const base::FilePath oem_mount_directory{kOemMountDirectory};
  const base::FilePath platform_xml_file_relative{kPlatformXmlFileRelative};
  const base::FilePath sdcard_mount_directory{kSdcardMountDirectory};
  const base::FilePath sdcard_rootfs_directory{kSdcardRootfsDirectory};
  const base::FilePath shared_mount_directory{kSharedMountDirectory};
  const base::FilePath sysfs_cpu{kSysfsCpu};
  const base::FilePath sysfs_tracing{kSysfsTracing};
  const base::FilePath system_lib_arm_directory_relative{
      kSystemLibArmDirectoryRelative};
  const base::FilePath usb_devices_directory{kUsbDevicesDirectory};

  const base::FilePath restorecon_whitelist_sync{kRestoreconWhitelistSync};
  // session_manager must start arc-setup job with ANDROID_DATA_DIR parameter
  // containing the path of the real android-data directory. They are passed
  // only when the mode is either --setup or --boot-continue.
  const bool kHasAndroidDataDir = mode == Mode::BOOT_CONTINUE;
  const base::FilePath android_data_directory =
      kHasAndroidDataDir ? GetFilePathOrDie(env.get(), "ANDROID_DATA_DIR")
                         : base::FilePath();
  const base::FilePath android_data_old_directory =
      kHasAndroidDataDir ? GetFilePathOrDie(env.get(), "ANDROID_DATA_OLD_DIR")
                         : base::FilePath();
};

ArcSetup::ArcSetup()
    : arc_mounter_(GetDefaultMounter()),
      arc_paths_(std::make_unique<ArcPaths>()),
      arc_setup_metrics_(std::make_unique<ArcSetupMetrics>()),
      sdk_version_(GetSdkVersion()) {}

ArcSetup::~ArcSetup() = default;

void ArcSetup::DeleteExecutableFilesInData(
    bool should_delete_data_dalvik_cache_directory,
    bool should_delete_data_app_executables) {
  if (!should_delete_data_dalvik_cache_directory &&
      !should_delete_data_app_executables) {
    return;
  }

  if (!base::PathExists(arc_paths_->android_data_old_directory)) {
    EXIT_IF(!InstallDirectory(0700, kHostRootUid, kHostRootGid,
                              arc_paths_->android_data_old_directory));
  }

  base::FilePath old_executables_directory;
  EXIT_IF(!base::CreateTemporaryDirInDir(arc_paths_->android_data_old_directory,
                                         "old_executables_",
                                         &old_executables_directory));

  // Move data/dalvik-cache to old_executables_directory.
  const base::FilePath dalvik_cache_directory =
      arc_paths_->android_data_directory.Append(
          base::FilePath("data/dalvik-cache"));
  if (should_delete_data_dalvik_cache_directory &&
      base::PathExists(dalvik_cache_directory)) {
    LOG(INFO) << "Moving " << dalvik_cache_directory.value() << " to "
              << old_executables_directory.value();
    if (!base::Move(dalvik_cache_directory, old_executables_directory))
      PLOG(ERROR) << "Failed to move dalvik-cache";
  }

  // Move data/app/oat cache.
  const base::FilePath app_directory =
      arc_paths_->android_data_directory.Append(base::FilePath("data/app"));
  if (should_delete_data_app_executables && base::PathExists(app_directory)) {
    base::ElapsedTimer timer;
    MoveDataAppOatDirectory(app_directory, old_executables_directory);
    LOG(INFO) << "Moving data/app/<package_name>/oat took "
              << timer.Elapsed().InMillisecondsRoundedUp() << "ms";
  }
}

void ArcSetup::WaitForRtLimitsJob() {
  constexpr base::TimeDelta kWaitForRtLimitsJobTimeOut =
      base::TimeDelta::FromSeconds(10);
  constexpr base::TimeDelta kSleepInterval =
      base::TimeDelta::FromMilliseconds(100);
  constexpr char kCgroupFilePath[] =
      "/sys/fs/cgroup/cpu/session_manager_containers/cpu.rt_runtime_us";

  base::ElapsedTimer timer;
  const base::FilePath cgroup_file(kCgroupFilePath);
  while (true) {
    if (base::PathExists(cgroup_file)) {
      std::string rt_runtime_us_str;
      EXIT_IF(!base::ReadFileToString(cgroup_file, &rt_runtime_us_str));
      int rt_runtime_us = 0;
      base::StringToInt(rt_runtime_us_str, &rt_runtime_us);
      if (rt_runtime_us > 0) {
        LOG(INFO) << cgroup_file.value() << " is set to " << rt_runtime_us;
        break;
      }
    }
    base::PlatformThread::Sleep(kSleepInterval);
    CHECK_GE(kWaitForRtLimitsJobTimeOut, timer.Elapsed())
        << ": rt-limits job didn't start in " << kWaitForRtLimitsJobTimeOut;
  }

  LOG(INFO) << "rt-limits job is ready in "
            << timer.Elapsed().InMillisecondsRoundedUp() << " ms";
}

ArcBinaryTranslationType ArcSetup::IdentifyBinaryTranslationType() {
  bool is_houdini_available = kUseHoudini;
  bool is_ndk_translation_available = kUseNdkTranslation;

  // TODO(yusukes): Remove this hack once b/70732332 is fixed.
  if (is_houdini_available && sdk_version() == AndroidSdkVersion::ANDROID_P) {
    // arc-setup is built with USE=houdini, but the image is P's. Disable
    // Houdini since b/70732332 hasn't been fixed yet.
    LOG(WARNING) << "Force disabling Houdini on P";
    is_houdini_available = false;
  }

  if (!base::PathExists(arc_paths_->android_rootfs_directory.Append(
          "system/lib/libndk_translation.so"))) {
    // Allow developers to use custom android build
    // without ndk-translation in it.
    is_ndk_translation_available = false;
  }

  if (!is_houdini_available && !is_ndk_translation_available)
    return ArcBinaryTranslationType::NONE;

  const bool prefer_ndk_translation =
      (!is_houdini_available ||
       GetBooleanEnvOrDie(arc_paths_->env.get(), "NATIVE_BRIDGE_EXPERIMENT"));

  if (is_ndk_translation_available && prefer_ndk_translation)
    return ArcBinaryTranslationType::NDK_TRANSLATION;

  return ArcBinaryTranslationType::HOUDINI;
}

void ArcSetup::SetUpBinFmtMisc(ArcBinaryTranslationType bin_type) {
  const std::string system_arch = base::SysInfo::OperatingSystemArchitecture();
  if (system_arch != "x86_64")
    return;

  base::FilePath root_directory;

  switch (bin_type) {
    case ArcBinaryTranslationType::NONE: {
      // No binary translation at all, neither Houdini nor NDK translation.
      return;
    }
    case ArcBinaryTranslationType::HOUDINI: {
      root_directory = arc_paths_->android_rootfs_directory.Append("vendor");
      break;
    }
    case ArcBinaryTranslationType::NDK_TRANSLATION: {
      root_directory = arc_paths_->android_rootfs_directory.Append("system");
      break;
    }
  }

  EXIT_IF(!RegisterAllBinFmtMiscEntries(
      arc_mounter_.get(), root_directory.Append("etc/binfmt_misc"),
      arc_paths_->binfmt_misc_directory));
}

void ArcSetup::SetUpAndroidData() {
  EXIT_IF(!InstallDirectory(0700, kHostRootUid, kHostRootGid,
                            arc_paths_->android_data_directory));
  // To make our bind-mount business easier, we first bind-mount the real
  // android-data directory ($ANDROID_DATA_DIR) to a fixed path
  // ($ANDROID_MUTABLE_SOURCE).
  // Then we do not need to pass around $ANDROID_DATA_DIR in every other places.
  EXIT_IF(!arc_mounter_->BindMount(arc_paths_->android_data_directory,
                                   arc_paths_->android_mutable_source));

  // match android/system/core/rootdir/init.rc
  EXIT_IF(!InstallDirectory(0771, kSystemUid, kSystemGid,
                            arc_paths_->android_mutable_source.Append("data")));
  EXIT_IF(
      !InstallDirectory(0770, kSystemUid, kCacheGid,
                        arc_paths_->android_mutable_source.Append("cache")));

  if (SetUpPackagesCache()) {
    // Note, GMS and GServices caches are valid only in case packages cache is
    // set which contains predefined value for shared Google user uid. That let
    // to set valid resources owner.
    SetUpGmsCoreCache();
    SetUpGservicesCache();
  }

  if (sdk_version() == AndroidSdkVersion::ANDROID_P)
    SetUpNetwork();
}

bool ArcSetup::SetUpPackagesCache() {
  base::ElapsedTimer timer;

  if (GetBooleanEnvOrDie(arc_paths_->env.get(), "SKIP_PACKAGES_CACHE_SETUP")) {
    LOG(INFO) << "Packages cache setup is disabled.";
    return false;
  }

  // When /data/system/packages.xml does not exist, copy pre-generated
  // /system/etc/packages_cache.xml to /data/system/packages.xml
  const base::FilePath packages_cache =
      arc_paths_->android_mutable_source.Append("data/system/packages.xml");
  if (base::PathExists(packages_cache))
    return false;

  const base::FilePath source_cache =
      arc_paths_->android_rootfs_directory.Append(
          "system/etc/packages_cache.xml");
  // Test if packages cache exists. Manually pushed images may not contain it.
  if (!base::PathExists(source_cache)) {
    LOG(INFO) << "Packages cache was not found "
              << "(this expected for manually-pushed images).";
    return false;
  }

  LOG(INFO) << "Installing packages cache to " << packages_cache.value() << ".";

  EXIT_IF(!InstallDirectory(0775, kSystemUid, kSystemGid,
                            packages_cache.DirName()));
  EXIT_IF(!base::CopyFile(source_cache, packages_cache));
  EXIT_IF(!Chown(kSystemUid, kSystemGid, packages_cache));
  EXIT_IF(!base::SetPosixFilePermissions(packages_cache, 0660));

  LOG(INFO) << "Packages cache setup completed in "
            << timer.Elapsed().InMillisecondsRoundedUp() << " ms";
  return true;
}

void ArcSetup::SetUpGmsCoreCache() {
  base::ElapsedTimer timer;

  const base::FilePath user_de =
      arc_paths_->android_mutable_source.Append("data/user_de");
  const base::FilePath user_de_0 = user_de.Append("0");
  const base::FilePath user_de_0_gms =
      user_de_0.Append("com.google.android.gms");

  // When /data/user_de/0/com.google.android.gms does not exist, this indicates
  // first run for GMS Core. Install set of pre-computed cache files if they
  // exist.
  if (base::PathExists(user_de_0_gms))
    return;

  const base::FilePath source_cache_dir =
      arc_paths_->android_rootfs_directory.Append("system/etc/gms_core_cache");
  if (!base::PathExists(source_cache_dir)) {
    LOG(INFO) << "GMS Core cache was not found "
              << "(this expected for manually-pushed images).";
    return;
  }

  LOG(INFO) << "Installing GMS Core cache to " << user_de_0_gms.value() << ".";

  EXIT_IF(!InstallDirectory(0711, kSystemUid, kSystemGid, user_de));
  EXIT_IF(!InstallDirectory(0771, kSystemUid, kSystemGid, user_de_0));
  EXIT_IF(!CopyWithAttributes(source_cache_dir, user_de_0_gms));

  LOG(INFO) << "GMS Core cache setup competed in "
            << timer.Elapsed().InMillisecondsRoundedUp() << " ms";
}

void ArcSetup::SetUpGservicesCache() {
  base::ElapsedTimer timer;

  // When /data/data/com.google.android.gsf does not exist, that indicates first
  // run for GServices. In this copy prepared directory with cache files.
  const base::FilePath data =
      arc_paths_->android_mutable_source.Append("data/data");
  const base::FilePath gsf_dir = data.Append("com.google.android.gsf");

  if (base::PathExists(gsf_dir))
    return;

  const base::FilePath source_cache_dir =
      arc_paths_->android_rootfs_directory.Append("system/etc/gservices_cache");
  if (!base::PathExists(source_cache_dir)) {
    LOG(INFO) << "GServices cache was not found "
              << "(this expected for manually-pushed images).";
    return;
  }

  LOG(INFO) << "Installing GServices cache to " << gsf_dir.value() << ".";

  EXIT_IF(!InstallDirectory(0771, kSystemUid, kSystemGid, data));
  EXIT_IF(!CopyWithAttributes(source_cache_dir, gsf_dir));

  LOG(INFO) << "GServices cache setup competed in "
            << timer.Elapsed().InMillisecondsRoundedUp() << " ms";
}

void ArcSetup::SetUpDalvikCacheInternal(
    const base::FilePath& dalvik_cache_directory,
    const base::FilePath& isa_relative) {
  const base::FilePath dest_directory =
      arc_paths_->android_mutable_source.Append("data/dalvik-cache")
          .Append(isa_relative);
  LOG(INFO) << "Setting up " << dest_directory.value();
  EXIT_IF(!base::PathExists(dest_directory));

  base::FilePath src_directory = dalvik_cache_directory.Append(isa_relative);
  EXIT_IF(!arc_mounter_->BindMount(src_directory, dest_directory));
}

void ArcSetup::SetUpDalvikCache() {
  const base::FilePath& dalvik_cache_directory =
      arc_paths_->art_dalvik_cache_directory;
  if (!base::PathExists(dalvik_cache_directory)) {
    LOG(INFO) << dalvik_cache_directory.value() << " does not exist.";
    return;
  }

  base::FileEnumerator directories(dalvik_cache_directory,
                                   false /* recursive */,
                                   base::FileEnumerator::FileType::DIRECTORIES);
  for (base::FilePath name = directories.Next(); !name.empty();
       name = directories.Next()) {
    SetUpDalvikCacheInternal(dalvik_cache_directory, name.BaseName());
  }
}

void ArcSetup::CleanUpDalvikCache() {
  const base::FilePath dalvik_cache_directory =
      arc_paths_->android_mutable_source.Append("data/dalvik-cache");
  IGNORE_ERRORS(
      arc_mounter_->UmountLazily(dalvik_cache_directory.Append("arm")));
  IGNORE_ERRORS(
      arc_mounter_->UmountLazily(dalvik_cache_directory.Append("x86")));
  IGNORE_ERRORS(
      arc_mounter_->UmountLazily(dalvik_cache_directory.Append("x86_64")));
}

void ArcSetup::CreateContainerFilesAndDirectories() {
  EXIT_IF(!InstallDirectory(0755, kHostRootUid, kHostRootGid,
                            base::FilePath("/run/arc")));
  EXIT_IF(!InstallDirectory(0755, kShellUid, kLogGid,
                            base::FilePath("/run/arc/bugreport")));

  // If the log file exists, change the UID/GID here. We used to use
  // android-root for the file, but now we use just root. The Upstart
  // job does not (and cannot efficiently) do it.
  // TODO(yusukes): This is a temporary migration code. Remove it once
  // we hit M68.
  const base::FilePath android_kmsg("/var/log/android.kmsg");
  if (base::PathExists(android_kmsg))
    EXIT_IF(!Chown(kHostRootUid, kHostRootGid, android_kmsg));

  // Create the FIFO file and start its reader job.
  RemoveAndroidKmsgFifo();
  EXIT_IF(mkfifo(arc_paths_->android_kmsg_fifo.value().c_str(), 0644) < 0);
  {
    base::ScopedFD fd =
        OpenFifoSafely(arc_paths_->android_kmsg_fifo, O_RDONLY, 0);
    EXIT_IF(!fd.is_valid());
    EXIT_IF(fchown(fd.get(), kRootUid, kRootGid) < 0);
  }
  EXIT_IF(!LaunchAndWait(
      {"/sbin/initctl", "start", "--no-wait", "arc-kmsg-logger"}));

  // TODO(b/28988348)
  EXIT_IF(!base::SetPosixFilePermissions(base::FilePath("/run/chrome"), 0755));
}

void ArcSetup::ApplyPerBoardConfigurations() {
  EXIT_IF(!MkdirRecursively(arc_paths_->oem_mount_directory));

  const base::FilePath hardware_features_xml("/etc/hardware_features.xml");
  if (!base::PathExists(hardware_features_xml))
    return;

  EXIT_IF(!arc_mounter_->Mount("tmpfs", arc_paths_->oem_mount_directory,
                               "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                               "mode=0755"));
  EXIT_IF(!MkdirRecursively(
      arc_paths_->oem_mount_directory.Append("etc/permissions")));

  // Detect camera device and generate camera profiles.
  const base::FilePath generate_camera_profile(
      "/usr/bin/generate_camera_profile");
  if (base::PathExists(generate_camera_profile)) {
    EXIT_IF(!LaunchAndWait({generate_camera_profile.value()}));

    const base::FilePath generated_media_profile_xml =
        base::FilePath(arc_paths_->camera_profile_dir)
            .Append(arc_paths_->media_profile_file);
    const base::FilePath new_media_profile_xml =
        base::FilePath(arc_paths_->oem_mount_directory)
            .Append("etc")
            .Append(arc_paths_->media_profile_file);
    if (base::PathExists(generated_media_profile_xml)) {
      EXIT_IF(
          !base::CopyFile(generated_media_profile_xml, new_media_profile_xml));
    }
  }

  const base::FilePath platform_xml_file =
      base::FilePath(arc_paths_->oem_mount_directory)
          .Append(arc_paths_->platform_xml_file_relative);
  EXIT_IF(!base::CopyFile(hardware_features_xml, platform_xml_file));

  const base::FilePath board_hardware_features(
      "/usr/sbin/board_hardware_features");
  if (!base::PathExists(board_hardware_features))
    return;

  // The board_hardware_features is usually made by shell script and should
  // receive platform XML file argument in absolute path to avoid unexpected
  // environment issues.
  EXIT_IF(!LaunchAndWait(
      {board_hardware_features.value(), platform_xml_file.value()}));
}

void ArcSetup::CreateBuildProperties() {
  EXIT_IF(
      !MkdirRecursively(arc_paths_->android_generated_properties_directory));

  // InitModel won't succeed on non-unibuild boards, but that doesn't matter
  // because the property files won't contain any templates that need to be
  // expanded.  On unibuild boards, if it doesn't succeed then
  // ExpandPropertyFile() will later fail when it can't look up the template
  // expansions.  Either way, errors here should be ignored.
  auto config = std::make_unique<brillo::CrosConfig>();
  IGNORE_ERRORS(config->InitModel());

  constexpr const char* prop_files[] = {"default.prop", "system/build.prop"};
  for (const auto& prop_file : prop_files) {
    const base::FilePath in_prop =
        arc_paths_->android_rootfs_directory.Append(prop_file);
    const base::FilePath expanded_prop =
        arc_paths_->android_generated_properties_directory.Append(
            in_prop.BaseName());
    ExpandPropertyFile(in_prop, expanded_prop, config.get());
  }
}

void ArcSetup::ExpandPropertyFile(const base::FilePath& input,
                                  const base::FilePath& output,
                                  brillo::CrosConfigInterface* config) {
  std::string content;
  std::string expanded;
  EXIT_IF(!base::ReadFileToString(input, &content));
  EXIT_IF(!ExpandPropertyContents(content, config, &expanded));
  EXIT_IF(!WriteToFile(output, 0600, expanded));
  EXIT_IF(!Chown(kRootUid, kRootGid, output));
}

void ArcSetup::MaybeStartUreadaheadInTracingMode() {
  const base::FilePath readahead_pack_file(
      "/var/lib/ureadahead/opt.google.containers.android.rootfs.root.pack");
  if (!base::PathExists(readahead_pack_file)) {
    // We should continue to launch the container even if arc-ureadahead-trace
    // fails to start (b/31680524).
    IGNORE_ERRORS(
        LaunchAndWait({"/sbin/initctl", "start", "arc-ureadahead-trace"}));
  }
}

void ArcSetup::SetUpSharedTmpfsForExternalStorage() {
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->sdcard_mount_directory));
  EXIT_IF(!MkdirRecursively(arc_paths_->sdcard_mount_directory));
  EXIT_IF(!arc_mounter_->Mount("tmpfs", arc_paths_->sdcard_mount_directory,
                               "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                               "mode=0755"));
  EXIT_IF(!arc_mounter_->SharedMount(arc_paths_->sdcard_mount_directory));
  EXIT_IF(
      !InstallDirectory(0755, kRootUid, kRootGid,
                        arc_paths_->sdcard_mount_directory.Append("default")));
  EXIT_IF(!InstallDirectory(0755, kRootUid, kRootGid,
                            arc_paths_->sdcard_mount_directory.Append("read")));
  EXIT_IF(
      !InstallDirectory(0755, kRootUid, kRootGid,
                        arc_paths_->sdcard_mount_directory.Append("write")));

  // Create the mount directories. In original Android, these are created in
  // EmulatedVolume.cpp just before /system/bin/sdcard is fork()/exec()'ed.
  // Following code just emulates it. The directories are owned by Android's
  // root.
  // Note that, these creation should be conceptually done in arc-sdcard
  // container, but to keep it simpler, here create the directories instead.
  EXIT_IF(!InstallDirectory(
      0755, kRootUid, kRootGid,
      arc_paths_->sdcard_mount_directory.Append("default/emulated")));
  EXIT_IF(!InstallDirectory(
      0755, kRootUid, kRootGid,
      arc_paths_->sdcard_mount_directory.Append("read/emulated")));
  EXIT_IF(!InstallDirectory(
      0755, kRootUid, kRootGid,
      arc_paths_->sdcard_mount_directory.Append("write/emulated")));
}

void ArcSetup::SetUpFilesystemForObbMounter() {
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->obb_mount_directory));
  EXIT_IF(!MkdirRecursively(arc_paths_->obb_mount_directory));
  EXIT_IF(!arc_mounter_->Mount("tmpfs", arc_paths_->obb_mount_directory,
                               "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                               "mode=0755"));
  EXIT_IF(!arc_mounter_->SharedMount(arc_paths_->obb_mount_directory));
}

bool ArcSetup::GenerateHostSideCodeInternal(
    const base::FilePath& host_dalvik_cache_directory) {
  base::ElapsedTimer timer;
  std::unique_ptr<ArtContainer> art_container =
      ArtContainer::CreateContainer(arc_mounter_.get());
  if (!art_container) {
    LOG(ERROR) << "Failed to create art container";
    return false;
  }
  const std::string salt = GetSalt();
  if (salt.empty())
    return false;

  const uint64_t offset_seed =
      GetArtCompilationOffsetSeed(GetSystemImageFingerprint(), salt);
  if (!art_container->PatchImage(offset_seed)) {
    LOG(ERROR) << "Failed to relocate boot images";
    return false;
  }
  arc_setup_metrics_->SendCodeRelocationTime(timer.Elapsed());
  return true;
}

bool ArcSetup::GenerateHostSideCode(
    const base::FilePath& host_dalvik_cache_directory) {
  bool result = true;
  base::ElapsedTimer timer;
  if (!GenerateHostSideCodeInternal(host_dalvik_cache_directory)) {
    // If anything fails, delete code in cache.
    LOG(INFO) << "Failed to generate host-side code. Deleting existing code in "
              << host_dalvik_cache_directory.value();
    DeleteFilesInDir(host_dalvik_cache_directory);
    result = false;
  }
  base::TimeDelta time_delta = timer.Elapsed();
  LOG(INFO) << "GenerateHostSideCode took "
            << time_delta.InMillisecondsRoundedUp() << "ms";
  arc_setup_metrics_->SendCodeRelocationResult(
      result ? ArcCodeRelocationResult::SUCCESS
             : ArcCodeRelocationResult::ERROR_UNABLE_TO_RELOCATE);
  return result;
}

bool ArcSetup::InstallLinksToHostSideCodeInternal(
    const base::FilePath& src_isa_directory,
    const base::FilePath& dest_isa_directory,
    const std::string& isa) {
  constexpr char kDalvikCacheSELinuxContext[] =
      "u:object_r:dalvikcache_data_file:s0";
  bool src_file_exists = false;
  LOG(INFO) << "Adding symlinks to " << dest_isa_directory.value();

  // Do the same as maybeCreateDalvikCache() in
  // framework/base/cmds/app_process/app_main.cpp.
  EXIT_IF(!InstallDirectory(0711, kRootUid, kRootGid, dest_isa_directory));
  EXIT_IF(!Chcon(kDalvikCacheSELinuxContext, dest_isa_directory));

  base::FileEnumerator src_file_iter(
      src_isa_directory, false /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::SHOW_SYM_LINKS);
  for (auto src_file = src_file_iter.Next(); !src_file.empty();
       src_file = src_file_iter.Next()) {
    const base::FilePath base_name = src_file.BaseName();
    LOG(INFO) << "Processing " << base_name.value();

    base::FilePath link_target;
    if (S_ISLNK(src_file_iter.GetInfo().stat().st_mode)) {
      // *boot*.oat files in |src_isa_directory| are links to /system. Create a
      // link to /system.
      EXIT_IF(!base::ReadSymbolicLink(src_file, &link_target));
    } else {
      // Create a link to a host-side *boot*.art file.
      link_target =
          arc_paths_->host_side_dalvik_cache_directory_in_container.Append(isa)
              .Append(base_name);
    }

    const base::FilePath dest_file = dest_isa_directory.Append(base_name);
    // Remove |dest_file| first when it exists. When |dest_file| is a symlink,
    // this deletes the link itself.
    IGNORE_ERRORS(base::DeleteFile(dest_file, false /* recursive */));
    EXIT_IF(!base::CreateSymbolicLink(link_target, dest_file));
    EXIT_IF(lchown(dest_file.value().c_str(), kRootUid, kRootGid) != 0);
    EXIT_IF(!Chcon(kDalvikCacheSELinuxContext, dest_file));

    LOG(INFO) << "Created a link to " << link_target.value();
    src_file_exists = true;
  }

  return src_file_exists;
}

bool ArcSetup::InstallLinksToHostSideCode() {
  bool result = true;
  base::ElapsedTimer timer;
  const base::FilePath& src_directory = arc_paths_->art_dalvik_cache_directory;
  const base::FilePath dest_directory =
      arc_paths_->android_data_directory.Append("data/dalvik-cache");

  EXIT_IF(!InstallDirectory(0771, kRootUid, kRootGid, dest_directory));
  // Iterate through each isa sub directory. For example, dalvik-cache/x86 and
  // dalvik-cache/x86_64
  base::FileEnumerator src_directory_iter(src_directory, false,
                                          base::FileEnumerator::DIRECTORIES);
  for (auto src_isa_directory = src_directory_iter.Next();
       !src_isa_directory.empty();
       src_isa_directory = src_directory_iter.Next()) {
    const std::string isa = src_isa_directory.BaseName().value();
    if (!InstallLinksToHostSideCodeInternal(src_isa_directory,
                                            dest_directory.Append(isa), isa)) {
      result = false;
      LOG(INFO) << "InstallLinksToHostSideCodeInternal() for " << isa
                << " failed. "
                << "Deleting container's /data/dalvik-cache...";
      DeleteExecutableFilesInData(true, /* delete dalvik cache */
                                  false /* delete data app executables */);
      break;
    }
  }

  LOG(INFO) << "InstallLinksToHostSideCode() took "
            << timer.Elapsed().InMillisecondsRoundedUp() << "ms";
  return result;
}

void ArcSetup::CreateAndroidCmdlineFile(bool is_dev_mode,
                                        bool is_inside_vm,
                                        bool is_debuggable) {
  const base::FilePath lsb_release_file_path("/etc/lsb-release");
  LOG(INFO) << "Developer mode is " << is_dev_mode;
  LOG(INFO) << "Inside VM is " << is_inside_vm;
  LOG(INFO) << "Debuggable is " << is_debuggable;
  const std::string chromeos_channel =
      GetChromeOsChannelFromFile(lsb_release_file_path);
  LOG(INFO) << "ChromeOS channel is \"" << chromeos_channel << "\"";
  const std::string arc_lcd_density =
      GetEnvOrDie(arc_paths_->env.get(), "ARC_LCD_DENSITY");
  LOG(INFO) << "lcd_density is " << arc_lcd_density;
  const std::string arc_ui_scale =
      GetEnvOrDie(arc_paths_->env.get(), "ARC_UI_SCALE");
  LOG(INFO) << "ui_scale is " << arc_ui_scale;

  const std::string arc_container_ipv4_address =
      GetEnvOrDie(arc_paths_->env.get(), "ARC_CONTAINER_IPV4_ADDRESS");
  const std::string arc_gateway_ipv4_address =
      GetEnvOrDie(arc_paths_->env.get(), "ARC_GATEWAY_IPV4_ADDRESS");

  std::string native_bridge;
  switch (IdentifyBinaryTranslationType()) {
    case ArcBinaryTranslationType::NONE:
      native_bridge = "0";
      break;
    case ArcBinaryTranslationType::HOUDINI:
      native_bridge = "libhoudini.so";
      break;
    case ArcBinaryTranslationType::NDK_TRANSLATION:
      native_bridge = "libndk_translation.so";
      break;
  }
  LOG(INFO) << "native_bridge is \"" << native_bridge << "\"";

  // Note that we are intentionally not setting the ro.kernel.qemu property
  // since that is tied to running the Android emulator, which has a few key
  // differences:
  // * It assumes that ADB is connected through the qemu pipe, which is not
  //   true in Chrome OS' case.
  // * It controls whether the emulated GLES implementation should be used
  //   (but can be overriden by setting ro.kernel.qemu.gles to -1).
  // * It disables a bunch of pixel formats and uses only RGB565.
  // * It disables Bluetooth (which we might do regardless).
  const std::string content = base::StringPrintf(
      "androidboot.hardware=cheets "
      "androidboot.container=1 "
      "androidboot.dev_mode=%d "
      "androidboot.vm=%d "
      "androidboot.debuggable=%d "
      "androidboot.lcd_density=%s "
      "androidboot.ui_scale=%s "
      "androidboot.container_ipv4_address=%s "
      "androidboot.gateway_ipv4_address=%s "
      "androidboot.partial_boot=1 "
      "androidboot.native_bridge=%s "
      "androidboot.chromeos_channel=%s\n",
      is_dev_mode, is_inside_vm, is_debuggable, arc_lcd_density.c_str(),
      arc_ui_scale.c_str(), arc_container_ipv4_address.c_str(),
      arc_gateway_ipv4_address.c_str(), native_bridge.c_str(),
      chromeos_channel.c_str());

  // TODO(yusukes): Stop using ro.boot.partial_boot in the container and
  // remove "androidboot.partial_boot=1".
  EXIT_IF(!WriteToFile(arc_paths_->android_cmdline, 0644, content));
}

void ArcSetup::CreateFakeProcfsFiles() {
  // Android attempts to modify these files in procfs during init
  // Since these files on the host side require root permissions to modify (real
  // root, not android-root), we need to present fake versions to Android.
  constexpr char kProcSecurityContext[] = "u:object_r:proc_security:s0";

  EXIT_IF(!WriteToFile(arc_paths_->fake_kptr_restrict, 0644, "2\n"));
  EXIT_IF(!Chown(kRootUid, kRootGid, arc_paths_->fake_kptr_restrict));
  EXIT_IF(!Chcon(kProcSecurityContext, arc_paths_->fake_kptr_restrict));

  EXIT_IF(!WriteToFile(arc_paths_->fake_mmap_rnd_bits, 0644, "32\n"));
  EXIT_IF(!Chown(kRootUid, kRootGid, arc_paths_->fake_mmap_rnd_bits));
  EXIT_IF(!Chcon(kProcSecurityContext, arc_paths_->fake_mmap_rnd_bits));

  EXIT_IF(!WriteToFile(arc_paths_->fake_mmap_rnd_compat_bits, 0644, "16\n"));
  EXIT_IF(!Chown(kRootUid, kRootGid, arc_paths_->fake_mmap_rnd_compat_bits));
  EXIT_IF(!Chcon(kProcSecurityContext, arc_paths_->fake_mmap_rnd_compat_bits));
}

void ArcSetup::SetUpMountPointForDebugFilesystem(bool is_dev_mode) {
  const base::FilePath sync_mount_directory =
      arc_paths_->debugfs_directory.Append("sync");

  EXIT_IF(!InstallDirectory(0755, kHostRootUid, kHostRootGid,
                            arc_paths_->debugfs_directory));

  // debug/sync does not exist on all kernels so ignore errors
  IGNORE_ERRORS(arc_mounter_->UmountLazily(sync_mount_directory));

  EXIT_IF(
      !InstallDirectory(0755, kSystemUid, kSystemGid, sync_mount_directory));

  const base::FilePath sync_directory("/sys/kernel/debug/sync");

  if (base::DirectoryExists(sync_directory)) {
    EXIT_IF(!Chown(kSystemUid, kSystemGid, sync_directory));
    EXIT_IF(!Chown(kSystemUid, kSystemGid, sync_directory.Append("info")));
    // Kernel change that introduces sw_sync is follows sync/info
    if (base::PathExists(sync_directory.Append("sw_sync")))
      EXIT_IF(!Chown(kSystemUid, kSystemGid, sync_directory.Append("sw_sync")));

    EXIT_IF(!arc_mounter_->BindMount(sync_directory, sync_mount_directory));
  }

  const base::FilePath tracing_mount_directory =
      arc_paths_->debugfs_directory.Append("tracing");

  IGNORE_ERRORS(arc_mounter_->UmountLazily(tracing_mount_directory));
  EXIT_IF(!InstallDirectory(0755, kHostRootUid, kHostRootGid,
                            tracing_mount_directory));

  if (!is_dev_mode)
    return;

  const base::FilePath tracing_directory("/sys/kernel/debug/tracing");
  EXIT_IF(!arc_mounter_->BindMount(tracing_directory, tracing_mount_directory));
}

void ArcSetup::SetUpMountPointForRemovableMedia() {
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->media_mount_directory));
  EXIT_IF(!InstallDirectory(0755, kRootUid, kSystemGid,
                            arc_paths_->media_mount_directory));

  const std::string media_mount_options =
      base::StringPrintf("mode=0755,uid=%u,gid=%u", kRootUid, kSystemGid);
  EXIT_IF(!arc_mounter_->Mount("tmpfs", arc_paths_->media_mount_directory,
                               "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                               media_mount_options.c_str()));
  EXIT_IF(!arc_mounter_->SharedMount(arc_paths_->media_mount_directory));
  EXIT_IF(
      !InstallDirectory(0755, kMediaUid, kMediaGid,
                        arc_paths_->media_mount_directory.Append("removable")));
}

void ArcSetup::SetUpMountPointForAdbd() {
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->adbd_mount_directory));
  EXIT_IF(!InstallDirectory(0770, kShellUid, kShellGid,
                            arc_paths_->adbd_mount_directory));

  const std::string adbd_mount_options =
      base::StringPrintf("mode=0770,uid=%u,gid=%u", kShellUid, kShellGid);
  EXIT_IF(!arc_mounter_->Mount("tmpfs", arc_paths_->adbd_mount_directory,
                               "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                               adbd_mount_options.c_str()));
  EXIT_IF(!arc_mounter_->SharedMount(arc_paths_->adbd_mount_directory));
}

void ArcSetup::CleanUpStaleMountPoints() {
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->media_dest_directory));
}

void ArcSetup::SetUpSharedMountPoints() {
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->shared_mount_directory));
  EXIT_IF(!InstallDirectory(0755, kRootUid, kRootGid,
                            arc_paths_->shared_mount_directory));

  // Use 0755 to make sure only the real root user can write to the shared
  // mount point.
  EXIT_IF(!arc_mounter_->Mount("tmpfs", arc_paths_->shared_mount_directory,
                               "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                               "mode=0755"));
  EXIT_IF(!arc_mounter_->SharedMount(arc_paths_->shared_mount_directory));
}

void ArcSetup::RestoreContext() {
  std::vector<base::FilePath> directories = {
      // Restore the label for the file now since this is the only place to do
      // so.
      // The file is bind-mounted in the container as /proc/cmdline, but unlike
      // /run/arc and /run/camera, the file cannot have the "mount_outside"
      // option
      // because /proc for the container is always mounted inside the container,
      // and cmdline file has to be mounted on top of that.
      arc_paths_->android_cmdline,

      arc_paths_->debugfs_directory,      arc_paths_->obb_mount_directory,
      arc_paths_->sdcard_mount_directory, arc_paths_->sysfs_cpu,
      arc_paths_->sysfs_tracing,
  };
  if (base::DirectoryExists(arc_paths_->restorecon_whitelist_sync))
    directories.push_back(arc_paths_->restorecon_whitelist_sync);
  // usbfs does not exist on test VMs without any USB emulation, skip it there.
  if (base::DirectoryExists(arc_paths_->usb_devices_directory))
    directories.push_back(arc_paths_->usb_devices_directory);

  EXIT_IF(!RestoreconRecursively(directories));
}

void ArcSetup::SetUpGraphicsSysfsContext() {
  const base::FilePath sysfs_drm_path("/sys/class/drm");
  const std::string sysfs_drm_context("u:object_r:gpu_device:s0");
  const std::string render_node_pattern("renderD*");
  const std::vector<std::string> attrs{"uevent",           "config",
                                       "vendor",           "device",
                                       "subsystem_vendor", "subsystem_device"};

  base::FileEnumerator drm_directory(
      sysfs_drm_path, false,
      base::FileEnumerator::FileType::FILES |
          base::FileEnumerator::FileType::DIRECTORIES |
          base::FileEnumerator::FileType::SHOW_SYM_LINKS,
      render_node_pattern);

  for (auto dev = drm_directory.Next(); !dev.empty();
       dev = drm_directory.Next()) {
    auto device = dev.Append("device");

    for (const auto& attr : attrs) {
      auto attr_path = device.Append(attr);
      if (base::PathExists(attr_path))
        EXIT_IF(!Chcon(sysfs_drm_context, Realpath(attr_path)));
    }
  }
}

void ArcSetup::SetUpNetwork() {
  constexpr char kSelinuxContext[] = "u:object_r:system_data_file:s0";
  constexpr gid_t kMiscGid = 9998 + kShiftGid;

  const base::FilePath data_dir =
      arc_paths_->android_mutable_source.Append("data");
  const base::FilePath misc_dir = data_dir.Append("misc");
  const base::FilePath eth_dir = misc_dir.Append("ethernet");
  const base::FilePath ipconfig_dest = eth_dir.Append("ipconfig.txt");

  std::string ip_addr =
      GetEnvOrDie(arc_paths_->env.get(), "ARC_CONTAINER_IPV4_ADDRESS");
  std::string gateway =
      GetEnvOrDie(arc_paths_->env.get(), "ARC_GATEWAY_IPV4_ADDRESS");

  // Get rid of prefix length from ip address.
  const size_t mask_position = ip_addr.find('/');
  if (mask_position != std::string::npos)
    ip_addr.resize(mask_position);

  // Ensure strings aren't too long.
  EXIT_IF(ip_addr.length() > std::numeric_limits<char>::max());
  EXIT_IF(gateway.length() > std::numeric_limits<char>::max());

  const char ip_addr_len = ip_addr.length();
  const char gateway_len = gateway.length();

  EXIT_IF(!InstallDirectory(01771, kSystemUid, kMiscGid, misc_dir));
  EXIT_IF(!Chcon(kSelinuxContext, misc_dir));

  EXIT_IF(!InstallDirectory(0770, kSystemUid, kSystemGid, eth_dir));
  EXIT_IF(!Chcon(kSelinuxContext, eth_dir));

  base::File ipconfig(ipconfig_dest,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  EXIT_IF(!ipconfig.IsValid());

  // The ipconfig.txt file is in network byte order. Since we can reasonably
  // expect the length of the ip address to be less than the maximum value of a
  // signed byte (char), we only use one byte for the length, and put it after a
  // null byte to make a 16 bit interget in network byte order. These null bytes
  // are at the end of the "first_section" and "second_section" to reduce the
  // number of write calls.
  // The file format was reverse engineered from the java class
  // com.android.server.net.IpConfigStore.
  // clang-format off
  constexpr char kFirstSection[] =
      "\0\0\0\x02\0\x02" "id" "\0\0\0\0"
      "\0\x0c" "ipAssignment" "\0\x06" "STATIC"
      "\0\x0b" "linkAddress" "\0";
  ipconfig.WriteAtCurrentPos(kFirstSection, sizeof(kFirstSection) - 1);
  ipconfig.WriteAtCurrentPos(&ip_addr_len, sizeof(ip_addr_len));
  ipconfig.WriteAtCurrentPos(ip_addr.c_str(), ip_addr_len);

  constexpr char kSecondSection[] =
      "\0\0\0" "\x1e"
      "\0\x07" "gateway" "\0\0\0\0\0\0\0\x01" "\0";
  ipconfig.WriteAtCurrentPos(kSecondSection, sizeof(kSecondSection) - 1);
  ipconfig.WriteAtCurrentPos(&gateway_len, sizeof(gateway_len));
  ipconfig.WriteAtCurrentPos(gateway.c_str(), gateway_len);

  constexpr char kThirdSection[] =
      "\0\x03" "dns" "\0\x07" "8.8.8.8"
      "\0\x03" "dns" "\0\x07" "8.8.4.4"
      "\0\x03" "eos";
  ipconfig.WriteAtCurrentPos(kThirdSection, sizeof(kThirdSection) - 1);
  // clang-format on

  EXIT_IF(!Chcon(kSelinuxContext, ipconfig_dest));
  EXIT_IF(!Chown(kSystemUid, kSystemGid, ipconfig_dest));
}

void ArcSetup::MakeMountPointsReadOnly() {
  // NOLINTNEXTLINE(runtime/int)
  constexpr unsigned long remount_flags =
      MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC;
  constexpr char kMountOptions[] = "seclabel,mode=0755";

  const std::string media_mount_options =
      base::StringPrintf("mode=0755,uid=%u,gid=%u", kRootUid, kSystemGid);

  // Make these mount points readonly so that Android container cannot modify
  // files/directories inside these filesystem. Android container cannot remove
  // the readonly flag because it is run in user namespace.
  // These directories are also bind-mounted as read-write into either the
  // SDCARD or arc-obb-mounter container, which runs outside of the user
  // namespace so that such a daemon can modify files/directories inside the
  // filesystem (See also arc-sdcard.conf and arc-obb-mounter.conf).
  EXIT_IF(!arc_mounter_->Remount(arc_paths_->sdcard_mount_directory,
                                 remount_flags, kMountOptions));
  EXIT_IF(!arc_mounter_->Remount(arc_paths_->obb_mount_directory, remount_flags,
                                 kMountOptions));
  EXIT_IF(!arc_mounter_->Remount(arc_paths_->media_mount_directory,
                                 remount_flags, media_mount_options.c_str()));
}

void ArcSetup::SetUpCameraProperty() {
  // Camera HAL V3 needs two properties from build.prop for picture taking.
  // Copy the information to /var/cache.
  const base::FilePath camera_prop_directory("/var/cache/camera");
  const base::FilePath camera_prop_file =
      base::FilePath(camera_prop_directory).Append("camera.prop");
  if (base::PathExists(camera_prop_file))
    return;

  if (!MkdirRecursively(camera_prop_directory))
    return;

  const base::FilePath build_prop =
      arc_paths_->android_rootfs_directory.Append("system/build.prop");
  std::string content;
  EXIT_IF(!base::ReadFileToString(build_prop, &content));

  std::vector<std::string> properties = base::SplitString(
      content, "\n", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_ALL);
  const std::string kManufacturer = "ro.product.manufacturer";
  const std::string kModel = "ro.product.model";
  std::string camera_properties;
  for (const auto& property : properties) {
    if (!property.compare(0, kManufacturer.length(), kManufacturer) ||
        !property.compare(0, kModel.length(), kModel)) {
      camera_properties += property + "\n";
    }
  }
  EXIT_IF(!WriteToFile(camera_prop_file, 0644, camera_properties));
}

void ArcSetup::SetUpSharedApkDirectory() {
  // TODO(yusuks): Remove the migration code in M68.
  if (base::PathExists(arc_paths_->old_apk_cache_dir)) {
    // The old directory is found. Move it to the new location. Still call
    // InstallDirectory() to make sure permissions, uid, and gid are all
    // correct.
    EXIT_IF(
        !base::Move(arc_paths_->old_apk_cache_dir, arc_paths_->apk_cache_dir));
  }

  EXIT_IF(!InstallDirectory(0700, kSystemUid, kSystemGid,
                            arc_paths_->apk_cache_dir));
}

void ArcSetup::CleanUpBinFmtMiscSetUp() {
  const std::string system_arch = base::SysInfo::OperatingSystemArchitecture();
  if (system_arch != "x86_64")
    return;
  std::unique_ptr<ScopedMount> binfmt_misc_mount =
      ScopedMount::CreateScopedMount(
          arc_mounter_.get(), "binfmt_misc", arc_paths_->binfmt_misc_directory,
          "binfmt_misc", MS_NOSUID | MS_NODEV | MS_NOEXEC, nullptr);
  // This function is for Mode::STOP. Ignore errors to make sure to run all
  // clean up code.
  if (!binfmt_misc_mount) {
    PLOG(INFO) << "Ignoring failure: Failed to mount binfmt_misc";
    return;
  }

  for (auto entry_name : kBinFmtMiscEntryNames) {
    UnregisterBinFmtMiscEntry(
        arc_paths_->binfmt_misc_directory.Append(entry_name));
  }
}

AndroidSdkVersion ArcSetup::GetSdkVersion() {
  const base::FilePath build_prop =
      arc_paths_->android_rootfs_directory.Append("system/build.prop");
  if (!base::PathExists(build_prop)) {
    // --onetime-setup always takes this path.
    LOG(INFO) << "SDK version unknown: " << build_prop.value()
              << " does not exist.";
    return AndroidSdkVersion::UNKNOWN;
  }

  std::string version_str;
  EXIT_IF(
      !GetPropertyFromFile(build_prop, "ro.build.version.sdk", &version_str));
  LOG(INFO) << "SDK version string: " << version_str;

  int version;
  EXIT_IF(!base::StringToInt(version_str, &version));
  LOG(INFO) << "SDK version: " << version;

  // TODO(yusukes): What 27 actually means is O-MR1, not P. Once our image
  // starts using 28, remove 'case 27:'.
  switch (version) {
    case 25:
      return AndroidSdkVersion::ANDROID_N_MR1;
    case 27:
    case 28:
      return AndroidSdkVersion::ANDROID_P;
  }

  CHECK(false) << "Unknown SDK version.";
  return AndroidSdkVersion::UNKNOWN;
}

void ArcSetup::UnmountOnStop() {
  // This function is for Mode::STOP. Use IGNORE_ERRORS to make sure to run all
  // clean up code.
  IGNORE_ERRORS(arc_mounter_->UmountLazily(
      arc_paths_->shared_mount_directory.Append("cache")));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(
      arc_paths_->shared_mount_directory.Append("data")));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->adbd_mount_directory));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->media_dest_directory));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->media_mount_directory));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->sdcard_mount_directory));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->shared_mount_directory));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->obb_mount_directory));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->oem_mount_directory));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->android_mutable_source));
  IGNORE_ERRORS(
      arc_mounter_->UmountLazily(arc_paths_->debugfs_directory.Append("sync")));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(
      arc_paths_->debugfs_directory.Append("tracing")));
  // Clean up in case this was not unmounted.
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->binfmt_misc_directory));
  IGNORE_ERRORS(
      arc_mounter_->UmountLazily(arc_paths_->android_rootfs_directory.Append(
          arc_paths_->system_lib_arm_directory_relative)));
}

void ArcSetup::RemoveBugreportPipe() {
  // This function is for Mode::STOP. Use IGNORE_ERRORS to make sure to run all
  // clean up code.
  IGNORE_ERRORS(base::DeleteFile(base::FilePath("/run/arc/bugreport/pipe"),
                                 false /* recursive */));
}

void ArcSetup::RemoveAndroidKmsgFifo() {
  // This function is for Mode::STOP. Use IGNORE_ERRORS to make sure to run all
  // clean up code.
  IGNORE_ERRORS(
      base::DeleteFile(arc_paths_->android_kmsg_fifo, false /* recursive */));
}

std::string ArcSetup::GetSystemImageFingerprint() {
  constexpr char kFingerprintProp[] = "ro.build.fingerprint";

  const base::FilePath build_prop =
      arc_paths_->android_rootfs_directory.Append("system/build.prop");

  std::string system_fingerprint;
  EXIT_IF(
      !GetPropertyFromFile(build_prop, kFingerprintProp, &system_fingerprint));
  EXIT_IF(system_fingerprint.empty());
  return system_fingerprint;
}

ArcBootType ArcSetup::GetBootType() {
  const std::string system_fingerprint = GetSystemImageFingerprint();

  // Note: The XML file name has to match com.android.server.pm.Settings's
  // mSettingsFilename. This will be very unlikely to change, but we still need
  // to be careful.
  const base::FilePath packages_xml =
      arc_paths_->android_data_directory.Append("data/system/packages.xml");

  if (!base::PathExists(packages_xml)) {
    // This path is taken when /data is empty, which is not an error.
    LOG(INFO) << packages_xml.value()
              << " does not exist. This is the very first boot aka opt-in.";
    return ArcBootType::FIRST_BOOT;
  }

  // Get a fingerprint from /data/system/packages.xml.
  std::string data_fingerprint;
  if (!GetFingerprintFromPackagesXml(packages_xml, &data_fingerprint)) {
    LOG(ERROR) << "Failed to get a fingerprint from " << packages_xml.value();
    // Return kFirstBootAfterUpdate so the caller invalidates art/oat files
    // which is safer than returning kRegularBoot.
    return ArcBootType::FIRST_BOOT_AFTER_UPDATE;
  }

  // If two fingerprints don't match, this is the first boot after OTA.
  // Android's PackageManagerService.isUpgrade(), at least on M, N, and
  // O releases, does exactly the same to detect OTA.
  // TODO(yusukes): Check Android P's behavior when it is ready.
  const bool ota_detected = system_fingerprint != data_fingerprint;
  if (!ota_detected) {
    LOG(INFO) << "This is regular boot.";
  } else {
    LOG(INFO) << "This is the first boot after OTA: system="
              << system_fingerprint << ", data=" << data_fingerprint;
  }
  return ota_detected ? ArcBootType::FIRST_BOOT_AFTER_UPDATE
                      : ArcBootType::REGULAR_BOOT;
}

void ArcSetup::ShouldDeleteDataExecutables(
    ArcBootType boot_type,
    bool* out_should_delete_data_dalvik_cache_directory,
    bool* out_should_delete_data_app_executables) {
  if (boot_type == ArcBootType::FIRST_BOOT_AFTER_UPDATE &&
      GetBooleanEnvOrDie(arc_paths_->env.get(),
                         "DELETE_DATA_EXECUTABLES_AFTER_OTA")) {
    *out_should_delete_data_dalvik_cache_directory = true;
    *out_should_delete_data_app_executables = true;
    return;
  }
  *out_should_delete_data_dalvik_cache_directory =
      GetBooleanEnvOrDie(arc_paths_->env.get(), "DELETE_DATA_DALVIK_CACHE_DIR");
  *out_should_delete_data_app_executables =
      GetBooleanEnvOrDie(arc_paths_->env.get(), "DELETE_DATA_APP_EXECUTABLES");
}

std::string ArcSetup::GetSalt() {
  constexpr char kSaltFile[] = "/home/.shadow/salt";
  constexpr size_t kSaltFileSize = 16;

  std::string per_machine_salt;
  if (!base::ReadFileToString(base::FilePath(kSaltFile), &per_machine_salt) ||
      per_machine_salt.size() < kSaltFileSize) {
    LOG(WARNING) << kSaltFile << " is not available yet. OOBE boot?";
    return std::string();
  }
  if (per_machine_salt.size() != kSaltFileSize) {
    LOG(WARNING) << "Unexpected " << kSaltFile
                 << " size: " << per_machine_salt.size();
  }
  return per_machine_salt;
}

std::string ArcSetup::GetSerialNumber() {
  const std::string chromeos_user =
      GetEnvOrDie(arc_paths_->env.get(), "CHROMEOS_USER");
  const std::string salt = GetSalt();
  EXIT_IF(salt.empty());  // at this point, the salt file should always exist.
  return arc::GenerateFakeSerialNumber(chromeos_user, salt);
}

void ArcSetup::MountSharedAndroidDirectories() {
  const base::FilePath cache_directory =
      arc_paths_->android_data_directory.Append("cache");
  const base::FilePath data_directory =
      arc_paths_->android_data_directory.Append("data");
  const base::FilePath data_cache_directory = data_directory.Append("cache");

  const base::FilePath shared_cache_directory =
      arc_paths_->shared_mount_directory.Append("cache");
  const base::FilePath shared_data_directory =
      arc_paths_->shared_mount_directory.Append("data");

  if (!base::PathExists(shared_data_directory)) {
    EXIT_IF(!InstallDirectory(0700, kHostRootUid, kHostRootGid,
                              shared_data_directory));
  }

  switch (sdk_version()) {
    case AndroidSdkVersion::UNKNOWN:
      NOTREACHED();
      break;
    case AndroidSdkVersion::ANDROID_N_MR1:
      if (!base::PathExists(shared_cache_directory)) {
        EXIT_IF(!InstallDirectory(0700, kHostRootUid, kHostRootGid,
                                  shared_cache_directory));
      }
      break;
    case AndroidSdkVersion::ANDROID_P:
      // TODO(yusukes): Double-check if we really want to migrate N's /cache as
      // /data/cache in P. If we don't, we can remove all the code for master
      // as well as the MS_REC flag in this function.
      if (!base::PathExists(data_cache_directory)) {
        EXIT_IF(!InstallDirectory(0700, kHostRootUid, kHostRootGid,
                                  data_cache_directory));
      }
      break;
  }

  // First, make the original data directory a mount point and also make it
  // executable. This has to be done *before* passing the directory into
  // the shared mount point because the new flags won't be propagated if the
  // mount point has already been shared with the slave.
  EXIT_IF(!arc_mounter_->BindMount(data_directory, data_directory));
  EXIT_IF(
      !arc_mounter_->Remount(data_directory, MS_NOSUID | MS_NODEV, "seclabel"));

  // Then, bind-mount /cache to the shared mount point. On master, pass
  // |cache_directory| as part of |data_directory| instead.
  switch (sdk_version()) {
    case AndroidSdkVersion::UNKNOWN:
      NOTREACHED();
      break;
    case AndroidSdkVersion::ANDROID_N_MR1:
      EXIT_IF(
          !arc_mounter_->BindMount(cache_directory, shared_cache_directory));
      break;
    case AndroidSdkVersion::ANDROID_P:
      EXIT_IF(!arc_mounter_->BindMount(cache_directory, data_cache_directory));
      break;
  }

  // Finally, bind-mount /data to the shared mount point. Note that MS_REC is
  // not needed for non-master builds, but it doesn't hurt either.
  EXIT_IF(!arc_mounter_->Mount(data_directory.value(), shared_data_directory,
                               nullptr, MS_BIND | MS_REC, nullptr));
}

void ArcSetup::UnmountSharedAndroidDirectories() {
  const base::FilePath data_directory =
      arc_paths_->android_data_directory.Append("data");
  const base::FilePath shared_cache_directory =
      arc_paths_->shared_mount_directory.Append("cache");
  const base::FilePath shared_data_directory =
      arc_paths_->shared_mount_directory.Append("data");

  IGNORE_ERRORS(arc_mounter_->UmountLazily(data_directory));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(shared_cache_directory));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(shared_data_directory));
  IGNORE_ERRORS(arc_mounter_->UmountLazily(arc_paths_->shared_mount_directory));
}

void ArcSetup::MaybeStartAdbdProxy(bool is_dev_mode,
                                   bool is_inside_vm,
                                   const std::string& serialnumber) {
  if (!is_dev_mode || is_inside_vm)
    return;
  const base::FilePath adbd_config_path("/etc/arc/adbd.json");
  if (!base::PathExists(adbd_config_path))
    return;

  // Now that we have identified that the system is capable of continuing, touch
  // the path where the FIFO will be located.
  const base::FilePath control_endpoint_path("/run/arc/adbd/ep0");
  EXIT_IF(!CreateOrTruncate(control_endpoint_path, 0600));
  EXIT_IF(!Chown(kShellUid, kShellGid, control_endpoint_path));

  EXIT_IF(!LaunchAndWait(
      {"/sbin/initctl", "start", "--no-wait", "arc-adbd",
       base::StringPrintf("SERIALNUMBER=%s", serialnumber.c_str())}));
}

void ArcSetup::ContinueContainerBoot(ArcBootType boot_type,
                                     const std::string& serialnumber) {
  constexpr char kCommand[] = "/system/bin/arcbootcontinue";

  // Run |kCommand| on the container side. The binary does the following:
  // * Bind-mount the actual cache and data in /var/arc/shared_mounts to /cache
  //   and /data.
  // * Set ro.boot.serialno and others.
  // * Then, set ro.data_mounted=1 to ask /init to start the processes in the
  //   "main" class.
  // We don't use -S (set UID), -G (set GID), and /system/bin/runcon here and
  // instead run the command with UID 0 (host's root) because our goal is to
  // remove or reduce [u]mount operations from the container, especially from
  // its /init, and then to enforce it with SELinux.
  const std::vector<std::string> command_line = {
      "/usr/bin/nsenter", "-t",
      GetEnvOrDie(arc_paths_->env.get(), "CONTAINER_PID"),
      "-m",  // enter mount namespace
      "-U",  // enter user namespace
      "-i",  // enter System V IPC namespace
      "-n",  // enter network namespace
      "-p",  // enter pid namespace
      "-r",  // set the root directory
      "-w",  // set the working directory
      "--", kCommand, "--serialno", serialnumber, "--disable-boot-completed",
      GetEnvOrDie(arc_paths_->env.get(), "DISABLE_BOOT_COMPLETED_BROADCAST"),
      "--vendor-privileged",
      GetEnvOrDie(arc_paths_->env.get(), "ENABLE_VENDOR_PRIVILEGED"),
      "--container-boot-type", std::to_string(static_cast<int>(boot_type)),
      // When this COPY_PACKAGES_CACHE is set to "1", SystemServer copies
      // /data/system/pacakges.xml to /data/system/pacakges_copy.xml after the
      // initialization stage of PackageManagerService.
      "--copy-packages-cache",
      GetEnvOrDie(arc_paths_->env.get(), "COPY_PACKAGES_CACHE"),
  };

  base::ElapsedTimer timer;
  EXIT_IF(!LaunchAndWait(command_line));
  LOG(INFO) << "Running " << kCommand << " took "
            << timer.Elapsed().InMillisecondsRoundedUp() << "ms";
}

void ArcSetup::EnsureContainerDirectories() {
  // uid/gid will be modified by cras.conf later.
  // FIXME(b/64553266): Work around push_to_device/deploy_vendor_image running
  // arc_setup after cras.conf by skipping the setup if the directory exists.
  if (!base::DirectoryExists(arc_paths_->cras_socket_directory))
    EXIT_IF(!InstallDirectory(01770, kHostRootUid, kHostRootGid,
                              arc_paths_->cras_socket_directory));
}

void ArcSetup::MountOnOnetimeSetup() {
  const bool is_writable =
      GetBooleanEnvOrDie(arc_paths_->env.get(), "WRITABLE_MOUNT");
  const unsigned long writable_flag =  // NOLINT(runtime/int)
      is_writable ? 0 : MS_RDONLY;

  if (is_writable)
    EXIT_IF(!arc_mounter_->Remount(base::FilePath("/"), 0 /* rw */, nullptr));

  // Try to drop as many privileges as possible. If we end up starting ARC,
  // we'll bind-mount the rootfs directory in the container-side with the
  // appropriate flags.
  EXIT_IF(!arc_mounter_->LoopMount(
      kSystemImage, arc_paths_->android_rootfs_directory,
      MS_NOEXEC | MS_NOSUID | MS_NODEV | writable_flag));

  unsigned long kBaseFlags =  // NOLINT(runtime/int)
      writable_flag | MS_NOEXEC | MS_NOSUID;

  // Though we can technically mount these in mount namespace with minijail,
  // we do not bother to handle loopback mounts by ourselves but just mount it
  // in host namespace. Unlike system.raw.img, these images are always squashfs.
  // Unlike system.raw.img, we don't remount them as exec either. The images do
  // not contain any executables.
  EXIT_IF(!arc_mounter_->LoopMount(
      kPassthroughImage, arc_paths_->media_rootfs_directory, kBaseFlags));
  EXIT_IF(!arc_mounter_->LoopMount(
      kSdcardRootfsImage, arc_paths_->sdcard_rootfs_directory, kBaseFlags));
  EXIT_IF(!arc_mounter_->LoopMount(
      kObbRootfsImage, arc_paths_->obb_rootfs_directory, kBaseFlags));
}

void ArcSetup::UnmountOnOnetimeStop() {
  IGNORE_ERRORS(arc_mounter_->LoopUmount(arc_paths_->obb_rootfs_directory));
  IGNORE_ERRORS(arc_mounter_->LoopUmount(arc_paths_->sdcard_rootfs_directory));
  IGNORE_ERRORS(arc_mounter_->LoopUmount(arc_paths_->media_rootfs_directory));
  IGNORE_ERRORS(arc_mounter_->LoopUmount(arc_paths_->android_rootfs_directory));
}

void ArcSetup::BindMountInContainerNamespaceOnPreChroot(
    const base::FilePath& rootfs,
    const ArcBinaryTranslationType binary_translation_type) {
  if (binary_translation_type == ArcBinaryTranslationType::HOUDINI) {
    // system_lib_arm either is empty or contains ndk-translation's libraries.
    // Since houdini is selected bind-mount its libraries instead.
    EXIT_IF(!arc_mounter_->BindMount(
        rootfs.Append("vendor/lib/arm"),
        rootfs.Append(arc_paths_->system_lib_arm_directory_relative)));
  }
}

void ArcSetup::RestoreContextOnPreChroot(const base::FilePath& rootfs) {
  {
    // The list of container directories that need to be recursively re-labeled.
    // Note that "var/run" (the parent directory) is not in the list  because
    // some of entries in the directory are on a read-only filesystem.
    // Note: The array is for directories. Do no add files to the array. Add
    // them to |kPaths| below instead.
    constexpr std::array<const char*, 8> kDirectories{
        "dev",
        "oem",
        "var/run/arc/apkcache",
        "var/run/arc/bugreport",
        "var/run/arc/dalvik-cache",
        "var/run/camera",
        "var/run/chrome",
        "var/run/cras"};

    // Transform |kDirectories| because the mount points are visible only in
    // |rootfs|. Note that Chrome OS' file_contexts does recognize paths with
    // the |rootfs| prefix.
    EXIT_IF(!RestoreconRecursively(
        PrependPath(kDirectories.cbegin(), kDirectories.cend(), rootfs)));
  }

  {
    // Do the same as above for files and directories but in a non-recursive
    // way.
    constexpr std::array<const char*, 5> kPaths{
        "default.prop", "sys/kernel/debug", "system/build.prop", "var/run/arc",
        "var/run/inputbridge"};
    EXIT_IF(!Restorecon(PrependPath(kPaths.cbegin(), kPaths.cend(), rootfs)));
  }
}

void ArcSetup::CreateDevColdbootDoneOnPreChroot(const base::FilePath& rootfs) {
  const base::FilePath coldboot_done = rootfs.Append("dev/.coldboot_done");
  // TODO(yusukes): Once N finishes migrating to run_oci, add an auto test for
  // checking this file to cheets_LoginScreen.
  EXIT_IF(!CreateOrTruncate(coldboot_done, 0755));
  EXIT_IF(!Chown(kRootUid, kRootGid, coldboot_done));
}

void ArcSetup::OnSetup() {
  const bool is_dev_mode =
      GetBooleanEnvOrDie(arc_paths_->env.get(), "CHROMEOS_DEV_MODE");
  const bool is_inside_vm =
      GetBooleanEnvOrDie(arc_paths_->env.get(), "CHROMEOS_INSIDE_VM");
  const bool is_debuggable =
      GetBooleanEnvOrDie(arc_paths_->env.get(), "ANDROID_DEBUGGABLE");

  // The host-side dalvik-cache directory is mounted into the container
  // via the json file. Create it regardless of whether the code integrity
  // feature is enabled.
  ArtContainer::CreateArtContainerDataDirectory();

  // Mount host-compiled and host-verified .art and .oat files. The container
  // will see these files, but other than that, the /data and /cache
  // directories are empty and read-only which is the best for security.

  // Unconditionally generate host-side code here.
  base::ElapsedTimer timer;
  GenerateHostSideCode(arc_paths_->art_dalvik_cache_directory);

  // For now, integrity checking time is the time needed to relocate
  // boot*.art files because of b/67912719. Once TPM is enabled, this will
  // report the total time spend on code verification + [relocation + sign]
  arc_setup_metrics_->SendCodeIntegrityCheckingTotalTime(timer.Elapsed());
  SetUpDalvikCache();

  SetUpSharedMountPoints();
  CreateContainerFilesAndDirectories();
  ApplyPerBoardConfigurations();
  SetUpSharedTmpfsForExternalStorage();
  SetUpFilesystemForObbMounter();
  CreateAndroidCmdlineFile(is_dev_mode, is_inside_vm, is_debuggable);
  CreateFakeProcfsFiles();
  SetUpMountPointForDebugFilesystem(is_dev_mode);
  SetUpMountPointForRemovableMedia();
  SetUpMountPointForAdbd();
  CleanUpStaleMountPoints();
  RestoreContext();
  SetUpGraphicsSysfsContext();
  MakeMountPointsReadOnly();
  SetUpCameraProperty();
  SetUpSharedApkDirectory();

  // These should be the last thing OnSetup() does because the job and
  // directories are not needed for arc-setup. Only the container's startup code
  // (in session_manager side) requires the job and directories.
  WaitForRtLimitsJob();
}

void ArcSetup::OnBootContinue() {
  // This feature is only available in NYC branch.
  // TODO(khmel): Support them in P and remove this block.
  if (sdk_version() != AndroidSdkVersion::ANDROID_N_MR1) {
    EXIT_IF(!arc_paths_->env->SetVar("SKIP_PACKAGES_CACHE_SETUP", "1"));
    EXIT_IF(!arc_paths_->env->SetVar("COPY_PACKAGES_CACHE", "0"));
  }

  const bool is_dev_mode =
      GetBooleanEnvOrDie(arc_paths_->env.get(), "CHROMEOS_DEV_MODE");
  const bool is_inside_vm =
      GetBooleanEnvOrDie(arc_paths_->env.get(), "CHROMEOS_INSIDE_VM");
  const std::string serialnumber = GetSerialNumber();
  const ArcBootType boot_type = GetBootType();
  bool should_delete_data_dalvik_cache_directory;
  bool should_delete_data_app_executables;
  ShouldDeleteDataExecutables(boot_type,
                              &should_delete_data_dalvik_cache_directory,
                              &should_delete_data_app_executables);
  DeleteExecutableFilesInData(should_delete_data_dalvik_cache_directory,
                              should_delete_data_app_executables);
  CleanUpDalvikCache();

  // The socket isn't created when the mini-container is started, so the
  // arc-setup --pre-chroot call won't label it. Label it here instead.
  EXIT_IF(!Chcon(kArcBridgeSocketContext, arc_paths_->arc_bridge_socket_path));

  // Set up |android_mutable_source|. Although the container does not use
  // the directory directly, we should still set up the directory so that
  // session_manager can delete (to be more precise, move) the directory
  // on opt-out. Since this creates cache and data directories when they
  // don't exist, this has to be done before calling ShareAndroidData().
  SetUpAndroidData();

  if (!InstallLinksToHostSideCode()) {
    arc_setup_metrics_->SendBootContinueCodeInstallationResult(
        ArcBootContinueCodeInstallationResult::ERROR_CANNOT_INSTALL_HOST_CODE);
  } else {
    arc_setup_metrics_->SendBootContinueCodeInstallationResult(
        ArcBootContinueCodeInstallationResult::SUCCESS);
  }

  // Set up /run/arc/shared_mounts/{cache,data} to expose the user's data to
  // the container.
  MountSharedAndroidDirectories();

  // Asks the container to continue boot.
  MaybeStartUreadaheadInTracingMode();
  MaybeStartAdbdProxy(is_dev_mode, is_inside_vm, serialnumber);
  ContinueContainerBoot(boot_type, serialnumber);
  // Unmount /run/arc/shared_mounts and its children. They are unnecessary at
  // this point.
  UnmountSharedAndroidDirectories();
}

void ArcSetup::OnStop() {
  CleanUpBinFmtMiscSetUp();
  CleanUpDalvikCache();
  UnmountOnStop();
  RemoveBugreportPipe();
  RemoveAndroidKmsgFifo();
}

void ArcSetup::OnOnetimeSetup() {
  EnsureContainerDirectories();
  MountOnOnetimeSetup();

  // Build properties are needed to finish booting the container, so we need
  // to set them up here instead of in the per-board setup.
  CreateBuildProperties();
}

void ArcSetup::OnOnetimeStop() {
  UnmountOnOnetimeStop();
}

void ArcSetup::OnPreChroot() {
  // binfmt_misc setup has to be done before entering container
  // namespace below (namely before CreateScopedMountNamespaceForPid).
  ArcBinaryTranslationType binary_translation_type =
      IdentifyBinaryTranslationType();
  SetUpBinFmtMisc(binary_translation_type);

  int container_pid;
  base::FilePath rootfs;

  EXIT_IF(!GetOciContainerState(base::FilePath("/dev/stdin"), &container_pid,
                                &rootfs));

  // Enter the container namespace since the paths we want to re-label here
  // are easier to access from inside of it.
  std::unique_ptr<ScopedMountNamespace> container_mount_ns =
      ScopedMountNamespace::CreateScopedMountNamespaceForPid(container_pid);
  PLOG_IF(FATAL, !container_mount_ns)
      << "Failed to enter the container mount namespace";

  BindMountInContainerNamespaceOnPreChroot(rootfs, binary_translation_type);
  RestoreContextOnPreChroot(rootfs);
  CreateDevColdbootDoneOnPreChroot(rootfs);
}

void ArcSetup::OnReadAhead() {
  EmulateArcUreadahead(arc_paths_->android_rootfs_directory, kReadAheadTimeout);
}

void ArcSetup::Run() {
  switch (arc_paths_->mode) {
    case Mode::SETUP:
      OnSetup();
      break;
    case Mode::STOP:
      OnStop();
      break;
    case Mode::BOOT_CONTINUE:
      OnBootContinue();
      break;
    case Mode::ONETIME_SETUP:
      OnOnetimeSetup();
      break;
    case Mode::ONETIME_STOP:
      OnOnetimeStop();
      break;
    case Mode::PRE_CHROOT:
      OnPreChroot();
      break;
    case Mode::READ_AHEAD:
      OnReadAhead();
      break;
    case Mode::UNKNOWN:
      NOTREACHED();
      break;
  }
}

void ArcSetup::MountOnOnetimeSetupForTesting() {
  MountOnOnetimeSetup();
}

void ArcSetup::UnmountOnOnetimeStopForTesting() {
  UnmountOnOnetimeStop();
}

}  // namespace arc
