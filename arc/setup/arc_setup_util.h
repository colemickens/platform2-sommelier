// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_SETUP_ARC_SETUP_UTIL_H_
#define ARC_SETUP_ARC_SETUP_UTIL_H_

#include <sys/types.h>
#include <unistd.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>

namespace base {

class Environment;
class TimeDelta;

}  // namespace base

namespace brillo {

class CrosConfigInterface;

}  // namespace brillo

namespace arc {

#if defined(USE_HOUDINI)
constexpr bool kUseHoudini = true;
#else
constexpr bool kUseHoudini = false;
#endif  // USE_HOUDINI

#if defined(USE_MASTER_CONTAINER)
constexpr bool kUseMasterContainer = true;
#else
constexpr bool kUseMasterContainer = false;
#endif  // USE_MASTER_CONTAINER

#if defined(USE_NDK_TRANSLATION)
constexpr bool kUseNdkTranslation = true;
#else
constexpr bool kUseNdkTranslation = false;
#endif  // USE_NDK_TRANSLATION

// A class that provides mount(2) and umount(2) wrappers. They return true on
// success.
class ArcMounter {
 public:
  virtual ~ArcMounter() = default;

  virtual bool Mount(const std::string& source,
                     const base::FilePath& target,
                     const char* filesystem_type,
                     unsigned long mount_flags,  // NOLINT(runtime/int)
                     const char* data) = 0;

  virtual bool Remount(const base::FilePath& target_directory,
                       unsigned long mount_flags,  // NOLINT(runtime/int)
                       const char* data) = 0;

  virtual bool LoopMount(const std::string& source,
                         const base::FilePath& target,
                         unsigned long mount_flags) = 0;  // NOLINT(runtime/int)

  virtual bool BindMount(const base::FilePath& old_path,
                         const base::FilePath& new_path) = 0;

  virtual bool SharedMount(const base::FilePath& path) = 0;

  virtual bool Umount(const base::FilePath& path) = 0;

  virtual bool UmountLazily(const base::FilePath& path) = 0;

  // Unmounts |path|, then frees the loop device for the |path|.
  virtual bool LoopUmount(const base::FilePath& path) = 0;
};

// A class that umounts a mountpoint when the mountpoint goes out of scope.
class ScopedMount {
 public:
  ScopedMount(const base::FilePath& path, ArcMounter* mounter);
  ~ScopedMount();

  // Mounts |source| to |target| and returns a unique_ptr that umounts the
  // mountpoint when it goes out of scope.
  static std::unique_ptr<ScopedMount> CreateScopedMount(
      ArcMounter* mounter,
      const std::string& source,
      const base::FilePath& target,
      const char* filesystem_type,
      unsigned long mount_flags,  // NOLINT(runtime/int)
      const char* data);

  // Loopmounts |source| to |target| and returns a unique_ptr that umounts the
  // mountpoint when it goes out of scope.
  static std::unique_ptr<ScopedMount> CreateScopedLoopMount(
      ArcMounter* mounter,
      const std::string& source,
      const base::FilePath& target,
      unsigned long flags);  // NOLINT(runtime/int)

  // Bindmounts |old_path| to |new_path| and returns a unique_ptr that umounts
  // the mountpoint when it goes out of scope.
  static std::unique_ptr<ScopedMount> CreateScopedBindMount(
      ArcMounter* mounter,
      const base::FilePath& old_path,
      const base::FilePath& new_path);

 private:
  // Owned by caller.
  ArcMounter* const mounter_;
  const base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMount);
};

// A class that restores a mount namespace when it goes out of scope. This can
// be done by entering another process' mount namespace by using
// CreateScopedMountNamespaceForPid(), or supplying a mount namespace FD
// directly.
class ScopedMountNamespace {
 public:
  // Enters the process identified by |pid|'s mount namespace and returns a
  // unique_ptr that restores the original mount namespace when it goes out of
  // scope.
  static std::unique_ptr<ScopedMountNamespace> CreateScopedMountNamespaceForPid(
      pid_t pid);

  explicit ScopedMountNamespace(base::ScopedFD mount_namespace_fd);
  ~ScopedMountNamespace();

 private:
  base::ScopedFD mount_namespace_fd_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMountNamespace);
};

// Finds an environment variable |name| from |env| and returns it as string.
// Otherwise calls exit(1).
std::string GetEnvOrDie(base::Environment* env, const char* name);

// Does the same as GetEnvOrDie but returns true when the variable is "1".
// When it is not, returns false.
bool GetBooleanEnvOrDie(base::Environment* env, const char* name);

// The same as GetEnvOrDie except that this version returns the variable as
// FilePath.
base::FilePath GetFilePathOrDie(base::Environment* env, const char* name);

// Resolves |path| to an absolute path that does not include symbolic links
// or the special .  or ..  directory entries.
base::FilePath Realpath(const base::FilePath& path);

// Creates directories specified by |full_path|. Newly created directories will
// have 0755 permissions. Returns true on success.
bool MkdirRecursively(const base::FilePath& full_path);

// Changes the owner of the |path|. Returns true on success.
bool Chown(uid_t uid, gid_t gid, const base::FilePath& path);

// Changes SELinux context of the |path|. Returns true on success.
bool Chcon(const std::string& context, const base::FilePath& path);

// Creates the |path| with the |mode|, |uid|, and |gid|. Also creates parent
// directories of the |path| if they do not exist. Newly created parent
// directories will have 0755 (mode), caller's uid, and caller's gid.
// Returns true on success.
bool InstallDirectory(mode_t mode,
                      uid_t uid,
                      gid_t gid,
                      const base::FilePath& path);

// Creates |file_path| with |mode| and writes |content| to the file. If the
// file already exists, this function overwrites the existing one and sets its
// mode to |mode|. Returns true on success.
bool WriteToFile(const base::FilePath& file_path,
                 mode_t mode,
                 const std::string& content);

// Reads |prop_file_path| for an Android property with |prop_name|. If the
// property is found, stores its value in |out_prop| and returns true.
// Otherwise, returns false without updating |out_prop|.
bool GetPropertyFromFile(const base::FilePath& prop_file_path,
                         const std::string& prop_name,
                         std::string* out_prop);

// Reads Android's packages.xml at |packages_xml_path| and stores the OS
// fingerprint for the internal storage found in the XML in |out_fingerprint|.
// If the file does not exist or no fingerprint is found in the file, returns
// false.
bool GetFingerprintFromPackagesXml(const base::FilePath& packages_xml_path,
                                   std::string* out_fingerprint);

// Creates |file_path| with |mode|. If the file already exists, this function
// sets the file size to 0 and mode to |mode|. Returns true on success.
bool CreateOrTruncate(const base::FilePath& file_path, mode_t mode);

// Waits for all paths in |paths| to be available. Returns true if all the paths
// are found. If it times out, returns false. If |out_elapsed| is not |nullptr|,
// the function stores the time spent in the function in the variable.
bool WaitForPaths(std::initializer_list<base::FilePath> paths,
                  const base::TimeDelta& timeout,
                  base::TimeDelta* out_elapsed);

// Launches the command specified by |argv| and waits for the command to finish.
// Returns true if the command returns 0.
//
// WARNING: LaunchAndWait is *very* slow. Use this only when it's unavoidable.
// One LaunchAndWait call will take at least ~40ms on ARM Chromebooks because
// arc_setup is executed when the CPU is very busy and fork/exec takes time.
//
// WARNING: *Never* execute /bin/[u]mount with LaunchAndWait which may take
// ~200ms or more. Instead, use one of the mount/umount syscall wrappers above.
bool LaunchAndWait(const std::vector<std::string>& argv);

// Restores contexts of the |directories| and their contents recursively.
// Returns true on success.
bool RestoreconRecursively(const std::vector<base::FilePath>& directories);

// Restores contexts of the |paths|. Returns true on success.
bool Restorecon(const std::vector<base::FilePath>& paths);

// Generates a unique, 20-character hex string from |chromeos_user| and
// |salt| which can be used as Android's ro.boot.serialno and ro.serialno
// properties. Note that Android treats serialno in a case-insensitive manner.
std::string GenerateFakeSerialNumber(const std::string& chromeos_user,
                                     const std::string& salt);

// Gets an offset seed (>0) that can be passed to ArtContainer::PatchImage().
uint64_t GetArtCompilationOffsetSeed(const std::string& image_build_id,
                                     const std::string& salt);

// Renames to fast remove executable cache in /data/app/package/oat
void MoveDataAppOatDirectory(const base::FilePath& data_app_directory,
                             const base::FilePath& temp_data_app_directory);

// Deletes files in |directory|, directory tree is kept to avoid recreating
// sub-directories.
bool DeleteFilesInDir(const base::FilePath& directory);

// Returns a mounter for production.
std::unique_ptr<ArcMounter> GetDefaultMounter();

// See FindLine() in arc_setup_util.cc.
bool FindLineForTesting(
    const base::FilePath& file_path,
    const base::Callback<bool(const std::string&, std::string*)>& callback,
    std::string* out_string);

// See OpenSafely() in arc_setup_util.cc.
base::ScopedFD OpenSafelyForTesting(const base::FilePath& path,
                                    int flags,
                                    mode_t mode);

// Reads |lsb_release_file_path| and returns the Chrome OS channel, or
// "unknown" in case of failures.
std::string GetChromeOsChannelFromFile(
    const base::FilePath& lsb_release_file_path);

// Reads the OCI container state from |path| and populates |out_container_pid|
// with the PID of the container and |out_rootfs| with the path to the root of
// the container.
bool GetOciContainerState(const base::FilePath& path,
                          pid_t* out_container_pid,
                          base::FilePath* out_rootfs);

// Expands the contents of a template Android property file.  Strings like
// {property} will be looked up in |config| and replaced with their values.
// Returns true if all {} strings were successfully expanded, or false if any
// properties were not found.
bool ExpandPropertyContents(const std::string& content,
                            brillo::CrosConfigInterface* config,
                            std::string* expanded_content);

// Truncates the value side of an Android key=val property line, including
// handling the special case of build fingerprint.
std::string TruncateAndroidProperty(const std::string& line);

}  // namespace arc

#endif  // ARC_SETUP_ARC_SETUP_UTIL_H_
