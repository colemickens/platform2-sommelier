// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_H_
#define CHROMEOS_PLATFORM_H_

#include <sys/stat.h>

#include <set>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <chromeos/utility.h>

namespace base { class Time; }

namespace chromeos {

// Default umask
extern const int kDefaultUmask;

class ProcessInformation;

// Platform specific routines abstraction layer.
// Also helps us to be able to mock them in tests.
class Platform {
 public:
  Platform();

  virtual ~Platform();

  // Calls the platform mount
  //
  // Parameters
  //   from - The node to mount from
  //   to - The node to mount to
  //   type - The fs type
  //   mount_options - The mount options to pass to mount()
  virtual bool Mount(const std::string& from, const std::string& to,
                     const std::string& type, const std::string& mount_options);

  // Creates a bind mount
  //
  // Parameters
  //   from - Where to mount from
  //   to - Where to mount to
  virtual bool Bind(const std::string& from, const std::string& to);

  // Calls the platform unmount
  //
  // Parameters
  //   path - The path to unmount
  //   lazy - Whether to call a lazy unmount
  //   was_busy (OUT) - Set to true on return if the mount point was busy
  virtual bool Unmount(const std::string& path, bool lazy, bool* was_busy);

  // Returns true if the directory is in the mtab
  //
  // Parameters
  //   directory - The directory to check
  virtual bool IsDirectoryMounted(const std::string& directory);

  // Returns true if the directory is in the mtab mounted with the specified
  // source
  //
  // Parameters
  //   directory - The directory to check
  //   from - The source node
  virtual bool IsDirectoryMountedWith(const std::string& directory,
                                      const std::string& from);

  // GetProcessesWithOpenFiles
  //
  // Parameters
  //   path - The path to check if the process has open files on
  //   pids (OUT) - The PIDs found
  void GetProcessesWithOpenFiles(const std::string& path_in,
                                 std::vector<ProcessInformation>* processes);

  // Calls the platform stat() function to obtain the ownership of
  // a given path. The path may be a directory or a file.
  //
  // Parameters
  //   path - The path to look up
  //   user_id - The user ID of the path. NULL if the result is not needed.
  //   group_id - The group ID of the path. NULL if the result is not needed.
  virtual bool GetOwnership(const std::string& path, uid_t* user_id,
                            gid_t* group_id) const;

  // Calls the platform chown() function on the given path.
  //
  // The path may be a directory or a file.
  //
  // Parameters
  //   path - The path to set ownership on
  //   user_id - The user_id to assign ownership to
  //   group_id - The group_id to assign ownership to
  virtual bool SetOwnership(const std::string& directory, uid_t user_id,
                            gid_t group_id) const;

  // Calls the platform stat() function to obtain the permissions of
  // the given path. The path may be a directory or a file.
  //
  // Parameters
  //   path - The path to look up
  //   mode - The permissions of the path
  virtual bool GetPermissions(const std::string& path, mode_t* mode) const;

  // Calls the platform chmod() function on the given path.
  // The path may be a directory or a file.
  //
  // Parameters
  //   path - The path to change the permissions on
  //   mode - the mode to change the permissions to
  virtual bool SetPermissions(const std::string& path, mode_t mode) const;

  // Sets the path accessible by a group with specified permissions
  //
  // Parameters
  //   path - The path to change the ownership and permissions on
  //   group_id - The group ID to assign to the path
  //   group_mode - The group permissions to assign to the path
  virtual bool SetGroupAccessible(const std::string& path,
                                  gid_t group_id,
                                  mode_t group_mode) const;

  // Sets the current umask, returning the old mask
  //
  // Parameters
  //   new_mask - The mask to set
  virtual int SetMask(int new_mask) const;

  // Returns the user and group ids for a user
  //
  // Parameters
  //   user - The username to query for
  //   user_id (OUT) - The user ID on success
  //   group_id (OUT) - The group ID on success
  virtual bool GetUserId(const std::string& user, uid_t* user_id,
                         gid_t* group_id) const;

  // Returns the group id for a group
  //
  // Parameters
  //   group - The group name to query for
  //   group_id (OUT) - The group ID on success
  virtual bool GetGroupId(const std::string& group, gid_t* group_id) const;

  // Return the available disk space in bytes on the volume containing |path|,
  // or -1 on failure.
  // Code duplicated from Chrome's base::SysInfo::AmountOfFreeDiskSpace().
  //
  // Parameters
  //   path - the pathname of any file within the mounted file system
  virtual int64 AmountOfFreeDiskSpace(const std::string& path) const;

  // Returns true if the specified file exists.
  //
  // Parameters
  //  path - Path of the file to check
  virtual bool FileExists(const std::string& path);

  // Check if a directory exists as the given path
  virtual bool DirectoryExists(const std::string& path);

  // Reads a file completely into a Blob.
  //
  // Parameters
  //  path - Path of the file to read
  //  blob - blob to populate
  virtual bool ReadFile(const std::string& path, chromeos::Blob* blob);
  virtual bool ReadFileToString(const std::string& path, std::string* string);

  // Writes the entirety of the data to the given file.
  //
  // Parameters
  //  path - Path of the file to write
  //  blob - blob to populate from
  virtual bool WriteFile(const std::string& path, const chromeos::Blob& blob);

  // Delete file(s) at the given path
  //
  // Parameters
  //  path - string containing file path to delete
  //  recursive - whether to perform recursive deletion of the subtree
  virtual bool DeleteFile(const std::string& path, bool recursive);

  // Create a directory with the given path
  virtual bool CreateDirectory(const std::string& path);

  // Enumerate all directory entries in a given directory
  //
  // Parameters
  //  path - root of the tree to enumerate
  //  is_recursive - true to enumerate recursively
  //  ent_list - vector of strings to add enumerate directory entry paths into
  virtual bool EnumerateDirectoryEntries(const std::string& path,
                                         bool is_recursive,
                                         std::vector<std::string>* ent_list);

  // Look up information about a file or directory
  //
  // Parameters
  //  path - element to look up
  //  buf - buffer to store results into
  virtual bool Stat(const std::string& path, struct stat *buf);

  // Rename a file or directory
  //
  // Parameters
  //  from - name to rename from
  //  to - name to rename to
  virtual bool Rename(const std::string& from, const std::string& to);

  // Retuns the current time.
  virtual base::Time GetCurrentTime() const;

  // Copies from to to.
  virtual bool Copy(const std::string& from, const std::string& to);

 private:
  // Returns the process and open file information for the specified process id
  // with files open on the given path
  //
  // Parameters
  //   pid - The process to check
  //   path_in - The file path to check for
  //   process_info (OUT) - The ProcessInformation to store the results in
  void GetProcessOpenFileInformation(pid_t pid, const std::string& path_in,
                                     ProcessInformation* process_info);

  // Returns a vector of PIDs that have files open on the given path
  //
  // Parameters
  //   path - The path to check if the process has open files on
  //   pids (OUT) - The PIDs found
  void LookForOpenFiles(const std::string& path_in, std::vector<pid_t>* pids);

  // Returns true if child is a file or folder below or equal to parent.  If
  // parent is a directory, it should end with a '/' character.
  //
  // Parameters
  //   parent - The parent directory
  //   child - The child directory/file
  bool IsPathChild(const std::string& parent, const std::string& child);

  // Returns the target of the specified link
  //
  // Parameters
  //   link_path - The link to check
  std::string ReadLink(const std::string& link_path);

  DISALLOW_COPY_AND_ASSIGN(Platform);
};

}  // namespace chromeos

#endif  // CRYPTOHOME_PLATFORM_H_
