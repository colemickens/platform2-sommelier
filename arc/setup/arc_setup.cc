// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/setup/arc_setup.h"

#include <fcntl.h>
#include <inttypes.h>
#include <linux/magic.h>
#include <sched.h>
#include <selinux/selinux.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/environment.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <base/sys_info.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <base/timer/elapsed_timer.h>
#include <brillo/cryptohome.h>
#include <brillo/file_utils.h>
#include <chromeos-config/libcros_config/cros_config.h>
#include <crypto/random.h>
#include <metrics/bootstat.h>
#include <metrics/metrics_library.h>

#include "arc/setup/arc_read_ahead.h"
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
#define AID_ROOT 0         /* traditional unix root user */
#define AID_SYSTEM 1000    /* system server */
#define AID_LOG 1007       /* log devices */
#define AID_SDCARD_RW 1015 /* external storage write access */
#define AID_MEDIA_RW 1023  /* internal media storage write access */
#define AID_SHELL 2000     /* adb and debug shell user */
#define AID_CACHE 2001     /* cache access */
#define AID_EVERYBODY 9997 /* shared between all apps in the same profile */

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
constexpr char kDefaultAppsBoardDirectory[] = "/var/cache/arc_default_apps";
constexpr char kDefaultAppsDirectory[] =
    "/usr/share/google-chrome/extensions/arc";
constexpr char kFakeKptrRestrict[] = "/run/arc/fake_kptr_restrict";
constexpr char kFakeMmapRndBits[] = "/run/arc/fake_mmap_rnd_bits";
constexpr char kFakeMmapRndCompatBits[] = "/run/arc/fake_mmap_rnd_compat_bits";
constexpr char kHostSideDalvikCacheDirectoryInContainer[] =
    "/var/run/arc/dalvik-cache";
constexpr char kHostDownloadsDirectory[] = "/home/chronos/user/Downloads";
constexpr char kMediaDestDirectory[] = "/run/arc/media/removable";
constexpr char kMediaDestDefaultDirectory[] =
    "/run/arc/media/removable-default";
constexpr char kMediaDestReadDirectory[] = "/run/arc/media/removable-read";
constexpr char kMediaDestWriteDirectory[] = "/run/arc/media/removable-write";
constexpr char kMediaMountDirectory[] = "/run/arc/media";
constexpr char kMediaProfileFile[] = "media_profiles.xml";
constexpr char kObbMountDirectory[] = "/run/arc/obb";
constexpr char kObbRootfsDirectory[] =
    "/opt/google/containers/arc-obb-mounter/mountpoints/container-root";
constexpr char kObbRootfsImage[] =
    "/opt/google/containers/arc-obb-mounter/rootfs.squashfs";
constexpr char kOemMountDirectory[] = "/run/arc/oem";
constexpr char kPlatformXmlFileRelative[] = "etc/permissions/platform.xml";
constexpr char kRestoreconWhitelistSync[] = "/sys/kernel/debug/sync";
constexpr char kSdcardConfigfsDirectory[] = "/sys/kernel/config/sdcardfs";
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

constexpr uid_t kHostChronosUid = 1000;
constexpr gid_t kHostChronosGid = 1000;

constexpr uid_t kHostArcCameraUid = 603;
constexpr gid_t kHostArcCameraGid = 603;

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
constexpr gid_t kSdcardRwGid = AID_SDCARD_RW + kShiftGid;
constexpr gid_t kEverybodyGid = AID_EVERYBODY + kShiftGid;

// The maximum time arc::EmulateArcUreadahead() can spend.
constexpr base::TimeDelta kReadAheadTimeout = base::TimeDelta::FromSeconds(7);
// The maximum time to wait for /data/media setup.
constexpr base::TimeDelta kInstalldTimeout = base::TimeDelta::FromSeconds(60);

// The IPV4 address of the container.
constexpr char kArcContainerIPv4Address[] = "100.115.92.2/30";

// The IPV4 address of the gateway inside the container. This corresponds to the
// address of "br0".
constexpr char kArcGatewayIPv4Address[] = "100.115.92.1";

// Property name for fingerprint.
constexpr char kFingerprintProp[] = "ro.build.fingerprint";

// System salt and arc salt file size.
constexpr size_t kSaltFileSize = 16;

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
    const base::FilePath format_path = binfmt_misc_directory.Append(entry_name);
    if (base::PathExists(format_path)) {
      // If we had already registered this format earlier and failed
      // unregistering it for some reason, the next operation will fail.
      LOG(WARNING) << "Skipping re-registration of " << entry_path.value();
      continue;
    }
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

// Returns SDK version upgrade type to be sent to UMA.
ArcSdkVersionUpgradeType GetUpgradeType(AndroidSdkVersion system_sdk_version,
                                        AndroidSdkVersion data_sdk_version) {
  if (data_sdk_version == AndroidSdkVersion::UNKNOWN ||  // First boot
      data_sdk_version == system_sdk_version) {
    return ArcSdkVersionUpgradeType::NO_UPGRADE;
  }
  if (data_sdk_version == AndroidSdkVersion::ANDROID_M) {
    if (system_sdk_version == AndroidSdkVersion::ANDROID_N_MR1)
      return ArcSdkVersionUpgradeType::M_TO_N;
    if (system_sdk_version == AndroidSdkVersion::ANDROID_P)
      return ArcSdkVersionUpgradeType::M_TO_P;
  }
  if (data_sdk_version == AndroidSdkVersion::ANDROID_N_MR1 &&
      system_sdk_version == AndroidSdkVersion::ANDROID_P) {
    return ArcSdkVersionUpgradeType::N_TO_P;
  }
  if (data_sdk_version < system_sdk_version) {
    // TODO(niwa): Handle P to Q upgrade.
    LOG(ERROR) << "Unexpected Upgrade: data_sdk_version="
               << static_cast<int>(data_sdk_version) << " system_sdk_version="
               << static_cast<int>(system_sdk_version);
    return ArcSdkVersionUpgradeType::UNKNOWN_UPGRADE;
  }
  LOG(ERROR) << "Unexpected Downgrade: data_sdk_version="
             << static_cast<int>(data_sdk_version)
             << " system_sdk_version=" << static_cast<int>(system_sdk_version);
  return ArcSdkVersionUpgradeType::UNKNOWN_DOWNGRADE;
}

// Checks whether to clear entire android data directory before starting the
// container by comparing |system_sdk_version| from the current boot against
// |data_sdk_version| from the previous boot.
bool ShouldDeleteAndroidData(AndroidSdkVersion system_sdk_version,
                             AndroidSdkVersion data_sdk_version) {
  // Downgraded from P to N. (b/80113276)
  // TODO(niwa): Handle Q to P too.
  if (data_sdk_version == AndroidSdkVersion::ANDROID_P &&
      system_sdk_version == AndroidSdkVersion::ANDROID_N_MR1) {
    LOG(INFO) << "Clearing /data dir because ARC was downgraded from P to N.";
    return true;
  }
  // Upgraded from M to P. (b/77591360)
  if (data_sdk_version == AndroidSdkVersion::ANDROID_M &&
      system_sdk_version == AndroidSdkVersion::ANDROID_P) {
    LOG(INFO) << "Clearing /data dir because ARC was upgraded from M to P.";
    return true;
  }
  return false;
}

void CheckProcessIsAliveOrExit(const std::string& pid_str) {
  pid_t pid;
  EXIT_IF(!base::StringToInt(pid_str, &pid));
  if (!IsProcessAlive(pid)) {
    LOG(ERROR) << "Process " << pid << " is NOT alive";
    exit(EXIT_FAILURE);
  }
  LOG(INFO) << "Process " << pid << " is still alive, at least as a zombie";
  // TODO(yusukes): Check if the PID is a zombie or not, and log accordingly.
}

void CheckNamespacesAvailableOrExit(const std::string& pid_str) {
  const base::FilePath proc("/proc");
  const base::FilePath ns = proc.Append(pid_str).Append("ns");
  EXIT_IF(!base::PathExists(ns));
  for (const char* entry :
       {"cgroup", "ipc", "mnt", "net", "pid", "user", "uts"}) {
    // Use the same syscall, open, as nsenter. Other syscalls like lstat may
    // succeed when open doesn't.
    const base::FilePath path_to_check = ns.Append(entry);
    base::ScopedFD fd(open(path_to_check.value().c_str(), O_RDONLY));
    if (!fd.is_valid()) {
      PLOG(ERROR) << "Failed to open " << path_to_check.value();
      exit(EXIT_FAILURE);
    }
  }
  LOG(INFO) << "Process " << pid_str << " still has all namespace entries";
}

void CheckOtherProcEntriesOrExit(const std::string& pid_str) {
  const base::FilePath proc("/proc");
  const base::FilePath proc_pid = proc.Append(pid_str);
  for (const char* entry : {"cwd", "root"}) {
    // Use open for the same reason as CheckNamespacesAvailableOrExit().
    const base::FilePath path_to_check = proc_pid.Append(entry);
    base::ScopedFD fd(open(path_to_check.value().c_str(), O_RDONLY));
    if (!fd.is_valid()) {
      PLOG(ERROR) << "Failed to open " << path_to_check.value();
      exit(EXIT_FAILURE);
    }
  }
  LOG(INFO) << "Process " << pid_str << " still has other proc entries";
}

// Creates subdirectories under dalvik-cache directory if not exists.
bool CreateArtContainerDataDirectory(
    const base::FilePath& art_dalvik_cache_directory) {
  for (const std::string& isa : ArtContainer::GetIsas()) {
    base::FilePath isa_directory = art_dalvik_cache_directory.Append(isa);
    // Use the same permissions as the ones used in maybeCreateDalvikCache() in
    // framework/base/cmds/app_process/app_main.cpp
    if (!InstallDirectory(0711, kRootUid, kRootGid, isa_directory)) {
      PLOG(ERROR) << "Failed to create art container data dir: "
                  << isa_directory.value();
      return false;
    }
  }
  return true;
}

// Stores relative path, mode_t for sdcard mounts.
struct EsdfsMount {
  const char* relative_path;
  mode_t mode;
  gid_t gid;
};

constexpr std::array<EsdfsMount, 3> kEsdfsMounts{{
    {"default/emulated", 0006, kSdcardRwGid},
    {"read/emulated", 0027, kEverybodyGid},
    {"write/emulated", 0007, kEverybodyGid},
}};

// Esdfs mount options:
// --------------------
// fsuid, fsgid  : Lower filesystem's uid/gid.
//
// derive_gid    : Changes uid/gid values on the lower filesystem for tracking
//                 storage user by apps and various categories.
//
// default_normal: Does not treat the default mount (using gid AID_SDCARD_RW)
//                 differently. Without this, the gid presented by the upper
//                 filesystem does not include the user, and would allow shell
//                 users to access all user’s data.
//
// mask          : Masks away permissions.
//
// gid           : Upper filesystem's group id.
//
// ns_fd         : Namespace file descriptor used to set the base namespace for
//                 the esdfs mount, similar to the  argument to setns(2).
//
// dl_uid, dl_gid: Downloads integration uid/gid.
//
// dl_loc        : The Android download directory acts as an overlay on dl_loc.

std::string CreateEsdfsMountOpts(uid_t fsuid,
                                 gid_t fsgid,
                                 mode_t mask,
                                 uid_t userid,
                                 gid_t gid,
                                 int container_userns_fd) {
  std::string opts = base::StringPrintf(
      "fsuid=%d,fsgid=%d,derive_gid,default_normal,mask=%d,multiuser,"
      "gid=%d,dl_loc=%s,dl_uid=%d,dl_gid=%d,ns_fd=%d",
      fsuid, fsgid, mask, gid, kHostDownloadsDirectory, kHostChronosUid,
      kHostChronosGid, container_userns_fd);
  LOG(INFO) << "Esdfs mount options: " << opts;
  return opts;
}

// Wait upto kInstalldTimeout for the sdcard source directory to be setup.
// On failure, exit.
bool WaitForSdcardSource(const base::FilePath& android_root) {
  bool ret;
  base::TimeDelta elapsed;
  // <android_root>/data path to synchronize with installd
  const base::FilePath fs_version = android_root.Append("data/.layout_version");

  LOG(INFO) << "Waiting upto " << kInstalldTimeout
            << " for installd to complete setting up /data.";
  ret = WaitForPaths({fs_version}, kInstalldTimeout, &elapsed);

  LOG(INFO) << "Waiting for installd took " << elapsed.InSeconds() << "s";
  if (!ret)
    LOG(ERROR) << "Timed out waiting for /data setup.";

  return ret;
}

// Don't use this, use GetOrCreateArcSalt instead. Reads the 16-byte per-machine
// random salt. The salt is created once when the machine is first used, and
// wiped/regenerated on powerwash/recovery. When it's not available yet (which
// could happen only on OOBE boot), returns an empty string.
// TODO(yusukes): Remove this function after M73.
std::string GetSystemSalt() {
  constexpr char kSaltFile[] = "/home/.shadow/salt";

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

// Reads a random number for the container from /var/lib/misc/arc_salt. If
// the file does not exist, generates a new one. This file will be cleared
// and regenerated after powerwash.
std::string GetOrCreateArcSalt() {
  constexpr char kArcSaltFile[] = "/var/lib/misc/arc_salt";
  constexpr mode_t kArcSaltFilePermissions = 0400;

  std::string arc_salt;
  const base::FilePath arc_salt_file(kArcSaltFile);
  if (!base::ReadFileToString(arc_salt_file, &arc_salt) ||
      arc_salt.size() != kSaltFileSize) {
    // If system salt value is available, reuse the system salt to avoid
    // clearing existing relocated boot*.art code.
    arc_salt = GetSystemSalt();
    if (arc_salt.size() != kSaltFileSize) {
      char rand_value[kSaltFileSize];
      crypto::RandBytes(rand_value, kSaltFileSize);
      arc_salt = std::string(rand_value, kSaltFileSize);
    }
    if (!brillo::WriteToFileAtomic(arc_salt_file, arc_salt.data(),
                                   arc_salt.size(), kArcSaltFilePermissions)) {
      LOG(ERROR) << "Failed to write arc salt file.";
      return std::string();
    }
  }
  return arc_salt;
}

}  // namespace

// A struct that holds all the FilePaths ArcSetup uses.
struct ArcPaths {
  static std::unique_ptr<ArcPaths> Create(Mode mode, const Config& config) {
    base::FilePath android_data;
    base::FilePath android_data_old;
    if (mode == Mode::BOOT_CONTINUE) {
      // session_manager must start arc-setup job with ANDROID_DATA_DIR
      // parameter containing the path of the real android-data directory. They
      // are passed only when the mode is boot-continue.
      android_data = base::FilePath(config.GetStringOrDie("ANDROID_DATA_DIR"));
      android_data_old =
          base::FilePath(config.GetStringOrDie("ANDROID_DATA_OLD_DIR"));
    }
    return base::WrapUnique(new ArcPaths(android_data, android_data_old));
  }

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
  const base::FilePath media_dest_default_directory{kMediaDestDefaultDirectory};
  const base::FilePath media_dest_read_directory{kMediaDestReadDirectory};
  const base::FilePath media_dest_write_directory{kMediaDestWriteDirectory};
  const base::FilePath media_mount_directory{kMediaMountDirectory};
  const base::FilePath media_profile_file{kMediaProfileFile};
  const base::FilePath obb_mount_directory{kObbMountDirectory};
  const base::FilePath obb_rootfs_directory{kObbRootfsDirectory};
  const base::FilePath oem_mount_directory{kOemMountDirectory};
  const base::FilePath platform_xml_file_relative{kPlatformXmlFileRelative};
  const base::FilePath sdcard_configfs_directory{kSdcardConfigfsDirectory};
  const base::FilePath sdcard_mount_directory{kSdcardMountDirectory};
  const base::FilePath sdcard_rootfs_directory{kSdcardRootfsDirectory};
  const base::FilePath shared_mount_directory{kSharedMountDirectory};
  const base::FilePath sysfs_cpu{kSysfsCpu};
  const base::FilePath sysfs_tracing{kSysfsTracing};
  const base::FilePath system_lib_arm_directory_relative{
      kSystemLibArmDirectoryRelative};
  const base::FilePath usb_devices_directory{kUsbDevicesDirectory};

  const base::FilePath restorecon_whitelist_sync{kRestoreconWhitelistSync};

  const base::FilePath android_data_directory;
  const base::FilePath android_data_old_directory;

 private:
  ArcPaths(const base::FilePath& android_data_directory,
           const base::FilePath& android_data_old_directory)
      : android_data_directory(android_data_directory),
        android_data_old_directory(android_data_old_directory) {}

  DISALLOW_COPY_AND_ASSIGN(ArcPaths);
};

ArcSetup::ArcSetup(Mode mode, const base::FilePath& config_json)
    : mode_(mode),
      config_(config_json, base::WrapUnique(base::Environment::Create())),
      arc_mounter_(GetDefaultMounter()),
      arc_paths_(ArcPaths::Create(mode_, config_)),
      arc_setup_metrics_(std::make_unique<ArcSetupMetrics>()) {}

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
       config_.GetBoolOrDie("NATIVE_BRIDGE_EXPERIMENT"));

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

  if (GetSdkVersion() >= AndroidSdkVersion::ANDROID_P)
    SetUpNetwork();
}

bool ArcSetup::SetUpPackagesCache() {
  base::ElapsedTimer timer;

  if (config_.GetBoolOrDie("SKIP_PACKAGES_CACHE_SETUP")) {
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

  // To support non-unibuild boards replace fingeprint in cache with current
  // system fingerprint.
  std::string content;
  std::string new_content;
  EXIT_IF(!base::ReadFileToString(source_cache, &content));
  SetFingerprintsForPackagesCache(
      content, GetSystemBuildProperyOrDie(kFingerprintProp), &new_content);

  EXIT_IF(!base::WriteFile(packages_cache, new_content.c_str(),
                           new_content.length()));
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

void ArcSetup::UnmountSdcard() {
  // We unmount here in both the ESDFS and the FUSE cases in order to
  // clean up after Android's /system/bin/sdcard. However, the paths
  // must be the same in both cases.
  for (auto mount : kEsdfsMounts) {
    base::FilePath kDestDirectory =
        arc_paths_->sdcard_mount_directory.Append(mount.relative_path);
    IGNORE_ERRORS(arc_mounter_->Umount(kDestDirectory));
  }

  LOG(INFO) << "Unmount sdcard complete.";
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
}

void ArcSetup::ApplyPerBoardConfigurations() {
  EXIT_IF(!MkdirRecursively(arc_paths_->oem_mount_directory.Append("etc")));

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
      EXIT_IF(
          !Chown(kHostArcCameraUid, kHostArcCameraGid, new_media_profile_xml));
    }
  }

  const base::FilePath hardware_features_xml("/etc/hardware_features.xml");
  if (!base::PathExists(hardware_features_xml))
    return;

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

void ArcSetup::SetUpSdcard() {
  constexpr unsigned int mount_flags =
      MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_NOATIME;
  const base::FilePath source_directory =
      arc_paths_->android_mutable_source.Append("data/media");

  const bool is_esdfs_supported = config_.GetBoolOrDie("USE_ESDFS");

  // Get the container's user namespace file descriptor.
  const int container_pid = config_.GetIntOrDie("CONTAINER_PID");
  base::ScopedFD container_userns_fd(HANDLE_EINTR(
      open(base::StringPrintf("/proc/%d/ns/user", container_pid).c_str(),
           O_RDONLY)));

  // SetUpSdcard can only be called from arc-sdcard is USE_ESDFS is enabled.
  CHECK(is_esdfs_supported);

  // Installd setups up the user data directory skeleton on first-time boot.
  // Wait for setup
  EXIT_IF(!WaitForSdcardSource(arc_paths_->android_mutable_source));

  for (auto mount : kEsdfsMounts) {
    base::FilePath dest_directory =
        arc_paths_->sdcard_mount_directory.Append(mount.relative_path);
    EXIT_IF(!arc_mounter_->Mount(
        source_directory.value(), dest_directory, "esdfs", mount_flags,
        CreateEsdfsMountOpts(kMediaUid, kMediaGid, mount.mode, kRootUid,
                             mount.gid, container_userns_fd.get())
            .c_str()));
  }

  LOG(INFO) << "Esdfs setup complete.";
}

void ArcSetup::SetUpSharedTmpfsForExternalStorage() {
  EXIT_IF(!arc_mounter_->UmountIfExists(arc_paths_->sdcard_mount_directory));
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
  EXIT_IF(!arc_mounter_->UmountIfExists(arc_paths_->obb_mount_directory));
  EXIT_IF(!MkdirRecursively(arc_paths_->obb_mount_directory));
  EXIT_IF(!arc_mounter_->Mount("tmpfs", arc_paths_->obb_mount_directory,
                               "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                               "mode=0755"));
  EXIT_IF(!arc_mounter_->SharedMount(arc_paths_->obb_mount_directory));
}

bool ArcSetup::GenerateHostSideCodeInternal(
    const base::FilePath& host_dalvik_cache_directory,
    ArcCodeRelocationResult* result) {
  *result = ArcCodeRelocationResult::ERROR_UNABLE_TO_RELOCATE;
  base::ElapsedTimer timer;
  std::unique_ptr<ArtContainer> art_container =
      ArtContainer::CreateContainer(arc_mounter_.get(), GetSdkVersion());
  if (!art_container) {
    LOG(ERROR) << "Failed to create art container";
    return false;
  }
  const std::string salt = GetOrCreateArcSalt();
  if (salt.empty()) {
    *result = ArcCodeRelocationResult::SALT_EMPTY;
    return false;
  }

  const uint64_t offset_seed = GetArtCompilationOffsetSeed(
      GetSystemBuildProperyOrDie(kFingerprintProp), salt);
  if (!art_container->PatchImage(offset_seed)) {
    LOG(ERROR) << "Failed to relocate boot images";
    return false;
  }
  *result = ArcCodeRelocationResult::SUCCESS;
  arc_setup_metrics_->SendCodeRelocationTime(timer.Elapsed());
  return true;
}

bool ArcSetup::GenerateHostSideCode(
    const base::FilePath& host_dalvik_cache_directory) {
  ArcCodeRelocationResult result;
  base::ElapsedTimer timer;
  if (!GenerateHostSideCodeInternal(host_dalvik_cache_directory, &result)) {
    // If anything fails, delete code in cache.
    LOG(INFO) << "Failed to generate host-side code. Deleting existing code in "
              << host_dalvik_cache_directory.value();
    DeleteFilesInDir(host_dalvik_cache_directory);
  }
  base::TimeDelta time_delta = timer.Elapsed();
  LOG(INFO) << "GenerateHostSideCode took "
            << time_delta.InMillisecondsRoundedUp() << "ms";
  arc_setup_metrics_->SendCodeRelocationResult(result);

  return result == ArcCodeRelocationResult::SUCCESS;
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
    if (IsDirectoryEmpty(src_isa_directory))
      continue;
    const std::string isa = src_isa_directory.BaseName().value();
    if (!InstallLinksToHostSideCodeInternal(src_isa_directory,
                                            dest_directory.Append(isa), isa)) {
      result = false;
      LOG(ERROR) << "InstallLinksToHostSideCodeInternal() for " << isa
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
  const int arc_lcd_density = config_.GetIntOrDie("ARC_LCD_DENSITY");
  LOG(INFO) << "lcd_density is " << arc_lcd_density;
  const int arc_file_picker = config_.GetIntOrDie("ARC_FILE_PICKER_EXPERIMENT");
  LOG(INFO) << "arc_file_picker is " << arc_file_picker;

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

  // Get the CLOCK_BOOTTIME offset and send it to the container as the at which
  // the container "booted". Given that there is no way to namespace time in
  // Linux, we need to communicate this in a userspace-only way.
  //
  // For the time being, the only component that uses this is bootstat. It uses
  // it to timeshift all readings from CLOCK_BOOTTIME and be able to more
  // accurately report the time against "Android boot".
  struct timespec ts;
  EXIT_IF(clock_gettime(CLOCK_BOOTTIME, &ts) != 0);

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
      "androidboot.disable_runas=%d "
      "androidboot.vm=%d "
      "androidboot.debuggable=%d "
      "androidboot.lcd_density=%d "
      "androidboot.container_ipv4_address=%s "
      "androidboot.gateway_ipv4_address=%s "
      "androidboot.native_bridge=%s "
      "androidboot.arc_file_picker=%d "
      "androidboot.chromeos_channel=%s "
      "androidboot.boottime_offset=%" PRId64 "\n" /* in nanoseconds */,
      is_dev_mode, !is_dev_mode, is_inside_vm, is_debuggable, arc_lcd_density,
      kArcContainerIPv4Address, kArcGatewayIPv4Address, native_bridge.c_str(),
      arc_file_picker, chromeos_channel.c_str(),
      ts.tv_sec * base::Time::kNanosecondsPerSecond + ts.tv_nsec);

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

  // debug/sync does not exist on all kernels
  EXIT_IF(!arc_mounter_->UmountIfExists(sync_mount_directory));

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

  EXIT_IF(!arc_mounter_->UmountIfExists(tracing_mount_directory));
  EXIT_IF(!InstallDirectory(0755, kHostRootUid, kHostRootGid,
                            tracing_mount_directory));

  if (!is_dev_mode)
    return;

  const base::FilePath tracing_directory("/sys/kernel/debug/tracing");
  EXIT_IF(!arc_mounter_->BindMount(tracing_directory, tracing_mount_directory));
}

void ArcSetup::MountDemoApps(const base::FilePath& demo_apps_image,
                             const base::FilePath& demo_apps_mount_directory) {
  // Verify that the demo apps image is under an imageloader mount point.
  EXIT_IF(demo_apps_image.ReferencesParent());
  EXIT_IF(!base::FilePath("/run/imageloader").IsParent(demo_apps_image));

  // Create the target mount point directory.
  EXIT_IF(!InstallDirectory(0700, kHostRootUid, kHostRootGid,
                            demo_apps_mount_directory));

  // imageloader securely verifies images before mounting them, so we can trust
  // the provided image and can mount it without MS_NOEXEC.
  EXIT_IF(!arc_mounter_->LoopMount(demo_apps_image.value(),
                                   demo_apps_mount_directory,
                                   MS_RDONLY | MS_NODEV));
}

void ArcSetup::SetUpMountPointForRemovableMedia() {
  EXIT_IF(!arc_mounter_->UmountIfExists(arc_paths_->media_mount_directory));
  EXIT_IF(!InstallDirectory(0755, kRootUid, kSystemGid,
                            arc_paths_->media_mount_directory));

  const std::string media_mount_options =
      base::StringPrintf("mode=0755,uid=%u,gid=%u", kRootUid, kSystemGid);
  EXIT_IF(!arc_mounter_->Mount("tmpfs", arc_paths_->media_mount_directory,
                               "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                               media_mount_options.c_str()));
  EXIT_IF(!arc_mounter_->SharedMount(arc_paths_->media_mount_directory));
  for (auto* directory : {"removable", "removable-default", "removable-read",
                          "removable-write"}) {
    EXIT_IF(
        !InstallDirectory(0755, kMediaUid, kMediaGid,
                          arc_paths_->media_mount_directory.Append(directory)));
  }
}

void ArcSetup::SetUpMountPointForAdbd() {
  EXIT_IF(!arc_mounter_->UmountIfExists(arc_paths_->adbd_mount_directory));
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
  EXIT_IF(!arc_mounter_->UmountIfExists(arc_paths_->media_dest_directory));
  EXIT_IF(
      !arc_mounter_->UmountIfExists(arc_paths_->media_dest_default_directory));
  EXIT_IF(!arc_mounter_->UmountIfExists(arc_paths_->media_dest_read_directory));
  EXIT_IF(
      !arc_mounter_->UmountIfExists(arc_paths_->media_dest_write_directory));
}

void ArcSetup::SetUpSharedMountPoints() {
  EXIT_IF(!arc_mounter_->UmountIfExists(arc_paths_->shared_mount_directory));
  EXIT_IF(!InstallDirectory(0755, kRootUid, kRootGid,
                            arc_paths_->shared_mount_directory));
  // Use 0755 to make sure only the real root user can write to the shared
  // mount point.
  EXIT_IF(!arc_mounter_->Mount("tmpfs", arc_paths_->shared_mount_directory,
                               "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                               "mode=0755"));
  EXIT_IF(!arc_mounter_->SharedMount(arc_paths_->shared_mount_directory));
}

void ArcSetup::SetUpOwnershipForSdcardConfigfs() {
  // Make sure <configfs>/sdcardfs/ and <configfs>/sdcardfs/extensions} are
  // owned by android-root.
  const base::FilePath extensions_dir =
      arc_paths_->sdcard_configfs_directory.Append("extensions");
  if (base::PathExists(extensions_dir)) {
    EXIT_IF(!Chown(kRootUid, kRootGid, arc_paths_->sdcard_configfs_directory));
    EXIT_IF(!Chown(kRootUid, kRootGid, extensions_dir));
  }
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
  const std::vector<std::string> attrs{
      "uevent",           "config",           "vendor", "device",
      "subsystem_vendor", "subsystem_device", "drm"};

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

void ArcSetup::SetUpPowerSysfsContext() {
  const base::FilePath sysfs_power_supply_path("/sys/class/power_supply");
  const std::string sysfs_batteryinfo_context(
      "u:object_r:sysfs_batteryinfo:s0");

  base::FileEnumerator power_supply_dir(
      sysfs_power_supply_path, false /* recursive */,
      base::FileEnumerator::FileType::DIRECTORIES);

  for (auto power_supply = power_supply_dir.Next(); !power_supply.empty();
       power_supply = power_supply_dir.Next()) {
    base::FileEnumerator power_supply_attrs(
        power_supply, false /* recursive */,
        base::FileEnumerator::FileType::FILES);

    for (auto attr = power_supply_attrs.Next(); !attr.empty();
         attr = power_supply_attrs.Next()) {
      EXIT_IF(!Chcon(sysfs_batteryinfo_context, Realpath(attr)));
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

  std::string ip_addr(kArcContainerIPv4Address);
  std::string gateway(kArcGatewayIPv4Address);

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

void ArcSetup::SetUpDefaultApps() {
  // This sets up default apps customization for the current board. Unibuild
  // may contain default apps the for particular board only. Default apps that
  // are shared for all boards of the same image exist in
  // /usr/share/google-chrome/extensions/arc
  // If customization exists it is located in
  // /usr/share/google-chrome/extensions/arc/BOARD_NAME.
  // Last folder is mapped using symbolic link to /var/cache/arc_default_apps.

  constexpr char kProductBoardProp[] = "ro.product.board";
  const std::string board = GetSystemBuildProperyOrDie(kProductBoardProp);

  const base::FilePath default_apps_root =
      base::FilePath(kDefaultAppsDirectory);
  const base::FilePath default_apps_board = default_apps_root.Append(board);
  if (!base::PathExists(default_apps_board)) {
    LOG(INFO) << "Board default app customization does not exist: "
              << default_apps_board.value();
    return;
  }

  // The DeleteFile call is to make sure that the link is created even if
  // |link_to_default_apps_board| exists as a file.
  const base::FilePath link_to_default_apps_board =
      base::FilePath(kDefaultAppsBoardDirectory);
  IGNORE_ERRORS(
      base::DeleteFile(link_to_default_apps_board, false /* recursive */));
  EXIT_IF(!base::CreateSymbolicLink(default_apps_board,
                                    link_to_default_apps_board));
  LOG(INFO) << "Board default app customization created: "
            << default_apps_board.value() << " -> "
            << link_to_default_apps_board.value();
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

AndroidSdkVersion ArcSetup::SdkVersionFromString(
    const std::string& version_str) {
  int version;
  if (base::StringToInt(version_str, &version)) {
    switch (version) {
      case 23:
        return AndroidSdkVersion::ANDROID_M;
      case 25:
        return AndroidSdkVersion::ANDROID_N_MR1;
      case 28: {
        // TODO(yusukes): Remove the hack once master-arc-dev starts using >28.
        const std::string version_release_str =
            GetSystemBuildProperyOrDie("ro.build.version.release");
        LOG(INFO) << "Release version string: " << version_release_str;
        return version_release_str == "Q" ? AndroidSdkVersion::ANDROID_Q
                                          : AndroidSdkVersion::ANDROID_P;
      }
    }
  }

  LOG(ERROR) << "Unknown SDK version : " << version_str;
  return AndroidSdkVersion::UNKNOWN;
}

AndroidSdkVersion ArcSetup::GetSdkVersion() {
  const std::string version_str =
      GetSystemBuildProperyOrDie("ro.build.version.sdk");
  LOG(INFO) << "SDK version string: " << version_str;

  const AndroidSdkVersion version = SdkVersionFromString(version_str);
  if (version == AndroidSdkVersion::UNKNOWN)
    LOG(FATAL) << "Unknown SDK version: " << version_str;
  return version;
}

void ArcSetup::UnmountOnStop() {
  // This function is for Mode::STOP. Use IGNORE_ERRORS to make sure to run all
  // clean up code.
  IGNORE_ERRORS(arc_mounter_->UmountIfExists(
      arc_paths_->shared_mount_directory.Append("cache")));
  IGNORE_ERRORS(arc_mounter_->UmountIfExists(
      arc_paths_->shared_mount_directory.Append("data")));
  IGNORE_ERRORS(arc_mounter_->LoopUmountIfExists(
      arc_paths_->shared_mount_directory.Append("demo_apps")));
  IGNORE_ERRORS(arc_mounter_->UmountIfExists(arc_paths_->adbd_mount_directory));
  IGNORE_ERRORS(arc_mounter_->UmountIfExists(arc_paths_->media_dest_directory));
  IGNORE_ERRORS(
      arc_mounter_->UmountIfExists(arc_paths_->media_dest_default_directory));
  IGNORE_ERRORS(
      arc_mounter_->UmountIfExists(arc_paths_->media_dest_read_directory));
  IGNORE_ERRORS(
      arc_mounter_->UmountIfExists(arc_paths_->media_dest_write_directory));
  IGNORE_ERRORS(
      arc_mounter_->UmountIfExists(arc_paths_->media_mount_directory));
  IGNORE_ERRORS(arc_mounter_->Umount(arc_paths_->sdcard_mount_directory));
  IGNORE_ERRORS(
      arc_mounter_->UmountIfExists(arc_paths_->shared_mount_directory));
  IGNORE_ERRORS(arc_mounter_->Umount(arc_paths_->obb_mount_directory));
  IGNORE_ERRORS(arc_mounter_->Umount(arc_paths_->oem_mount_directory));
  IGNORE_ERRORS(
      arc_mounter_->UmountIfExists(arc_paths_->android_mutable_source));
  IGNORE_ERRORS(arc_mounter_->UmountIfExists(
      arc_paths_->debugfs_directory.Append("sync")));
  IGNORE_ERRORS(arc_mounter_->UmountIfExists(
      arc_paths_->debugfs_directory.Append("tracing")));
  // Clean up in case this was not unmounted.
  IGNORE_ERRORS(
      arc_mounter_->UmountIfExists(arc_paths_->binfmt_misc_directory));
  IGNORE_ERRORS(
      arc_mounter_->UmountIfExists(arc_paths_->android_rootfs_directory.Append(
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

void ArcSetup::GetBootTypeAndDataSdkVersion(
    ArcBootType* out_boot_type, AndroidSdkVersion* out_data_sdk_version) {
  const std::string system_fingerprint =
      GetSystemBuildProperyOrDie(kFingerprintProp);

  // Note: The XML file name has to match com.android.server.pm.Settings's
  // mSettingsFilename. This will be very unlikely to change, but we still need
  // to be careful.
  const base::FilePath packages_xml =
      arc_paths_->android_data_directory.Append("data/system/packages.xml");

  if (!base::PathExists(packages_xml)) {
    // This path is taken when /data is empty, which is not an error.
    LOG(INFO) << packages_xml.value()
              << " does not exist. This is the very first boot aka opt-in.";
    *out_boot_type = ArcBootType::FIRST_BOOT;
    *out_data_sdk_version = AndroidSdkVersion::UNKNOWN;
    return;
  }

  // Get a fingerprint from /data/system/packages.xml.
  std::string data_fingerprint;
  std::string data_sdk_version;
  if (!GetFingerprintAndSdkVersionFromPackagesXml(
          packages_xml, &data_fingerprint, &data_sdk_version)) {
    LOG(ERROR) << "Failed to get a fingerprint from " << packages_xml.value();
    // Return kFirstBootAfterUpdate so the caller invalidates art/oat files
    // which is safer than returning kRegularBoot.
    *out_boot_type = ArcBootType::FIRST_BOOT_AFTER_UPDATE;
    *out_data_sdk_version = AndroidSdkVersion::UNKNOWN;
    return;
  }

  // If two fingerprints don't match, this is the first boot after OTA.
  // Android's PackageManagerService.isUpgrade(), at least on M, N, and
  // O releases, does exactly the same to detect OTA.
  const bool ota_detected = system_fingerprint != data_fingerprint;
  if (!ota_detected) {
    LOG(INFO) << "This is regular boot.";
  } else {
    LOG(INFO) << "This is the first boot after OTA: system="
              << system_fingerprint << ", data=" << data_fingerprint;
  }
  LOG(INFO) << "Data SDK version: " << data_sdk_version;
  LOG(INFO) << "System SDK version: " << static_cast<int>(GetSdkVersion());
  *out_boot_type = ota_detected ? ArcBootType::FIRST_BOOT_AFTER_UPDATE
                                : ArcBootType::REGULAR_BOOT;
  *out_data_sdk_version = SdkVersionFromString(data_sdk_version);
}

void ArcSetup::ShouldDeleteDataExecutables(
    ArcBootType boot_type,
    bool* out_should_delete_data_dalvik_cache_directory,
    bool* out_should_delete_data_app_executables) {
  if (boot_type == ArcBootType::FIRST_BOOT_AFTER_UPDATE) {
    // Delete /data/dalvik-cache and /data/app/<package_name>/oat before the
    // container starts since this is the first boot after OTA.
    *out_should_delete_data_dalvik_cache_directory = true;
    *out_should_delete_data_app_executables = true;
    return;
  }
  // Otherwise, clear neither /data/dalvik-cache nor /data/app/*/oat.
  *out_should_delete_data_dalvik_cache_directory = false;
  *out_should_delete_data_app_executables = false;
}

std::string ArcSetup::GetSerialNumber() {
  const std::string chromeos_user = config_.GetStringOrDie("CHROMEOS_USER");
  const std::string salt = GetOrCreateArcSalt();
  EXIT_IF(salt.empty());  // at this point, the salt file should always exist.
  return arc::GenerateFakeSerialNumber(chromeos_user, salt);
}

void ArcSetup::DeleteUnusedCacheDirectory() {
  if (GetSdkVersion() == AndroidSdkVersion::ANDROID_M ||
      GetSdkVersion() == AndroidSdkVersion::ANDROID_N_MR1) {
    return;
  }
  // /home/.../android-data/cache is bind-mounted to /cache on N in
  // MountSharedAndroidDirectories, but it is no longer bind-mounted on P.
  EXIT_IF(
      !MoveDirIntoDataOldDir(arc_paths_->android_data_directory.Append("cache"),
                             arc_paths_->android_data_old_directory));
}

void ArcSetup::MountSharedAndroidDirectories() {
  const base::FilePath cache_directory =
      arc_paths_->android_data_directory.Append("cache");
  const base::FilePath data_directory =
      arc_paths_->android_data_directory.Append("data");

  const base::FilePath shared_cache_directory =
      arc_paths_->shared_mount_directory.Append("cache");
  const base::FilePath shared_data_directory =
      arc_paths_->shared_mount_directory.Append("data");

  if (!base::PathExists(shared_data_directory)) {
    EXIT_IF(!InstallDirectory(0700, kHostRootUid, kHostRootGid,
                              shared_data_directory));
  }

  if (GetSdkVersion() == AndroidSdkVersion::ANDROID_N_MR1 &&
      !base::PathExists(shared_cache_directory)) {
    EXIT_IF(!InstallDirectory(0700, kHostRootUid, kHostRootGid,
                              shared_cache_directory));
  }

  // First, make the original data directory a mount point and also make it
  // executable. This has to be done *before* passing the directory into
  // the shared mount point because the new flags won't be propagated if the
  // mount point has already been shared with the slave.
  EXIT_IF(!arc_mounter_->BindMount(data_directory, data_directory));
  EXIT_IF(
      !arc_mounter_->Remount(data_directory, MS_NOSUID | MS_NODEV, "seclabel"));

  // Then, bind-mount /cache to the shared mount point on N.
  if (GetSdkVersion() == AndroidSdkVersion::ANDROID_N_MR1)
    EXIT_IF(!arc_mounter_->BindMount(cache_directory, shared_cache_directory));

  // Finally, bind-mount /data to the shared mount point.
  EXIT_IF(!arc_mounter_->Mount(data_directory.value(), shared_data_directory,
                               nullptr, MS_BIND, nullptr));

  const std::string demo_session_apps =
      config_.GetStringOrDie("DEMO_SESSION_APPS_PATH");
  if (!demo_session_apps.empty()) {
    MountDemoApps(base::FilePath(demo_session_apps),
                  arc_paths_->shared_mount_directory.Append("demo_apps"));
  }
}

void ArcSetup::UnmountSharedAndroidDirectories() {
  const base::FilePath data_directory =
      arc_paths_->android_data_directory.Append("data");
  const base::FilePath shared_cache_directory =
      arc_paths_->shared_mount_directory.Append("cache");
  const base::FilePath shared_data_directory =
      arc_paths_->shared_mount_directory.Append("data");
  const base::FilePath shared_demo_apps_directory =
      arc_paths_->shared_mount_directory.Append("demo_apps");

  IGNORE_ERRORS(arc_mounter_->Umount(data_directory));
  IGNORE_ERRORS(arc_mounter_->UmountIfExists(shared_cache_directory));
  IGNORE_ERRORS(arc_mounter_->Umount(shared_data_directory));
  IGNORE_ERRORS(arc_mounter_->LoopUmountIfExists(shared_demo_apps_directory));
  IGNORE_ERRORS(arc_mounter_->Umount(arc_paths_->shared_mount_directory));
}

void ArcSetup::MaybeStartAdbdProxy(bool is_dev_mode,
                                   bool is_inside_vm,
                                   const std::string& serialnumber) {
  if (!is_dev_mode || is_inside_vm)
    return;
  const base::FilePath adbd_config_path("/etc/arc/adbd.json");
  if (!base::PathExists(adbd_config_path))
    return;
  // Poll the firmware to determine whether UDC is enabled or not. We're only
  // stopping the process if it's explicitly disabled because some systems (like
  // ARM) do not have this signal wired in and just rely on the presence of
  // adbd.json.
  if (LaunchAndWait({"/usr/bin/crossystem", "dev_enable_udc?0"}))
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

  const bool mount_demo_apps =
      !config_.GetStringOrDie("DEMO_SESSION_APPS_PATH").empty();

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
  const std::string pid_str = config_.GetStringOrDie("CONTAINER_PID");
  const std::vector<std::string> command_line = {
      "/usr/bin/nsenter", "-t", pid_str,
      "-m",  // enter mount namespace
      "-U",  // enter user namespace
      "-i",  // enter System V IPC namespace
      "-n",  // enter network namespace
      "-p",  // enter pid namespace
      "-r",  // set the root directory
      "-w",  // set the working directory
      "--", kCommand, "--serialno", serialnumber, "--disable-boot-completed",
      config_.GetStringOrDie("DISABLE_BOOT_COMPLETED_BROADCAST"),
      "--vendor-privileged", config_.GetStringOrDie("ENABLE_VENDOR_PRIVILEGED"),
      "--container-boot-type", std::to_string(static_cast<int>(boot_type)),
      // When this COPY_PACKAGES_CACHE is set to "1", SystemServer copies
      // /data/system/pacakges.xml to /data/system/pacakges_copy.xml after the
      // initialization stage of PackageManagerService.
      "--copy-packages-cache", config_.GetStringOrDie("COPY_PACKAGES_CACHE"),
      "--mount-demo-apps", mount_demo_apps ? "1" : "0", "--is-demo-session",
      config_.GetStringOrDie("IS_DEMO_SESSION"), "--locale",
      config_.GetStringOrDie("LOCALE"), "--preferred-languages",
      config_.GetStringOrDie("PREFERRED_LANGUAGES"),
      // Whether ARC should transition the supervision setup
      //   "0": No transition necessary.
      //   "1": Child -> regular transition, should disable supervision.
      //   "2": Regular -> child transition, should enable supervision.
      "--supervision-transition",
      config_.GetStringOrDie("SUPERVISION_TRANSITION")};

  base::ElapsedTimer timer;
  if (!LaunchAndWait(command_line)) {
    auto elapsed = timer.Elapsed().InMillisecondsRoundedUp();
    // ContinueContainerBoot() failed. Try to find out why it failed and log
    // messages accordingly. If one of these functions calls exit(), it means
    // that '/usr/bin/nsenter' is very likely the command that failed (rather
    // than '/system/bin/arcbootcontinue'.)
    CheckProcessIsAliveOrExit(pid_str);
    CheckNamespacesAvailableOrExit(pid_str);
    CheckOtherProcEntriesOrExit(pid_str);

    // Either nsenter or arcbootcontinue failed, but we don't know which. For
    // example, arcbootcontinue may fail if it tries to set a property while
    // init is being shut down or crashing.
    LOG(ERROR) << kCommand << " failed for unknown reason after " << elapsed
               << "ms";
    exit(EXIT_FAILURE);
  }
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
  const bool is_writable = config_.GetBoolOrDie("WRITABLE_MOUNT");
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
      kSdcardRootfsImage, arc_paths_->sdcard_rootfs_directory, kBaseFlags));
  EXIT_IF(!arc_mounter_->LoopMount(
      kObbRootfsImage, arc_paths_->obb_rootfs_directory, kBaseFlags));
}

void ArcSetup::UnmountOnOnetimeStop() {
  IGNORE_ERRORS(arc_mounter_->LoopUmount(arc_paths_->obb_rootfs_directory));
  IGNORE_ERRORS(arc_mounter_->LoopUmount(arc_paths_->sdcard_rootfs_directory));
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
        "oem/etc",
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
  EXIT_IF(!CreateOrTruncate(coldboot_done, 0755));
  EXIT_IF(!Chown(kRootUid, kRootGid, coldboot_done));
}

void ArcSetup::OnSetup() {
  const bool is_dev_mode = config_.GetBoolOrDie("CHROMEOS_DEV_MODE");
  const bool is_inside_vm = config_.GetBoolOrDie("CHROMEOS_INSIDE_VM");
  const bool is_debuggable = config_.GetBoolOrDie("ANDROID_DEBUGGABLE");

  // The host-side dalvik-cache directory is mounted into the container
  // via the json file. Create it regardless of whether the code integrity
  // feature is enabled.
  EXIT_IF(
      !CreateArtContainerDataDirectory(arc_paths_->art_dalvik_cache_directory));

  // Mount host-compiled and host-verified .art and .oat files. The container
  // will see these files, but other than that, the /data and /cache
  // directories are empty and read-only which is the best for security.

  // Unconditionally generate host-side code here.
  if (GetSdkVersion() <= AndroidSdkVersion::ANDROID_P) {
    base::ElapsedTimer timer;
    EXIT_IF(!GenerateHostSideCode(arc_paths_->art_dalvik_cache_directory));

    // For now, integrity checking time is the time needed to relocate
    // boot*.art files because of b/67912719. Once TPM is enabled, this will
    // report the total time spend on code verification + [relocation + sign]
    arc_setup_metrics_->SendCodeIntegrityCheckingTotalTime(timer.Elapsed());
  }

  // Make sure directories for all ISA are there just to make config.json happy.
  for (const auto* isa : {"arm", "x86", "x86_64"}) {
    EXIT_IF(
        !MkdirRecursively(arc_paths_->art_dalvik_cache_directory.Append(isa)));
  }

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
  if (GetSdkVersion() >= AndroidSdkVersion::ANDROID_P)
    SetUpPowerSysfsContext();
  MakeMountPointsReadOnly();
  SetUpCameraProperty();
  SetUpSharedApkDirectory();

  // These should be the last thing OnSetup() does because the job and
  // directories are not needed for arc-setup. Only the container's startup code
  // (in session_manager side) requires the job and directories.
  WaitForRtLimitsJob();
}

void ArcSetup::OnBootContinue() {
  const bool is_dev_mode = config_.GetBoolOrDie("CHROMEOS_DEV_MODE");
  const bool is_inside_vm = config_.GetBoolOrDie("CHROMEOS_INSIDE_VM");
  const std::string serialnumber = GetSerialNumber();

  ArcBootType boot_type;
  AndroidSdkVersion data_sdk_version;
  GetBootTypeAndDataSdkVersion(&boot_type, &data_sdk_version);

  arc_setup_metrics_->SendSdkVersionUpgradeType(
      GetUpgradeType(GetSdkVersion(), data_sdk_version));

  if (ShouldDeleteAndroidData(GetSdkVersion(), data_sdk_version)) {
    EXIT_IF(!MoveDirIntoDataOldDir(arc_paths_->android_data_directory,
                                   arc_paths_->android_data_old_directory));
  }

  bool should_delete_data_dalvik_cache_directory;
  bool should_delete_data_app_executables;
  ShouldDeleteDataExecutables(boot_type,
                              &should_delete_data_dalvik_cache_directory,
                              &should_delete_data_app_executables);
  DeleteExecutableFilesInData(should_delete_data_dalvik_cache_directory,
                              should_delete_data_app_executables);

  // The socket isn't created when the mini-container is started, so the
  // arc-setup --mode=pre-chroot call won't label it. Label it here instead.
  EXIT_IF(!Chcon(kArcBridgeSocketContext, arc_paths_->arc_bridge_socket_path));

  // Set up |android_mutable_source|. Although the container does not use
  // the directory directly, we should still set up the directory so that
  // session_manager can delete (to be more precise, move) the directory
  // on opt-out. Since this creates cache and data directories when they
  // don't exist, this has to be done before calling ShareAndroidData().
  SetUpAndroidData();

  if (GetSdkVersion() <= AndroidSdkVersion::ANDROID_P) {
    if (!InstallLinksToHostSideCode()) {
      arc_setup_metrics_->SendBootContinueCodeInstallationResult(
          ArcBootContinueCodeInstallationResult::
              ERROR_CANNOT_INSTALL_HOST_CODE);
    } else {
      arc_setup_metrics_->SendBootContinueCodeInstallationResult(
          ArcBootContinueCodeInstallationResult::SUCCESS);
    }
  }

  // Set up /run/arc/shared_mounts/{cache,data,demo_apps} to expose the user's
  // data to the container. Demo apps are setup only for demo sessions.
  MountSharedAndroidDirectories();

  MaybeStartUreadaheadInTracingMode();
  MaybeStartAdbdProxy(is_dev_mode, is_inside_vm, serialnumber);

  // Asks the container to continue boot.
  ContinueContainerBoot(boot_type, serialnumber);

  // Unmount /run/arc/shared_mounts and its children. They are unnecessary at
  // this point.
  UnmountSharedAndroidDirectories();

  DeleteUnusedCacheDirectory();

  const std::string env_to_pass = base::StringPrintf(
      "CONTAINER_PID=%d", config_.GetIntOrDie("CONTAINER_PID"));
  EXIT_IF(!LaunchAndWait(
      {"/sbin/initctl", "start", "--no-wait", "arc-sdcard", env_to_pass}));
}

void ArcSetup::OnStop() {
  CleanUpBinFmtMiscSetUp();
  UnmountOnStop();
  RemoveBugreportPipe();
  RemoveAndroidKmsgFifo();
}

void ArcSetup::OnOnetimeSetup() {
  EnsureContainerDirectories();
  MountOnOnetimeSetup();

  // Setup ownership for <configfs>/sdcard, if the directory exists.
  SetUpOwnershipForSdcardConfigfs();

  // Build properties are needed to finish booting the container, so we need
  // to set them up here instead of in the per-board setup.
  CreateBuildProperties();

  // Setup per-board default apps. This has to be called after
  // CreateBuildProperties because CreateBuildProperties sets the name of board.
  SetUpDefaultApps();
}

void ArcSetup::OnOnetimeStop() {
  UnmountOnOnetimeStop();
}

void ArcSetup::OnPreChroot() {
  // Note: Do not try to create a directory in tmpfs here. Recent (4.8+)
  // kernel doesn't allow us to do so and returns EOVERFLOW. b/78262683

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
  EmulateArcUreadahead(arc_paths_->android_rootfs_directory, kReadAheadTimeout,
                       GetSdkVersion());
}

void ArcSetup::OnRemoveData() {
  const std::string chromeos_user = config_.GetStringOrDie("CHROMEOS_USER");
  const base::FilePath root_path =
      brillo::cryptohome::home::GetRootPath(chromeos_user);
  // Ensure the user directory exists.
  EXIT_IF(!base::DirectoryExists(root_path));

  const base::FilePath android_data = root_path.Append("android-data");
  const base::FilePath android_data_old = root_path.Append("android-data-old");

  EXIT_IF(!MoveDirIntoDataOldDir(android_data, android_data_old));
}

void ArcSetup::OnMountSdcard() {
  // Set up sdcard asynchronously from arc-sdcard so that waiting on installd
  // does not add latency to boot-continue (and result in session-manager
  // related timeouts).
  SetUpSdcard();
}

void ArcSetup::OnUnmountSdcard() {
  UnmountSdcard();
}

void ArcSetup::OnUpdateRestoreconLast() {
  // On Android, /init writes the security.restorecon_last attribute to /data
  // (and /cache on N) after it finishes updating labels of the files in the
  // directories, but on ARC, writing the attribute fails silently because
  // processes in user namespace are not allowed to write arbitrary entries
  // under security.* even with CAP_SYS_ADMIN. (b/33084415, b/33402785)
  // As a workaround, let this command outside the container set the
  // attribute for ARC.
  constexpr char kRestoreconLastXattr[] = "security.restorecon_last";
  std::vector<base::FilePath> context_files;
  std::vector<base::FilePath> target_directories = {
      arc_paths_->android_mutable_source.Append("data")};

  switch (GetSdkVersion()) {
    case AndroidSdkVersion::ANDROID_N_MR1:
      context_files.push_back(
          arc_paths_->android_rootfs_directory.Append("file_contexts.bin"));
      // Unlike P, N uses a dedicated partition for /cache.
      target_directories.push_back(
          arc_paths_->android_mutable_source.Append("cache"));
      break;
    case AndroidSdkVersion::ANDROID_P:
    case AndroidSdkVersion::ANDROID_Q:
      // The order of files to read is important. Do not reorder.
      context_files.push_back(
          arc_paths_->android_rootfs_directory.Append("plat_file_contexts"));
      context_files.push_back(
          arc_paths_->android_rootfs_directory.Append("vendor_file_contexts"));
      break;
    case AndroidSdkVersion::ANDROID_M:
    case AndroidSdkVersion::UNKNOWN:
      NOTREACHED();
  }

  std::string hash;
  EXIT_IF(!GetSha1HashOfFiles(context_files, &hash));
  for (const auto& target : target_directories) {
    EXIT_IF(!SetXattr(target, kRestoreconLastXattr, hash));
  }
}

std::string ArcSetup::GetSystemBuildProperyOrDie(const std::string& name) {
  if (system_properties_.empty()) {
    const base::FilePath build_prop =
        arc_paths_->android_generated_properties_directory.Append("build.prop");
    EXIT_IF(!GetPropertiesFromFile(build_prop, &system_properties_));
  }
  DCHECK(!system_properties_.empty());
  const auto it = system_properties_.find(name);
  CHECK(system_properties_.end() != it) << "Failed to read property: " << name;
  CHECK(!it->second.empty());
  return it->second;
}

void ArcSetup::Run() {
  switch (mode_) {
    case Mode::SETUP:
      bootstat_log("mini-android-start");
      OnSetup();
      bootstat_log("arc-setup-for-mini-android-end");
      break;
    case Mode::STOP:
      OnStop();
      break;
    case Mode::BOOT_CONTINUE:
      bootstat_log("android-start");
      OnBootContinue();
      bootstat_log("arc-setup-end");
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
    case Mode::REMOVE_DATA:
      OnRemoveData();
      break;
    case Mode::MOUNT_SDCARD:
      OnMountSdcard();
      break;
    case Mode::UNMOUNT_SDCARD:
      OnUnmountSdcard();
      break;
    case Mode::UPDATE_RESTORECON_LAST:
      OnUpdateRestoreconLast();
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
