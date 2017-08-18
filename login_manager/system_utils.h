// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_SYSTEM_UTILS_H_

#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/files/scoped_temp_dir.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <login_manager/named_platform_handle_utils.h>

namespace base {
class FilePath;
}

struct DBusPendingCall;

namespace login_manager {

enum class DevModeState {
  DEV_MODE_OFF,
  DEV_MODE_ON,
  DEV_MODE_UNKNOWN,
};

enum class VmState {
  OUTSIDE_VM,
  INSIDE_VM,
  UNKNOWN,
};

class ScopedDBusPendingCall;

class SystemUtils {
 public:
  SystemUtils() {}
  virtual ~SystemUtils() {}

  // Sends |signal| to |pid|, with uid and euid set to |owner|.
  // NOTE: Your saved UID is kept unchanged.  If you expect to drop and regain
  // root privs, MAKE SURE YOUR suid == 0.
  virtual int kill(pid_t pid, uid_t owner, int signal) = 0;

  // Returns time, in seconds, since the unix epoch.
  virtual time_t time(time_t* t) = 0;

  // Forks a new process.  In the parent, returns child's pid.  In child, 0.
  virtual pid_t fork() = 0;

  // Run an external program and collect its stdout in |output|.
  virtual bool GetAppOutput(const std::vector<std::string>& argv,
                            std::string* output) = 0;

  // Returns the current developer mode.
  virtual DevModeState GetDevModeState() = 0;

  // Returns whether Chrome OS is running inside a Virtual Machine.
  virtual VmState GetVmState() = 0;

  // Returns: true if process group specified by |child_spec| exited,
  //          false if we time out.
  virtual bool ProcessGroupIsGone(pid_t child_spec,
                                  base::TimeDelta timeout) = 0;

  // Returns: true if process specified by |child_spec| exited,
  //          false if we time out.
  virtual bool ProcessIsGone(pid_t child_spec, base::TimeDelta timeout) = 0;

  virtual bool EnsureAndReturnSafeFileSize(const base::FilePath& file,
                                           int32_t* file_size_32) = 0;

  // Returns whether a file exists.
  virtual bool Exists(const base::FilePath& file) = 0;

  // Returns whether a directory exists.
  virtual bool DirectoryExists(const base::FilePath& dir) = 0;

  // Returns true if the given directory is empty or does not exist.
  virtual bool IsDirectoryEmpty(const base::FilePath& dir) = 0;

  // Creates a uniquely-named read-only file under |dir|.
  // Upon success, sets |temp_file| and returns true. Upon failure, |temp_file|
  // remains untouched.
  virtual bool CreateReadOnlyFileInTempDir(base::FilePath* temp_file) = 0;

  // Creates a uniquely-named directory under |parent_dir|.
  // Upon success, sets |out_dir| and returns true. Upon failure, |out_dir|
  // remains untouched.
  virtual bool CreateTemporaryDirIn(const base::FilePath& parent_dir,
                                    base::FilePath* out_dir) = 0;

  // Creates a directory.
  virtual bool CreateDir(const base::FilePath& dir) = 0;

  // Generates a guaranteed-unique filename in a write-only temp dir.
  // Returns false upon failure.
  virtual bool GetUniqueFilenameInWriteOnlyTempDir(
      base::FilePath* temp_file_path) = 0;

  // Removes a directory tree.
  virtual bool RemoveDirTree(const base::FilePath& dir) = 0;

  // Removes a file.
  virtual bool RemoveFile(const base::FilePath& filename) = 0;

  // Renames a directory.
  virtual bool RenameDir(const base::FilePath& source,
                         const base::FilePath& target) = 0;

  // Atomically writes the given buffer into the file, overwriting any
  // data that was previously there.  Returns true upon success, false
  // otherwise.
  virtual bool AtomicFileWrite(const base::FilePath& filename,
                               const std::string& data) = 0;

  // Returns the amount of free disk space in bytes for the filesystem
  // containing |path|.
  virtual int64_t AmountOfFreeDiskSpace(const base::FilePath& path) = 0;

  // Calls brillo::userdb::GetGroupInfo().
  virtual bool GetGroupInfo(const std::string& group_name, gid_t* out_gid) = 0;

  // Changes ownership of |filename|. When |pid| or |gid| is -1, that ID is
  // not changed. Returns true upon success.
  virtual bool ChangeOwner(const base::FilePath& filename,
                           pid_t pid,
                           gid_t gid) = 0;

  // Calls base::SetPosixFilePermissions().
  virtual bool SetPosixFilePermissions(const base::FilePath& filename,
                                       mode_t mode) = 0;

  // Calls mojo::edk::CreateServerHandle().
  virtual ScopedPlatformHandle CreateServerHandle(
      const NamedPlatformHandle& named_handle) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemUtils);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SYSTEM_UTILS_H_
