// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_PLATFORM_H_
#define CRYPTOHOME_PLATFORM_H_

#include <base/basictypes.h>
#include <string>
#include <vector>

namespace cryptohome {

// Default mount options
extern const int kDefaultMountOptions;
// Default length to use in call to getpwnam_r if the system default is not
// available
extern const int kDefaultPwnameLength;
// Default umask
extern const int kDefaultUmask;
// Where to find mtab
extern const std::string kMtab;
// The procfs dir
extern const std::string kProcDir;

// TODO(fes): Description
class Platform {
 public:

  Platform();

  virtual ~Platform();

  // Calls the platform mount
  //
  // Paramters
  //   from - The node to mount from
  //   to - The node to mount to
  //   type - The fs type
  //   mount_options - The mount options to pass to mount()
  bool Mount(const std::string& from, const std::string& to,
             const std::string& type, const std::string& mount_options);

  // Calls the platform unmount
  //
  // Parameters
  //   path - The path to unmount
  //   lazy - Whether to call a lazy unmount
  //   was_busy (OUT) - Set to true on return if the mount point was busy
  bool Unmount(const std::string& path, bool lazy, bool* was_busy);

  // Returns true if the directory is in the mtab
  //
  // Parameters
  //   directory - The directory to check
  bool IsDirectoryMounted(const std::string& directory);

  // Returns true if the directory is in the mtab mounted with the specified
  // source
  //
  // Parameters
  //   directory - The directory to check
  //   from - The source node
  bool IsDirectoryMountedWith(const std::string& directory,
                              const std::string& from);

  // Terminates or kills processes (except the current) that have files open on
  // the specified path.  Returns true if it tried to kill any processes.
  //
  // Parameters
  //   path - The path to check if the process has open files on
  //   hard - If true, send a SIGKILL instead of SIGTERM
  bool TerminatePidsWithOpenFiles(const std::string& path, bool hard);

  // Returns a vector of PIDs that have files open on the given path
  //
  // Parameters
  //   path - The path to check if the process has open files on
  //   pids (OUT) - The PIDs found
  void LookForOpenFiles(const std::string& path_in, std::vector<pid_t>* pids);

  // Terminates or kills processes (except the current) that have the user ID
  // specified.  Returns true if it tried to kill any processes.
  //
  // Parameters
  //   path - The path to check if the process has open files on
  //   hard - If true, send a SIGKILL instead of SIGTERM
  bool TerminatePidsForUser(const uid_t uid, bool hard);

  // Returns a vector of PIDs whose Real, Effective, Saved, or File UID is equal
  // to that requested
  //
  // Parameters
  //   uid - the user ID to search for
  //   pids (OUT) - the list of PIDs
  void GetPidsForUser(uid_t uid, std::vector<pid_t>* pids);

  // Calls the platform chown() function recursively on the directory
  //
  // Parameters
  //   directory - The directory to set ownership on
  //   user_id - The user_id to assign ownership to
  //   group_id - The group_id to assign ownership to
  bool SetOwnership(const std::string& directory, uid_t user_id,
                    gid_t group_id);

  // Sets the current umask, returning the old mask
  //
  // Parameters
  //   new_mask - The mask to set
  int SetMask(int new_mask);

  // Returns the user and group ids for a user
  //
  // Parameters
  //   user - The username to query for
  //   user_id (OUT) - The user ID on success
  //   group_id (OUT) - The group ID on success
  bool GetUserId(const std::string& user, uid_t* user_id, gid_t* group_id);

  // Clears the user keyring
  static void ClearUserKeyring();

  // Overrides the default mount options
  void set_mount_options(int value) {
    mount_options_ = value;
  }

  // Overrides the default mtab file
  void set_mtab_file(const std::string& value) {
    mtab_file_ = value;
  }

  // Overrides the default procfs dir
  void set_proc_dir(const std::string& value) {
    proc_dir_ = value;
  }

 private:
  int mount_options_;
  int umask_;
  std::string mtab_file_;
  std::string proc_dir_;

  DISALLOW_COPY_AND_ASSIGN(Platform);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_PLATFORM_H_
