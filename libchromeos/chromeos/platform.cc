// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Platform

#include "platform.h"

#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <base/file_util.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <base/sys_info.h>
#include <base/time.h>
#include <chromeos/utility.h>

#include "process_information.h"

using base::SplitString;
using std::string;

namespace chromeos {

const int kDefaultMountOptions = MS_NOEXEC | MS_NOSUID | MS_NODEV;
const int kDefaultPwnameLength = 1024;
const int kDefaultUmask = S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH
                               | S_IXOTH;
const std::string kMtab = "/etc/mtab";
const std::string kProcDir = "/proc";

Platform::Platform() { }

Platform::~Platform() { }

bool Platform::IsDirectoryMounted(const std::string& directory) {
  // Trivial string match from /etc/mtab to see if the cryptohome mount point is
  // listed.  This works because Chrome OS is a controlled environment and the
  // only way /home/chronos/user should be mounted is if cryptohome mounted it.
  string contents;
  if (file_util::ReadFileToString(FilePath(kMtab), &contents)) {
    if (contents.find(StringPrintf(" %s ", directory.c_str()))
        != string::npos) {
      return true;
    }
  }
  return false;
}

bool Platform::IsDirectoryMountedWith(const std::string& directory,
                                      const std::string& from) {
  // Trivial string match from /etc/mtab to see if the cryptohome mount point
  // and the user's vault path are present.  Assumes this user is mounted if it
  // finds both.  This will need to change if simultaneous login is implemented.
  string contents;
  if (file_util::ReadFileToString(FilePath(kMtab), &contents)) {
    if ((contents.find(StringPrintf(" %s ", directory.c_str()))
         != string::npos)
        && (contents.find(StringPrintf("%s ",
                                       from.c_str()).c_str())
            != string::npos)) {
      return true;
    }
  }
  return false;
}

bool Platform::Mount(const std::string& from, const std::string& to,
                     const std::string& type,
                     const std::string& mount_options) {
  if (mount(from.c_str(), to.c_str(), type.c_str(), kDefaultMountOptions,
            mount_options.c_str())) {
    return false;
  }
  return true;
}

bool Platform::Bind(const std::string& from, const std::string& to) {
  if (mount(from.c_str(), to.c_str(), NULL, kDefaultMountOptions | MS_BIND,
            NULL))
    return false;
  return true;
}

bool Platform::Unmount(const std::string& path, bool lazy, bool* was_busy) {
  if (lazy) {
    if (umount2(path.c_str(), MNT_DETACH)) {
      if (was_busy) {
        *was_busy = (errno == EBUSY);
      }
      return false;
    }
  } else {
    if (umount(path.c_str())) {
      if (was_busy) {
        *was_busy = (errno == EBUSY);
      }
      return false;
    }
  }
  if (was_busy) {
    *was_busy = false;
  }
  return true;
}

void Platform::GetProcessesWithOpenFiles(
    const std::string& path,
    std::vector<ProcessInformation>* processes) {
  std::vector<pid_t> pids;
  LookForOpenFiles(path, &pids);
  for (std::vector<pid_t>::iterator it = pids.begin(); it != pids.end(); ++it) {
    pid_t pid = static_cast<pid_t>(*it);
    processes->push_back(ProcessInformation());
    GetProcessOpenFileInformation(pid, path,
                                  &processes->at(processes->size() - 1));
  }
}

std::string Platform::ReadLink(const std::string& link_path) {
  char link_buf[PATH_MAX];
  ssize_t link_length = readlink(link_path.c_str(), link_buf, sizeof(link_buf));
  if (link_length > 0) {
    return std::string(link_buf, link_length);
  }
  return std::string();
}

void Platform::GetProcessOpenFileInformation(pid_t pid,
                                             const std::string& path_in,
                                             ProcessInformation* process_info) {
  process_info->set_process_id(pid);
  FilePath pid_path(StringPrintf("/proc/%d", pid));
  FilePath cmdline_file = pid_path.Append("cmdline");
  string contents;
  std::vector<std::string> cmd_line;
  if (file_util::ReadFileToString(cmdline_file, &contents)) {
    SplitString(contents, '\0', &cmd_line);
  }
  process_info->set_cmd_line(&cmd_line);

  // Make sure that if we get a directory, it has a trailing separator
  FilePath file_path(path_in);
  file_util::EnsureEndsWithSeparator(&file_path);
  std::string path = file_path.value();

  FilePath cwd_path = pid_path.Append("cwd");
  std::string link_val = ReadLink(cwd_path.value());
  if (IsPathChild(path, link_val)) {
    process_info->set_cwd(&link_val);
  } else {
    link_val.clear();
    process_info->set_cwd(&link_val);
  }

  // Open /proc/<pid>/fd
  FilePath fd_dirpath = pid_path.Append("fd");

  file_util::FileEnumerator fd_dir_enum(fd_dirpath, false,
                                        file_util::FileEnumerator::FILES);

  std::set<std::string> open_files;
  // List open file descriptors
  for (FilePath fd_path = fd_dir_enum.Next();
       !fd_path.empty();
       fd_path = fd_dir_enum.Next()) {
    link_val = ReadLink(fd_path.value());
    if (IsPathChild(path, link_val)) {
      open_files.insert(link_val);
    }
  }
  process_info->set_open_files(&open_files);
}

void Platform::LookForOpenFiles(const std::string& path_in,
                                std::vector<pid_t>* pids) {
  // Make sure that if we get a directory, it has a trailing separator
  FilePath file_path(path_in);
  file_util::EnsureEndsWithSeparator(&file_path);
  std::string path = file_path.value();

  // Open /proc
  file_util::FileEnumerator proc_dir_enum(FilePath(kProcDir), false,
      file_util::FileEnumerator::DIRECTORIES);

  int linkbuf_length = path.length();
  std::vector<char> linkbuf(linkbuf_length);

  // List PIDs in /proc
  for (FilePath pid_path = proc_dir_enum.Next();
       !pid_path.empty();
       pid_path = proc_dir_enum.Next()) {
    const char* pidstr = pid_path.BaseName().value().c_str();
    pid_t pid = 0;
    // Ignore PID 1 and errors
    if (!base::StringToInt(pidstr, &pid) || pid <= 1) {
      continue;
    }

    FilePath cwd_path = pid_path.Append("cwd");
    ssize_t link_length = readlink(cwd_path.value().c_str(),
                                   &linkbuf[0],
                                   linkbuf.size());
    if (link_length > 0) {
      std::string open_file(&linkbuf[0], link_length);
      if (IsPathChild(path, open_file)) {
        pids->push_back(pid);
        continue;
      }
    }

    // Open /proc/<pid>/fd
    FilePath fd_dirpath = pid_path.Append("fd");

    file_util::FileEnumerator fd_dir_enum(fd_dirpath, false,
                                          file_util::FileEnumerator::FILES);

    // List open file descriptors
    for (FilePath fd_path = fd_dir_enum.Next();
         !fd_path.empty();
         fd_path = fd_dir_enum.Next()) {
      link_length = readlink(fd_path.value().c_str(), &linkbuf[0],
                                     linkbuf.size());
      if (link_length > 0) {
        std::string open_file(&linkbuf[0], link_length);
        if (IsPathChild(path, open_file)) {
          pids->push_back(pid);
          break;
        }
      }
    }
  }
}

bool Platform::IsPathChild(const std::string& parent,
                           const std::string& child) {
  if (parent.length() == 0 || child.length() == 0) {
    return false;
  }
  if (child.length() >= parent.length()) {
    if (child.compare(0, parent.length(), parent, 0, parent.length()) == 0) {
      return true;
    }
  } else if ((parent[parent.length() - 1] == '/') &&
             (child.length() == (parent.length() - 1))) {
    if (child.compare(0, child.length(), parent, 0, parent.length() - 1) == 0) {
      return true;
    }
  }
  return false;
}

bool Platform::GetOwnership(const string& path,
                            uid_t* user_id, gid_t* group_id) const {
  struct stat path_status;
  if (stat(path.c_str(), &path_status) != 0) {
    PLOG(ERROR) << "stat() of " << path << " failed.";
    return false;
  }
  if (user_id)
    *user_id = path_status.st_uid;
  if (group_id)
    *group_id = path_status.st_gid;
  return true;
}

bool Platform::SetOwnership(const std::string& path, uid_t user_id,
                            gid_t group_id) const {
  if (chown(path.c_str(), user_id, group_id)) {
    PLOG(ERROR) << "chown() of " << path.c_str() << " to (" << user_id
                << "," << group_id << ") failed.";
    return false;
  }
  return true;
}

bool Platform::GetPermissions(const string& path, mode_t* mode) const {
  struct stat path_status;
  if (stat(path.c_str(), &path_status) != 0) {
    PLOG(ERROR) << "stat() of " << path << " failed.";
    return false;
  }
  *mode = path_status.st_mode;
  return true;
}

bool Platform::SetPermissions(const std::string& path, mode_t mode) const {
  if (chmod(path.c_str(), mode)) {
    PLOG(ERROR) << "chmod() of " << path.c_str() << " to (" << std::oct << mode
                << ") failed.";
    return false;
  }
  return true;
}

bool Platform::SetGroupAccessible(const string& path, gid_t group_id,
                                  mode_t group_mode) const {
  uid_t user_id;
  mode_t mode;
  if (!GetOwnership(path, &user_id, NULL) ||
      !GetPermissions(path, &mode) ||
      !SetOwnership(path, user_id, group_id) ||
      !SetPermissions(path, (mode & ~S_IRWXG) | (group_mode & S_IRWXG))) {
    LOG(ERROR) << "Couldn't set up group access on directory: " << path;
    return false;
  }
  return true;
}

int Platform::SetMask(int new_mask) const {
  return umask(new_mask);
}

bool Platform::GetUserId(const std::string& user, uid_t* user_id,
                         gid_t* group_id) const {
  // Load the passwd entry
  long user_name_length = sysconf(_SC_GETPW_R_SIZE_MAX);  // NOLINT long
  if (user_name_length == -1) {
    user_name_length = kDefaultPwnameLength;
  }
  struct passwd user_info, *user_infop;
  std::vector<char> user_name_buf(user_name_length);
  if (getpwnam_r(user.c_str(), &user_info, &user_name_buf[0],
                user_name_length, &user_infop)) {
    return false;
  }
  *user_id = user_info.pw_uid;
  *group_id = user_info.pw_gid;
  return true;
}

bool Platform::GetGroupId(const std::string& group, gid_t* group_id) const {
  // Load the group entry
  long group_name_length = sysconf(_SC_GETGR_R_SIZE_MAX);  // NOLINT long
  if (group_name_length == -1) {
    group_name_length = kDefaultPwnameLength;
  }
  struct group group_info, *group_infop;
  std::vector<char> group_name_buf(group_name_length);
  if (getgrnam_r(group.c_str(), &group_info, &group_name_buf[0],
                group_name_length, &group_infop)) {
    return false;
  }
  *group_id = group_info.gr_gid;
  return true;
}

int64 Platform::AmountOfFreeDiskSpace(const string& path) const {
  return base::SysInfo::AmountOfFreeDiskSpace(FilePath(path));
}

bool Platform::FileExists(const std::string& path) {
  return file_util::PathExists(FilePath(path));
}

bool Platform::DirectoryExists(const std::string& path) {
  return file_util::DirectoryExists(FilePath(path));
}

bool Platform::WriteFile(const std::string& path,
                         const chromeos::Blob& blob) {
  FilePath file_path(path);
  if (!file_util::DirectoryExists(file_path.DirName())) {
    if (!file_util::CreateDirectory(file_path.DirName())) {
      LOG(ERROR) << "Cannot create directory: " << file_path.DirName().value();
      return false;
    }
  }
  // chromeos::Blob::size_type is std::vector::size_type and is unsigned.
  if (blob.size() > static_cast<chromeos::Blob::size_type>(INT_MAX)) {
    LOG(ERROR) << "Cannot write to " << path
               << ". Blob is too large: " << blob.size() << " bytes.";
    return false;
  }

  int data_written = file_util::WriteFile(
      file_path,
      reinterpret_cast<const char*>(&blob[0]),
      blob.size());
  return data_written == static_cast<int>(blob.size());
}

bool Platform::ReadFile(const std::string& path, chromeos::Blob* blob) {
  int64 file_size;
  FilePath file_path(path);
  if (!file_util::PathExists(file_path)) {
    return false;
  }
  if (!file_util::GetFileSize(file_path, &file_size)) {
    LOG(ERROR) << "Could not get size of " << path;
    return false;
  }
  // Compare to the max of a signed integer.
  if (file_size > static_cast<int64>(INT_MAX)) {
    LOG(ERROR) << "File " << path << " is too large: "
               << file_size << " bytes.";
    return false;
  }
  chromeos::Blob buf(file_size);
  int data_read = file_util::ReadFile(file_path,
                                      reinterpret_cast<char*>(&buf[0]),
                                      file_size);
  // Cast is okay because of comparison to INT_MAX above.
  if (data_read != static_cast<int>(file_size)) {
    LOG(ERROR) << "Only read " << data_read << " of " << file_size << " bytes.";
    return false;
  }
  blob->swap(buf);
  return true;
}

bool Platform::ReadFileToString(const std::string& path, std::string* string) {
  return file_util::ReadFileToString(FilePath(path), string);
}

bool Platform::CreateDirectory(const std::string& path) {
  return file_util::CreateDirectory(FilePath(path));
}

bool Platform::DeleteFile(const std::string& path, bool is_recursive) {
  return file_util::Delete(FilePath(path), is_recursive);
}

bool Platform::EnumerateDirectoryEntries(const std::string& path,
                                         bool recursive,
                                         std::vector<std::string>* ent_list) {
  file_util::FileEnumerator::FileType ft = static_cast<typeof(ft)>(
    file_util::FileEnumerator::FILES | file_util::FileEnumerator::DIRECTORIES |
    file_util::FileEnumerator::SHOW_SYM_LINKS);
  file_util::FileEnumerator ent_enum(FilePath(path), recursive, ft);
  for (FilePath path = ent_enum.Next(); !path.empty(); path = ent_enum.Next())
    ent_list->push_back(path.value());
  return true;
}

base::Time Platform::GetCurrentTime() const {
  return base::Time::Now();
}

bool Platform::Stat(const std::string& path, struct stat *buf) {
  return lstat(path.c_str(), buf) == 0;
}

bool Platform::Rename(const std::string& from, const std::string& to) {
  return rename(from.c_str(), to.c_str()) == 0;
}

bool Platform::Copy(const std::string& from, const std::string& to) {
  FilePath from_path(from);
  FilePath to_path(to);
  return file_util::CopyDirectory(from_path, to_path, true);
}

}  // namespace chromeos
