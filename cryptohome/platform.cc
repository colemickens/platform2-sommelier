// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Platform

#include "platform.h"

#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/file_util.h>
#include <base/string_util.h>

// Included last to avoid redefinition problems
extern "C" {
#include <keyutils.h>
}

using std::string;

namespace cryptohome {

const int kDefaultMountOptions = MS_NOEXEC | MS_NOSUID | MS_NODEV;
const int kDefaultPwnameLength = 1024;
const int kDefaultUmask = S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH
                               | S_IXOTH;
const std::string kMtab = "/etc/mtab";
const std::string kProcDir = "/proc";

Platform::Platform()
    : mount_options_(kDefaultMountOptions),
      umask_(kDefaultUmask),
      mtab_file_(kMtab),
      proc_dir_(kProcDir) {
}

Platform::~Platform() {
}

bool Platform::IsDirectoryMounted(const std::string& directory) {
  // Trivial string match from /etc/mtab to see if the cryptohome mount point is
  // listed.  This works because Chrome OS is a controlled environment and the
  // only way /home/chronos/user should be mounted is if cryptohome mounted it.
  string contents;
  if (file_util::ReadFileToString(FilePath(mtab_file_), &contents)) {
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
  if (file_util::ReadFileToString(FilePath(mtab_file_), &contents)) {
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
  if (mount(from.c_str(), to.c_str(), type.c_str(), mount_options_,
            mount_options.c_str())) {
    return false;
  }
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

bool Platform::TerminatePidsWithOpenFiles(const std::string& path, bool hard) {
  std::vector<pid_t> pids;
  LookForOpenFiles(path, &pids);
  for (std::vector<pid_t>::iterator it = pids.begin(); it != pids.end(); it++) {
    pid_t pid = static_cast<pid_t>(*it);
    if (pid != getpid()) {
      if (hard) {
        kill(pid, SIGTERM);
      } else {
        kill(pid, SIGKILL);
      }
    }
  }
  return (pids.size() != 0);
}

void Platform::GetProcessesWithOpenFiles(
    const std::string& path,
    std::vector<ProcessInformation>* processes) {
  std::vector<pid_t> pids;
  LookForOpenFiles(path, &pids);
  for (std::vector<pid_t>::iterator it = pids.begin(); it != pids.end(); it++) {
    pid_t pid = static_cast<pid_t>(*it);
    processes->push_back(ProcessInformation());
    processes->at(processes->size() - 1).process_id = pid;
    FilePath cmdline_file(StringPrintf("/proc/%d/cmdline", pid));
    string contents;
    if (!file_util::ReadFileToString(cmdline_file, &contents)) {
      continue;
    }
    processes->at(processes->size() - 1).cmd_line.swap(contents);
  }
}

void Platform::LookForOpenFiles(const std::string& path_in,
                                std::vector<pid_t>* pids) {
  // Make sure that if we get a directory, it has a trailing separator
  FilePath file_path(path_in);
  file_util::EnsureEndsWithSeparator(&file_path);
  std::string path = file_path.value();
  bool is_directory = file_util::EndsWithSeparator(file_path);
  std::string dir_path = (is_directory
                          ? path.substr(0, path.length() - 1)
                          : "");

  // Open /proc
  file_util::FileEnumerator proc_dir_enum(FilePath(proc_dir_), false,
      file_util::FileEnumerator::DIRECTORIES);

  int linkbuf_length = path.length();
  std::vector<char> linkbuf(linkbuf_length);

  // List PIDs in /proc
  FilePath pid_path;
  while (!(pid_path = proc_dir_enum.Next()).empty()) {
    pid_t pid = static_cast<pid_t>(atoi(pid_path.BaseName().value().c_str()));
    // Ignore PID 1 and errors
    if (pid <= 1) {
      continue;
    }
    // Open /proc/<pid>/fd
    FilePath fd_dirpath = pid_path.Append("fd");

    file_util::FileEnumerator fd_dir_enum(fd_dirpath, false,
                                          file_util::FileEnumerator::FILES);

    // List open file descriptors
    FilePath fd_path;
    while (!(fd_path = fd_dir_enum.Next()).empty()) {
      ssize_t link_length = readlink(fd_path.value().c_str(), &linkbuf[0],
                                     linkbuf.size());
      if (link_length > 0) {
        std::string open_file(&linkbuf[0], link_length);
        if (open_file.length() >= path.length()) {
          if (open_file.substr(0, path.length()).compare(path) == 0) {
            pids->push_back(pid);
            break;
          }
        } else if (is_directory && open_file.length() == dir_path.length()) {
          if (open_file.compare(dir_path) == 0) {
            pids->push_back(pid);
            break;
          }
        }
      }
    }
  }
}

bool Platform::TerminatePidsForUser(const uid_t uid, bool hard) {
  std::vector<pid_t> pids;
  GetPidsForUser(uid, &pids);
  for (std::vector<pid_t>::iterator it = pids.begin(); it != pids.end(); it++) {
    pid_t pid = static_cast<pid_t>(*it);
    if (pid != getpid()) {
      if (hard) {
        kill(pid, SIGTERM);
      } else {
        kill(pid, SIGKILL);
      }
    }
  }
  return (pids.size() != 0);
}

void Platform::GetPidsForUser(uid_t uid, std::vector<pid_t>* pids) {

  // Open /proc
  file_util::FileEnumerator proc_dir_enum(FilePath(proc_dir_), false,
      file_util::FileEnumerator::DIRECTORIES);


  // List PIDs in /proc
  FilePath pid_path;
  while (!(pid_path = proc_dir_enum.Next()).empty()) {
    pid_t pid = static_cast<pid_t>(atoi(pid_path.BaseName().value().c_str()));
    if (pid <= 1) {
      continue;
    }
    // Open /proc/<pid>/status
    FilePath status_path = pid_path.Append("status");
    string contents;
    if (!file_util::ReadFileToString(status_path, &contents)) {
      continue;
    }

    size_t uid_loc = contents.find("Uid:");
    if (!uid_loc) {
      continue;
    }
    uid_loc += 4;

    size_t uid_end = contents.find("\n", uid_loc);
    if (!uid_end) {
      continue;
    }

    contents = contents.substr(uid_loc, uid_end - uid_loc);

    std::vector<std::string> tokens;
    Tokenize(contents, " \t", &tokens);

    for (std::vector<std::string>::iterator it = tokens.begin();
        it != tokens.end(); it++) {
      std::string& value = *it;
      if (value.length() == 0) {
        continue;
      }
      uid_t check_uid = static_cast<uid_t>(atoi(value.c_str()));
      if (check_uid == uid) {
        pids->push_back(pid);
        break;
      }
    }
  }
}

bool Platform::SetOwnership(const std::string& directory, uid_t user_id,
                            gid_t group_id) {
  if (chown(directory.c_str(), user_id, group_id)) {
    return false;
  }
  return true;
}

int Platform::SetMask(int new_mask) {
  return umask(new_mask);
}

bool Platform::GetUserId(const std::string& user, uid_t* user_id,
                         gid_t* group_id) {
  // Load the passwd entry
  long user_name_length = sysconf(_SC_GETPW_R_SIZE_MAX);
  if(user_name_length == -1) {
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

void Platform::ClearUserKeyring() {
  keyctl(KEYCTL_CLEAR, KEY_SPEC_USER_KEYRING);
}

} // namespace cryptohome
