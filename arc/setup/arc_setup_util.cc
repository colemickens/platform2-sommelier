// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc_setup_util.h"  // NOLINT - TODO(b/32971714): fix it properly.

#include <fcntl.h>
#include <limits.h>
#include <linux/loop.h>
#include <linux/major.h>
#include <mntent.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <selinux/restorecon.h>
#include <selinux/selinux.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <fstream>
#include <list>
#include <set>
#include <utility>

#include <base/bind.h>
#include <base/environment.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/process/launch.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <base/timer/elapsed_timer.h>
#include <base/values.h>
#include <crypto/sha2.h>

namespace arc {

namespace {

std::string GetLoopDevice(int32_t device) {
  return base::StringPrintf("/dev/loop%d", device);
}

// A callback function for SELinux restorecon.
PRINTF_FORMAT(2, 3)
int RestoreConLogCallback(int type, const char* fmt, ...) {
  va_list ap;

  std::string message = "restorecon: ";
  va_start(ap, fmt);
  message += base::StringPrintV(fmt, ap);
  va_end(ap);

  // This already has a line feed at the end, so trim it off to avoid
  // empty lines in the log.
  base::TrimString(message, "\r\n", &message);

  if (type == SELINUX_INFO)
    LOG(INFO) << message;
  else
    LOG(ERROR) << message;

  return 0;
}

bool RestoreconInternal(const std::vector<base::FilePath>& paths,
                        bool is_recursive) {
  union selinux_callback cb;
  cb.func_log = RestoreConLogCallback;
  selinux_set_callback(SELINUX_CB_LOG, cb);

  const unsigned int restorecon_flags =
      (is_recursive ? SELINUX_RESTORECON_RECURSE : 0) |
      SELINUX_RESTORECON_REALPATH;

  bool success = true;
  for (const auto& path : paths) {
    if (selinux_restorecon(path.value().c_str(), restorecon_flags) != 0) {
      LOG(ERROR) << "Error in restorecon of " << path.value();
      success = false;
    }
  }
  return success;
}

// A callback function for GetPropertyFromFile.
bool FindProperty(const std::string& line_prefix_to_find,
                  const std::string& line,
                  std::string* out_prop) {
  if (base::StartsWith(line, line_prefix_to_find,
                       base::CompareCase::SENSITIVE)) {
    *out_prop = line.substr(line_prefix_to_find.length());
    return true;
  }
  return false;
}

// A callback function for GetFingerprintFromPackagesXml. This checks if the
// |line| is like
//    <version sdkVersion="25" databaseVersion="3" fingerprint="..." />
// and store the fingerprint part in |out_fingerprint| if it is. Ignore a line
// with a volumeUuid attribute which means that the line is for an external
// storage. What we need is a fingerprint for an internal storage.
bool FindFingerprint(const std::string& line, std::string* out_fingerprint) {
  constexpr char kElementVersion[] = "<version ";
  constexpr char kAttributeVolumeUuid[] = " volumeUuid=\"";
  constexpr char kAttributeSdkVersion[] = " sdkVersion=\"";
  constexpr char kAttributeDatabaseVersion[] = " databaseVersion=\"";
  constexpr char kAttributeFingerprint[] = " fingerprint=\"";

  // Parsing an XML this way is not very clean but in this case, it works (and
  // fast.) Android's packages.xml is written in com.android.server.pm.Settings'
  // writeLPr(), and the write function always uses Android's FastXmlSerializer.
  // The serializer does not try to pretty-print the XML, and inserts '\n' only
  // to certain places like endTag.
  // TODO(yusukes): Check Android P's writeLPr() and FastXmlSerializer although
  // they unlikely change.
  std::string trimmed;
  base::TrimWhitespaceASCII(line, base::TRIM_ALL, &trimmed);
  if (!base::StartsWith(trimmed, kElementVersion, base::CompareCase::SENSITIVE))
    return false;  // Not a <version> element. Ignoring.

  if (trimmed.find(kAttributeVolumeUuid) != std::string::npos)
    return false;  // This is for an external storage. Ignoring.

  std::string::size_type pos = trimmed.find(kAttributeFingerprint);
  if (pos == std::string::npos ||
      // Do some more sanity checks.
      trimmed.find(kAttributeSdkVersion) == std::string::npos ||
      trimmed.find(kAttributeDatabaseVersion) == std::string::npos) {
    LOG(WARNING) << "Unexpected <version> format: " << trimmed;
    return false;
  }

  // Found the line we need.
  std::string fingerprint = trimmed.substr(pos + strlen(kAttributeFingerprint));
  pos = fingerprint.find('"');
  if (pos == std::string::npos) {
    LOG(WARNING) << "<version> doesn't have a valid fingerprint: " << trimmed;
    return false;
  }

  *out_fingerprint = fingerprint.substr(0, pos);
  return true;
}

// Reads |file_path| line by line and pass each line to the |callback| after
// trimming it. |out_string| is also passed to the |callback| each time. If
// |callback| returns true, stops reading the file and returns true. |callback|
// must update |out_string| before returning true.
bool FindLine(
    const base::FilePath& file_path,
    const base::Callback<bool(const std::string&, std::string*)>& callback,
    std::string* out_string) {
  // Do exactly the same stream handling as TextContentsEqual() in
  // base/files/file_util.cc which is known to work.
  std::ifstream file(file_path.value().c_str(), std::ios::in);
  if (!file.is_open()) {
    PLOG(WARNING) << "Cannot open " << file_path.value();
    return false;
  }

  do {
    std::string line;
    std::getline(file, line);

    // Check for any error state.
    if (file.bad()) {
      PLOG(WARNING) << "Failed to read " << file_path.value();
      return false;
    }

    // Trim all '\r' and '\n' characters from the end of the line.
    std::string::size_type end = line.find_last_not_of("\r\n");
    if (end == std::string::npos)
      line.clear();
    else if (end + 1 < line.length())
      line.erase(end + 1);

    // Stop reading the file if |callback| returns true.
    if (callback.Run(line, out_string))
      return true;
  } while (!file.eof());

  // |callback| didn't find anything in the file.
  return false;
}

// Sets the permission of the given |path|. If |path| is symbolic link, sets
// the permission of a file which the symlink points to.
bool SetFilePermissions(const base::FilePath& path, mode_t mode) {
  struct stat st;
  if (stat(path.value().c_str(), &st) < 0) {
    PLOG(ERROR) << "Failed to stat " << path.value();
    return false;
  }
  if ((st.st_mode & 07000) && ((st.st_mode & 07000) != (mode & 07000))) {
    LOG(INFO) << "Changing permissions of " << path.value() << " from "
              << (st.st_mode & ~S_IFMT) << " to " << (mode & ~S_IFMT);
  }

  if (chmod(path.value().c_str(), mode) != 0) {
    PLOG(ERROR) << "Failed to chmod " << path.value() << " to " << mode;
    return false;
  }
  return true;
}

class ArcMounterImpl : public ArcMounter {
 public:
  ArcMounterImpl() = default;
  ~ArcMounterImpl() override = default;

  bool Mount(const std::string& source,
             const base::FilePath& target,
             const char* filesystem_type,
             unsigned long mount_flags,  // NOLINT(runtime/int)
             const char* data) override {
    std::string source_resolved;
    if (!source.empty() && source[0] == '/')
      source_resolved = Realpath(base::FilePath(source)).value();
    else
      source_resolved = source;  // not a path (e.g. "tmpfs")

    if (mount(source_resolved.c_str(), Realpath(target).value().c_str(),
              filesystem_type, mount_flags, data) != 0) {
      PLOG(ERROR) << "Failed to mount " << source << " to " << target.value();
      return false;
    }
    return true;
  }

  bool Remount(const base::FilePath& target_directory,
               unsigned long mount_flags,  // NOLINT(runtime/int)
               const char* data) override {
    return Mount(std::string(),  // ignored
                 target_directory,
                 nullptr,  // ignored
                 mount_flags | MS_REMOUNT, data);
  }

  bool LoopMount(const std::string& source,
                 const base::FilePath& target,
                 unsigned long mount_flags) override {  // NOLINT(runtime/int)
    constexpr size_t kRetryMax = 10;
    for (size_t i = 0; i < kRetryMax; ++i) {
      bool retry = false;
      if (LoopMountInternal(source, target, mount_flags, &retry))
        return true;
      if (!retry)
        break;
      LOG(INFO) << "LoopMountInternal failed with EBUSY. Retrying...";
    }
    return false;
  }

  bool BindMount(const base::FilePath& old_path,
                 const base::FilePath& new_path) override {
    return Mount(old_path.value(), new_path, nullptr, MS_BIND, nullptr);
  }

  bool SharedMount(const base::FilePath& path) override {
    return Mount("none", path, nullptr, MS_SHARED, nullptr);
  }

  bool Umount(const base::FilePath& path) override {
    if (umount(Realpath(path).value().c_str()) != 0) {
      PLOG(ERROR) << "Failed to umount " << path.value();
      return false;
    }
    return true;
  }

  bool UmountLazily(const base::FilePath& path) override {
    if (umount2(Realpath(path).value().c_str(), MNT_DETACH) != 0) {
      PLOG(ERROR) << "Failed to lazy-umount " << path.value();
      return false;
    }
    return true;
  }

  bool LoopUmount(const base::FilePath& path) override {
    struct stat st;
    if (stat(path.value().c_str(), &st) < 0) {
      PLOG(ERROR) << "Failed to stat " << path.value();
      return false;
    }

    if (!Umount(path))
      return false;

    if (major(st.st_dev) != LOOP_MAJOR) {
      LOG(ERROR) << path.value()
                 << " is not loop-mounted. st_dev=" << st.st_dev;
      return false;
    }

    const std::string device_file = GetLoopDevice(minor(st.st_dev));
    const int fd = open(device_file.c_str(), O_RDWR);
    if (fd < 0) {
      PLOG(ERROR) << "Failed to open " << device_file;
      return false;
    }
    base::ScopedFD scoped_loop_fd(fd);

    if (ioctl(scoped_loop_fd.get(), LOOP_CLR_FD)) {
      PLOG(ERROR) << "Failed to free " << device_file;
      return false;
    }
    return true;
  }

 private:
  bool LoopMountInternal(const std::string& source,
                         const base::FilePath& target,
                         unsigned long mount_flags,  // NOLINT(runtime/int)
                         bool* out_retry) {
    constexpr char kLoopControl[] = "/dev/loop-control";

    *out_retry = false;
    int fd = open(kLoopControl, O_RDONLY);
    if (fd < 0) {
      PLOG(ERROR) << "Failed to open " << kLoopControl;
      return false;
    }
    base::ScopedFD scoped_control_fd(fd);

    const int32_t device_num =
        ioctl(scoped_control_fd.get(), LOOP_CTL_GET_FREE);
    if (device_num < 0) {
      PLOG(ERROR) << "Failed to allocate a loop device";
      return false;
    }

    const std::string device_file = GetLoopDevice(device_num);
    fd = open(device_file.c_str(), O_RDWR);
    if (fd < 0) {
      PLOG(ERROR) << "Failed to open " << device_file;
      return false;
    }
    base::ScopedFD scoped_loop_fd(fd);

    const bool is_readonly_mount = mount_flags & MS_RDONLY;
    fd = open(source.c_str(), is_readonly_mount ? O_RDONLY : O_RDWR);
    if (fd < 0) {
      // If the open failed because we tried to open a read only file as RW
      // we fallback to opening it with O_RDONLY
      if (!is_readonly_mount && (errno == EROFS || errno == EACCES)) {
        LOG(WARNING) << source << " is write-protected, using read-only";
        fd = open(source.c_str(), O_RDONLY);
      }
      if (fd < 0) {
        PLOG(ERROR) << "Failed to open " << source;
        return false;
      }
    }
    base::ScopedFD scoped_source_fd(fd);

    if (ioctl(scoped_loop_fd.get(), LOOP_SET_FD, scoped_source_fd.get()) < 0) {
      PLOG(ERROR) << "Failed to associate " << source << " with "
                  << device_file;
      // Set |out_retry| to true if LOOP_SET_FD returns EBUSY. The errno
      // indicates that another process has grabbed the same |device_num|
      // before arc-setup does that.
      *out_retry = (errno == EBUSY);
      return false;
    }

    if (Mount(device_file, target, "squashfs", mount_flags, nullptr))
      return true;

    // For debugging, ext4 might be used.
    if (Mount(device_file, target, "ext4", mount_flags, nullptr)) {
      LOG(INFO) << "Mounted " << source << " as ext4";
      return true;
    }

    // Mount failed. Remove |source| from the loop device so that |device_num|
    // can be reused.
    if (ioctl(scoped_loop_fd.get(), LOOP_CLR_FD) < 0)
      PLOG(ERROR) << "Failed to remove " << source << " from " << device_file;
    return false;
  }
};

}  // namespace

ScopedMount::ScopedMount(const base::FilePath& path, ArcMounter* mounter)
    : mounter_(mounter), path_(path) {}

ScopedMount::~ScopedMount() {
  PLOG_IF(INFO, !mounter_->UmountLazily(path_))
      << "Ignoring failure to umount " << path_.value();
}

// static
std::unique_ptr<ScopedMount> ScopedMount::CreateScopedMount(
    ArcMounter* mounter,
    const std::string& source,
    const base::FilePath& target,
    const char* filesystem_type,
    unsigned long mount_flags,  // NOLINT(runtime/int)
    const char* data) {
  if (!mounter->Mount(source, target, filesystem_type, mount_flags, data))
    return nullptr;
  return std::make_unique<ScopedMount>(target, mounter);
}

// static
std::unique_ptr<ScopedMount> ScopedMount::CreateScopedLoopMount(
    ArcMounter* mounter,
    const std::string& source,
    const base::FilePath& target,
    unsigned long flags) {  // NOLINT(runtime/int)
  if (!mounter->LoopMount(source, target, flags))
    return nullptr;
  return std::make_unique<ScopedMount>(target, mounter);
}

// static
std::unique_ptr<ScopedMount> ScopedMount::CreateScopedBindMount(
    ArcMounter* mounter,
    const base::FilePath& old_path,
    const base::FilePath& new_path) {
  if (!mounter->BindMount(old_path, new_path))
    return nullptr;
  return std::make_unique<ScopedMount>(new_path, mounter);
}

ScopedMountNamespace::ScopedMountNamespace(base::ScopedFD mount_namespace_fd)
    : mount_namespace_fd_(std::move(mount_namespace_fd)) {}

ScopedMountNamespace::~ScopedMountNamespace() {
  PLOG_IF(ERROR, setns(mount_namespace_fd_.get(), CLONE_NEWNS) != 0)
      << "Ignoring failure to restore original mount namespace";
}

// static
std::unique_ptr<ScopedMountNamespace>
ScopedMountNamespace::CreateScopedMountNamespaceForPid(pid_t pid) {
  constexpr char kCurrentMountNamespacePath[] = "/proc/self/ns/mnt";
  base::ScopedFD original_mount_namespace_fd(
      open(kCurrentMountNamespacePath, O_RDONLY));
  if (!original_mount_namespace_fd.is_valid()) {
    PLOG(ERROR) << "Failed to get the original mount namespace FD";
    return nullptr;
  }
  base::ScopedFD mount_namespace_fd(
      open(base::StringPrintf("/proc/%d/ns/mnt", pid).c_str(), O_RDONLY));
  if (!mount_namespace_fd.is_valid()) {
    PLOG(ERROR) << "Failed to get PID " << pid << "'s mount namespace FD";
    return nullptr;
  }

  if (setns(mount_namespace_fd.get(), CLONE_NEWNS) != 0) {
    PLOG(ERROR) << "Failed to enter PID " << pid << "'s mount namespace";
    return nullptr;
  }
  return std::make_unique<ScopedMountNamespace>(
      std::move(original_mount_namespace_fd));
}

std::string GetEnvOrDie(base::Environment* env, const char* name) {
  DCHECK(env);
  std::string result;
  CHECK(env->GetVar(name, &result)) << name << " not found";
  return result;
}

bool GetBooleanEnvOrDie(base::Environment* env, const char* name) {
  return GetEnvOrDie(env, name) == "1";
}

base::FilePath GetFilePathOrDie(base::Environment* env, const char* name) {
  return base::FilePath(GetEnvOrDie(env, name));
}

base::FilePath Realpath(const base::FilePath& path) {
  // We cannot use base::NormalizeFilePath because the function fails
  // if |path| points to a directory (for Windows compatibility.)
  char buf[PATH_MAX] = {};
  if (!realpath(path.value().c_str(), buf)) {
    if (errno != ENOENT)
      PLOG(WARNING) << "Failed to resolve " << path.value();
    return path;
  }
  return base::FilePath(buf);
}

// Taken from Chromium r403830 then slightly modified (see comments below.)
bool MkdirRecursively(const base::FilePath& full_path) {
  std::vector<base::FilePath> subpaths;

  // Collect a list of all parent directories.
  base::FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (base::FilePath path = full_path.DirName();
       path.value() != last_path.value(); path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (std::vector<base::FilePath>::reverse_iterator i = subpaths.rbegin();
       i != subpaths.rend(); ++i) {
    if (base::DirectoryExists(*i))
      continue;
    // Note: the original Chromium code uses 0700. We use 0755.
    if (mkdir(i->value().c_str(), 0755) == 0)
      continue;
    return false;
  }
  return true;
}

bool Chown(uid_t uid, gid_t gid, const base::FilePath& path) {
  return chown(path.value().c_str(), uid, gid) == 0;
}

bool Chcon(const std::string& context, const base::FilePath& path) {
  if (lsetfilecon(path.value().c_str(), context.c_str()) < 0) {
    PLOG(ERROR) << "Could not label " << path.value() << " with " << context;
    return false;
  }

  return true;
}

bool InstallDirectory(mode_t mode,
                      uid_t uid,
                      gid_t gid,
                      const base::FilePath& path) {
  if (!MkdirRecursively(path))
    return false;

  // Unlike 'mkdir -m mode -p' which does not change modes when the path already
  // exists, 'install -d' always sets modes and owner regardless of whether the
  // path exists or not.
  const bool chown_result = Chown(uid, gid, path);
  const bool chmod_result = SetFilePermissions(path, mode);
  return chown_result && chmod_result;
}

bool WriteToFile(const base::FilePath& file_path,
                 mode_t mode,
                 const std::string& content) {
  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid())
    return false;
  if (!SetFilePermissions(file_path, mode))
    return false;
  if (content.empty())
    return true;
  // Note: Write() makes a best effort to write all data. While-loop is not
  // needed here.
  return file.Write(0, content.c_str(), content.size()) ==
         static_cast<int>(content.size());
}

bool GetPropertyFromFile(const base::FilePath& prop_file_path,
                         const std::string& prop_name,
                         std::string* out_prop) {
  const std::string line_prefix_to_find = prop_name + '=';
  if (FindLine(prop_file_path, base::Bind(&FindProperty, line_prefix_to_find),
               out_prop)) {
    return true;  // found the line.
  }
  LOG(WARNING) << prop_name << " is not in " << prop_file_path.value();
  return false;
}

bool GetFingerprintFromPackagesXml(const base::FilePath& packages_xml_path,
                                   std::string* out_fingerprint) {
  if (FindLine(packages_xml_path, base::Bind(&FindFingerprint),
               out_fingerprint)) {
    return true;  // found it.
  }
  LOG(WARNING) << "No fingerprint found in " << packages_xml_path.value();
  return false;
}

bool CreateOrTruncate(const base::FilePath& file_path, mode_t mode) {
  return WriteToFile(file_path, mode, std::string());
}

bool WaitForPaths(std::initializer_list<base::FilePath> paths,
                  const base::TimeDelta& timeout,
                  base::TimeDelta* out_elapsed) {
  const base::TimeDelta sleep_interval = timeout / 20;
  std::list<base::FilePath> left(paths);

  base::ElapsedTimer timer;
  do {
    left.erase(std::remove_if(left.begin(), left.end(), base::PathExists),
               left.end());
    if (left.empty())
      break;  // all paths are found.
    base::PlatformThread::Sleep(sleep_interval);
  } while (timeout >= timer.Elapsed());

  if (out_elapsed)
    *out_elapsed = timer.Elapsed();

  for (const auto& path : left)
    LOG(ERROR) << path.value() << " not found";
  return left.empty();
}

bool LaunchAndWait(const std::vector<std::string>& argv) {
  base::Process process(base::LaunchProcess(argv, base::LaunchOptions()));
  if (!process.IsValid())
    return false;
  int exit_code = -1;
  return process.WaitForExit(&exit_code) && (exit_code == 0);
}

bool RestoreconRecursively(const std::vector<base::FilePath>& directories) {
  return RestoreconInternal(directories, true /* is_recursive */);
}

bool Restorecon(const std::vector<base::FilePath>& paths) {
  return RestoreconInternal(paths, false /* is_recursive */);
}

std::string GenerateFakeSerialNumber(const std::string& chromeos_user,
                                     const std::string& salt) {
  constexpr size_t kMaxHardwareIdLen = 20;
  const std::string hash(crypto::SHA256HashString(chromeos_user + salt));
  return base::HexEncode(hash.data(), hash.length())
      .substr(0, kMaxHardwareIdLen);
}

uint64_t GetArtCompilationOffsetSeed(const std::string& image_build_id,
                                     const std::string& salt) {
  uint64_t result = 0;
  std::string input;
  do {
    input += image_build_id + salt;
    crypto::SHA256HashString(input, &result, sizeof(result));
  } while (!result);
  return result;
}

void MoveDataAppOatDirectory(const base::FilePath& data_app_directory,
                             const base::FilePath& old_executables_directory) {
  base::FileEnumerator dir_enum(data_app_directory, false /* recursive */,
                                base::FileEnumerator::DIRECTORIES);
  for (base::FilePath pkg_directory_name = dir_enum.Next();
       !pkg_directory_name.empty(); pkg_directory_name = dir_enum.Next()) {
    const base::FilePath oat_directory = pkg_directory_name.Append("oat");
    if (!PathExists(oat_directory))
      continue;

    const base::FilePath temp_oat_directory = old_executables_directory.Append(
        "oat-" + pkg_directory_name.BaseName().value());
    base::File::Error file_error;
    if (!base::ReplaceFile(oat_directory, temp_oat_directory, &file_error)) {
      PLOG(ERROR) << "Failed to move cache folder " << oat_directory.value()
                  << ". Error code: " << file_error;
    }
  }
}

bool DeleteFilesInDir(const base::FilePath& directory) {
  base::FileEnumerator files(
      directory, true /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::SHOW_SYM_LINKS);
  bool retval = true;
  for (base::FilePath file = files.Next(); !file.empty(); file = files.Next()) {
    if (!DeleteFile(file, false /*recursive*/)) {
      LOG(ERROR) << "Failed to delete file " << file.value();
      retval = false;
    }
  }
  return retval;
}

std::unique_ptr<ArcMounter> GetDefaultMounter() {
  return std::make_unique<ArcMounterImpl>();
}

bool FindLineForTesting(
    const base::FilePath& file_path,
    const base::Callback<bool(const std::string&, std::string*)>& callback,
    std::string* out_string) {
  return FindLine(file_path, callback, out_string);
}

std::string GetChromeOsChannelFromFile(
    const base::FilePath& lsb_release_file_path) {
  constexpr char kChromeOsReleaseTrackProp[] = "CHROMEOS_RELEASE_TRACK";
  const std::set<std::string> kChannels = {
      "beta-channel",    "canary-channel", "dev-channel",
      "dogfood-channel", "stable-channel", "testimage-channel"};
  const std::string kUnknown = "unknown";
  const std::string kChannelSuffix = "-channel";

  // Read the channel property from /etc/lsb-release
  std::string chromeos_channel;
  if (!GetPropertyFromFile(lsb_release_file_path, kChromeOsReleaseTrackProp,
                           &chromeos_channel)) {
    LOG(ERROR) << "Failed to get the ChromeOS channel from "
               << lsb_release_file_path.value();
    return kUnknown;
  }

  if (kChannels.find(chromeos_channel) == kChannels.end()) {
    LOG(WARNING) << "Unknown ChromeOS channel: \"" << chromeos_channel << "\"";
    return kUnknown;
  }
  return chromeos_channel.erase(chromeos_channel.find(kChannelSuffix),
                                kChannelSuffix.size());
}

bool GetOciContainerState(const base::FilePath& path,
                          pid_t* out_container_pid,
                          base::FilePath* out_rootfs) {
  // Read the OCI container state from |path|. Its format is documented in
  // https://github.com/opencontainers/runtime-spec/blob/master/runtime.md#state
  std::string json_str;
  if (!base::ReadFileToString(path, &json_str)) {
    PLOG(ERROR) << "Failed to read json string from " << path.value();
    return false;
  }
  std::string error_msg;
  std::unique_ptr<base::Value> container_state_value =
      base::JSONReader::ReadAndReturnError(
          json_str, base::JSON_PARSE_RFC, nullptr /* error_code_out */,
          &error_msg, nullptr /* error_line_out */,
          nullptr /* error_column_out */);
  if (!container_state_value) {
    LOG(ERROR) << "Failed to parse json: " << error_msg;
    return false;
  }
  const base::DictionaryValue* container_state = nullptr;
  if (!container_state_value->GetAsDictionary(&container_state)) {
    LOG(ERROR) << "Failed to read container state as dictionary";
    return false;
  }

  // Get the container PID and the rootfs path.
  if (!container_state->GetInteger("pid", out_container_pid)) {
    LOG(ERROR) << "Failed to get PID from container state";
    return false;
  }
  const base::DictionaryValue* annotations = nullptr;
  if (!container_state->GetDictionary("annotations", &annotations)) {
    LOG(ERROR) << "Failed to get annotations from container state";
    return false;
  }
  std::string container_root_str;
  if (!annotations->GetStringWithoutPathExpansion(
          "org.chromium.run_oci.container_root", &container_root_str)) {
    LOG(ERROR)
        << "Failed to get org.chromium.run_oci.container_root annotation";
    return false;
  }
  base::FilePath container_root(container_root_str);
  if (!base::ReadSymbolicLink(
          container_root.Append("mountpoints/container-root"), out_rootfs)) {
    PLOG(ERROR) << "Failed to read container root symlink";
    return false;
  }

  return true;
}

}  // namespace arc
