// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tool to manipulate CUPS.
#include "debugd/src/cups_tool.h"

#include <errno.h>
#include <ftw.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <base/strings/string_piece.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <brillo/flag_helper.h>
#include <brillo/userdb_utils.h>

#include "debugd/src/constants.h"
#include "debugd/src/process_with_output.h"

namespace debugd {

namespace {

const char kJobName[] = "cupsd";
const char kLpadminCommand[] = "/usr/sbin/lpadmin";
const char kTestPPDCommand[] = "/usr/bin/cupstestppd";

const char kLpadminUser[] = "lpadmin";
const char kLpadminGroup[] = "lpadmin";
const char kLpGroup[] = "lp";
const char kDownloadsFolder[] = "Downloads";

bool IsInPPDCache(const base::FilePath& path) {
  // TODO(skau): Implement once cache location is determined.
  return false;
}

// Returns true if the path is a child of the primary user's Downloads folder.
bool MatchesPrimaryUserDownloads(
    const std::vector<std::string>& path_components) {
  std::vector<std::string> primary_user = {"/", "home", "chronos", "user",
                                           kDownloadsFolder};
  return std::equal(primary_user.begin(), primary_user.end(),
                    path_components.begin());
}

bool IsHexDigits(base::StringPiece digits) {
  for (char digit : digits) {
    if (!base::IsHexDigit(digit))
      return false;
  }

  return true;
}

bool MatchesUserString(base::StringPiece input) {
  std::vector<base::StringPiece> pieces = base::SplitStringPiece(
      input, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  return pieces.size() == 2 && pieces[0] == "u" && IsHexDigits(pieces[1]);
}

// Matches the path reported by Chrome for the Downloads folder.
bool MatchesChromeDownloadsPath(
    const std::vector<std::string>& path_components) {
  return path_components.size() > 5 && path_components[0] == "/" &&
         path_components[1] == "home" && path_components[2] == "chronos" &&
         MatchesUserString(path_components[3]) &&
         path_components[4] == kDownloadsFolder;
}

// Returns true if the path is a child of the user's Downloads folder in a
// multi-user environment.
bool MatchesMultiUserDownloads(
    const std::vector<std::string>& path_components) {
  // Path consists of /home/user/<hex string>/Downloads.
  return path_components.size() > 5 && path_components[0] == "/" &&
         path_components[1] == "home" && path_components[2] == "user" &&
         IsHexDigits(path_components[3]) &&
         path_components[4] == kDownloadsFolder;
}

bool IsDownload(const base::FilePath& path) {
  std::vector<std::string> path_components;
  path.GetComponents(&path_components);

  return MatchesChromeDownloadsPath(path_components) ||
         MatchesPrimaryUserDownloads(path_components) ||
         MatchesMultiUserDownloads(path_components);
}

bool SetPerms(const base::FilePath& fpath, mode_t mode, uid_t uid, gid_t gid) {
  const std::string& path = fpath.value();
  if (!base::SetPosixFilePermissions(fpath, mode)) {
    PLOG(ERROR) << "Could not modify file permissions";
    return false;
  }

  if (chown(path.c_str(), uid, gid) != 0) {
    PLOG(ERROR) << "Could not take ownership of file";
    return false;
  }

  return true;
}

int StopCups(DBus::Error* error) {
  int result;

  result = ProcessWithOutput::RunProcess("initctl", {"stop", kJobName},
                                         true,     // requires root
                                         nullptr,  // stdin
                                         nullptr,  // stdout
                                         nullptr,  // stderr
                                         error);

  // Don't log errors, since job may not be started.
  return result;
}

int RemovePath(const char* fpath, const struct stat* sb, int typeflag,
               struct FTW* ftwbuf) {
  int ret = remove(fpath);
  if (ret)
    PLOG(WARNING) << "could not remove file/path: " << fpath;

  return ret;
}

int ClearDirectory(const char* path) {
  if (access(path, F_OK) && errno == ENOENT)
    // Directory doesn't exist.  Skip quietly.
    return 0;

  return nftw(path, RemovePath, FTW_D, FTW_DEPTH | FTW_PHYS);
}

int ClearCupsState() {
  int ret = 0;

  ret |= ClearDirectory("/var/cache/cups");
  ret |= ClearDirectory("/var/spool/cups");

  return ret;
}

// Returns the exit code for the executed process.  Supports RunAsUser except
// that access to the root mount namespace is enabled if |root_mount_ns| is
// true.
int RunAsUserWithMount(const std::string& user, const std::string& group,
              const std::string& command,
              const ProcessWithOutput::ArgList& arg_list, bool root_mount_ns,
              DBus::Error* error) {
  ProcessWithOutput process;
  process.set_separate_stderr(true);
  process.SandboxAs(user, group);
  if (root_mount_ns)
    process.AllowAccessRootMountNamespace();

  if (!process.Init())
    return ProcessWithOutput::kRunError;

  process.AddArg(command);
  for (const std::string& arg : arg_list) {
    process.AddArg(arg);
  }
  int result = process.Run();

  if (result != 0) {
    std::string error_msg;
    process.GetError(&error_msg);
    PLOG(ERROR) << error_msg;
  }

  return result;
}

// Runs |command| as |user|:|group| with args |arg_list|.  Returns the exit code
// for the executed process.
int RunAsUser(const std::string& user, const std::string& group,
              const std::string& command,
              const ProcessWithOutput::ArgList& arg_list, DBus::Error* error) {
  return RunAsUserWithMount(user, group, command, arg_list, false, error);
}

// Runs cupstestppd on |file_name| returns true if it is a valid ppd file.
bool TestPPD(const std::string& path, DBus::Error* error) {
  // TODO(skau): Run cupstestppd in seccomp crbug.com/633383.
  return RunAsUser(kLpadminUser, kLpadminGroup, kTestPPDCommand, {path},
                   error) == 0;
}

// Runs lpadmin with the provided |arg_list|.
int Lpadmin(const ProcessWithOutput::ArgList& arg_list, DBus::Error* error) {
  // TODO(skau): Run lpadmin in seccomp crbug.com/637160.
  // Run in lp group so we can read and write /var/run/cups.sock.
  return RunAsUser(kLpadminUser, kLpGroup, kLpadminCommand, arg_list, error);
}

// Runs /bin/cp in a new minijail.  This is done becuase /home might not be
// mounted in our current minijail.  Returns true if successful.
bool RunCopy(const std::string& src, const std::string& dst,
             DBus::Error* error) {
  std::string err_msg;
  int result =
      RunAsUserWithMount("root", "root", "/bin/cp",
                         {"-u", src, dst},  // update, source, destination
                         true,  // access to root mount namespace required
                         error);

  if (result != 0) {
    LOG(ERROR) << "Could not copy file src(" << src << "), "
               << "dst(" << dst << ")";
    return false;
  }

  return true;
}

// Copies the file at |src_path| to |temp_dir_path| and fixes the permissions.
// Returns true if the file was copied successfully.
bool CopyPPDFile(const base::FilePath& src_path,
                 const base::FilePath& temp_dir_path, DBus::Error* error) {
  const base::FilePath destination = temp_dir_path.Append(src_path.BaseName());
  if (!RunCopy(src_path.value(), destination.value(), error))
    return false;

  // Transfer file ownership.
  uid_t uid;
  gid_t gid = 0;

  // Set ownership to lpadmin:lpadmin so cupstestppd and lpadmin can use the
  // file appropriately.  root:lpadmin doesn't work because lpadmin is run as
  // lpadmin:lp.
  if (!brillo::userdb::GetUserInfo(kLpadminUser, &uid, nullptr) ||
      !brillo::userdb::GetGroupInfo(kLpadminGroup, &gid)) {
    PLOG(ERROR) << "Could not retrieve owner information";
    return false;
  }

  if (!SetPerms(temp_dir_path, 0750, uid, gid)) {
    PLOG(FATAL) << "Could not change directory permissions";
    return false;
  }

  if (!SetPerms(destination, 0640, uid, gid)) {
    PLOG(FATAL) << "Could not change file permissions";
    return false;
  }

  return true;
}

// Returns true if the path is one from which we allow copying.  i.e. user's
// Downloads directories or the PPD cache.
bool ValidatePPDPath(const base::FilePath& path) {
  if (!path.IsAbsolute()) {
    LOG(ERROR) << "Path must be absolute " << path.value();
    return false;
  }

  if (path.ReferencesParent()) {
    LOG(ERROR) << "Parent references are not allowed " << path.value();
    return false;
  }

  if (!IsInPPDCache(path) && !IsDownload(path)) {
    LOG(ERROR) << "Illegal path " << path.value();
    return false;
  }

  return true;
}

bool ConfigurePrinter(const std::string& name, const std::string& uri,
                      const std::string& ppd_file, DBus::Error* error) {
  if (ppd_file.empty()) {
    LOG(ERROR) << "A ppd path must be provided";
    return false;
  }

  base::FilePath src_ppd(ppd_file);
  if (!ValidatePPDPath(src_ppd))
    return false;

  // Scoped temp dir will cleanup the directory when we're done.
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    PLOG(ERROR) << "Could not create temp directory";
    return false;
  }

  const base::FilePath& temp_dir_path = temp_dir.path();
  if (!CopyPPDFile(src_ppd, temp_dir_path, error))
    return false;

  base::FilePath copied_ppd = temp_dir_path.Append(src_ppd.BaseName());
  int result = EXIT_FAILURE;
  if (TestPPD(copied_ppd.value(), error))
    result =
        Lpadmin({"-v", uri, "-p", name, "-P", copied_ppd.value(), "-E"}, error);

  return result == EXIT_SUCCESS;
}

bool AutoConfigurePrinter(const std::string& name, const std::string& uri,
                          DBus::Error* error) {
  // Autoconfiguration requires ipp or ipps.
  if (!base::StartsWith(uri, "ipp://", base::CompareCase::INSENSITIVE_ASCII) &&
      !base::StartsWith(uri, "ipps://", base::CompareCase::INSENSITIVE_ASCII)) {
    LOG(WARNING) << "IPP or IPPS required for IPP Everywhere: " << uri;
    return false;
  }

  return Lpadmin({"-v", uri, "-p", name, "-m", "everywhere", "-E"}, error) == 0;
}

}  // namespace

// Invokes lpadmin with arguments to configure a new printer.  For IPP
// printers, it can attempt autoconf using '-m everywhere'.
bool CupsTool::AddPrinter(const std::string& name, const std::string& uri,
                          const std::string& ppd_file, bool ipp_everywhere,
                          DBus::Error* error) {
  if (ipp_everywhere)
    return AutoConfigurePrinter(name, uri, error);

  return ConfigurePrinter(name, uri, ppd_file, error);
}

// Invokes lpadmin with -x to delete a printer.
bool CupsTool::RemovePrinter(const std::string& name, DBus::Error* error) {
  return Lpadmin({"-x", name}, error) == 0;
}

// Stop cupsd and clear its state.  Needs to launch helper with root
// permissions, so we can restart Upstart jobs, and clear privileged
// directories.
void CupsTool::ResetState(DBus::Error* error) {
  // Ignore errors; CUPS may not even be started.
  StopCups(error);

  // There's technically a race -- cups can be restarted in the meantime -- but
  // (a) we don't expect applications to be racing with this (e.g., this method
  // may be used on logout or login) and
  // (b) clearing CUPS's state while it's running should at most confuse CUPS
  // (e.g., missing printers or jobs).
  ClearCupsState();
}

}  // namespace debugd
