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
const char kLpadminSeccompPolicy[] = "/usr/share/policy/lpadmin-seccomp.policy";
const char kTestPPDCommand[] = "/usr/bin/cupstestppd";
const char kTestPPDSeccompPolicy[] =
    "/usr/share/policy/cupstestppd-seccomp.policy";

const char kLpadminUser[] = "lpadmin";
const char kLpadminGroup[] = "lpadmin";
const char kLpGroup[] = "lp";
const char kDownloadsFolder[] = "Downloads";

// Max PPD size we'll allow, in bytes.
const int kMaxPPDSize = 250000;

// Temporary name for the ppd file, zipped and unzipped variants.
const char kTempPPDFileName[] = "tmp.ppd";
const char kGZippedTempPPDFileName[] = "tmp.ppd.gz";

// This pathname has to match the path generated in
// //chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.cc
const char kPPDCacheFolder[] = "PPDCache";

bool IsHexDigits(base::StringPiece digits) {
  for (char digit : digits) {
    if (!base::IsHexDigit(digit))
      return false;
  }
  return true;
}

// Determine whether or not the contents look like gzipped data by
// looking for the magic number in the header.
bool IsGZipped(const std::vector<uint8_t>& contents) {
  // Min header size is 10 bytes, min footer size is 8 bytes, so 18 bytes is a
  // lower bound on size for gzip contents.
  return contents.size() >= 18 && contents[0] == 0x1f && contents[1] == 0x8b;
}

bool MatchesUserString(base::StringPiece input) {
  std::vector<base::StringPiece> pieces = base::SplitStringPiece(
      input, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  return pieces.size() == 2 && pieces[0] == "u" && IsHexDigits(pieces[1]);
}

bool IsInPPDCache(const std::vector<std::string>& path_components) {
  // {/, home, chronos, u-HEX, PPDCache, <FileName>}
  return path_components.size() == 6 && path_components[0] == "/" &&
         path_components[1] == "home" && path_components[2] == "chronos" &&
         MatchesUserString(path_components[3]) &&
         path_components[4] == kPPDCacheFolder;
}

// Returns true if the path is a child of the primary user's Downloads folder.
bool MatchesPrimaryUserDownloads(
    const std::vector<std::string>& path_components) {
  std::vector<std::string> primary_user = {"/", "home", "chronos", "user",
                                           kDownloadsFolder};
  return path_components.size() >= primary_user.size() &&
         std::equal(primary_user.begin(), primary_user.end(),
                    path_components.begin());
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

bool IsDownload(const std::vector<std::string>& path_components) {
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
                       const std::string& seccomp_policy,
                       const ProcessWithOutput::ArgList& arg_list,
                       bool root_mount_ns, DBus::Error* error) {
  ProcessWithOutput process;
  process.set_separate_stderr(true);
  process.SandboxAs(user, group);

  if (!seccomp_policy.empty())
    process.SetSeccompFilterPolicyFile(seccomp_policy);

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

// Runs |command| as |user|:|group| with args |arg_list|.  If |seccomp_policy|
// is non-empty, apply it to restrict syscalls.  Returns the exit code
// for the executed process.
int RunAsUser(const std::string& user, const std::string& group,
              const std::string& command,
              const std::string& seccomp_policy,
              const ProcessWithOutput::ArgList& arg_list, DBus::Error* error) {
  return RunAsUserWithMount(user, group, command, seccomp_policy, arg_list,
                            false, error);
}

// Runs cupstestppd on |file_name| returns true if it is a valid ppd file.
bool TestPPD(const std::string& path, DBus::Error* error) {
  // TODO(skau): Run cupstestppd in seccomp crbug.com/633383.
  return RunAsUser(kLpadminUser, kLpadminGroup, kTestPPDCommand,
                   kTestPPDSeccompPolicy, {path}, error) == 0;
}

// Runs lpadmin with the provided |arg_list|.
int Lpadmin(const ProcessWithOutput::ArgList& arg_list, DBus::Error* error) {
  // TODO(skau): Run lpadmin in seccomp crbug.com/637160.
  // Run in lp group so we can read and write /run/cups/cups.sock.
  return RunAsUser(kLpadminUser, kLpGroup, kLpadminCommand,
                   kLpadminSeccompPolicy, arg_list, error);
}

// Write |contents| to |filename| in |temp_ppd_dir| and set permissions so
// it is usable by lpadmin to validate and install the ppd.
//
// Note that |temp_ppd_dir|'s parents must be readable by lpadmin, and
// |temp_ppd_dir| should be dedicated to this particular purpose as we chown it
// to lpadmin.
//
// Returns true and fills in temp_ppd_file with the full path to the written
// file if the write is successful.
bool WriteTempPPDFile(const base::FilePath& temp_ppd_dir,
                      const std::vector<uint8_t>& contents,
                      base::FilePath* temp_ppd_file,
                      DBus::Error* error) {
  *temp_ppd_file = temp_ppd_dir.Append(
      IsGZipped(contents) ? kGZippedTempPPDFileName : kTempPPDFileName);
  if (!base::WriteFile(*temp_ppd_file,
                       reinterpret_cast<const char*>(contents.data()),
                       contents.size())) {
    LOG(ERROR) << "Could not write to temporary file "
               << temp_ppd_file->value();
    return false;
  }

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
  if (!SetPerms(temp_ppd_dir, 0750, uid, gid)) {
    PLOG(FATAL) << "Could not change directory permissions";
    return false;
  }
  if (!SetPerms(*temp_ppd_file, 0640, uid, gid)) {
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

  std::vector<std::string> path_components;
  path.GetComponents(&path_components);

  if (!IsInPPDCache(path_components) && !IsDownload(path_components)) {
    LOG(ERROR) << "Illegal path " << path.value();
    return false;
  }

  return true;
}

}  // namespace

// Invokes lpadmin with arguments to configure a new printer.  For IPP
// printers, it can attempt autoconf using '-m everywhere'.
bool CupsTool::AddPrinter(const std::string& name, const std::string& uri,
                          const std::string& ppd_file, bool ipp_everywhere,
                          DBus::Error* error) {
  if (ipp_everywhere)
    return AddAutoConfiguredPrinter(name, uri, error);

  if (ppd_file.empty()) {
    LOG(ERROR) << "A ppd path must be provided";
    return false;
  }

  base::FilePath src_ppd(ppd_file);
  if (!ValidatePPDPath(src_ppd))
    return false;

  // Read the file contents into memory and hand it off to
  // AddManuallyConfiguredPrinter() to do the real work.
  std::string ppd_contents;

  if (!ReadFileToStringWithMaxSize(src_ppd, &ppd_contents, kMaxPPDSize)) {
    LOG(ERROR) << "Failed to read " << ppd_file;
    return false;
  }

  return AddManuallyConfiguredPrinter(
      name,
      uri,
      std::vector<uint8_t>(ppd_contents.begin(), ppd_contents.end()),
      error);
}

// Invokes lpadmin with arguments to configure a new printer using '-m
// everywhere'.
int32_t CupsTool::AddAutoConfiguredPrinter(const std::string& name,
                                           const std::string& uri,
                                           DBus::Error* error) {
  // Autoconfiguration requires ipp or ipps.
  if (!base::StartsWith(uri, "ipp://", base::CompareCase::INSENSITIVE_ASCII) &&
      !base::StartsWith(uri, "ipps://", base::CompareCase::INSENSITIVE_ASCII)) {
    LOG(WARNING) << "IPP or IPPS required for IPP Everywhere: " << uri;
    return false;
  }

  bool success = (Lpadmin({"-v", uri, "-p", name, "-m", "everywhere", "-E"},
                          error) == EXIT_SUCCESS);
  if (success) {
    return 0;
  }
  // TODO(skau) - Use this channel to return better failure code information.
  return 1;
}

int32_t CupsTool::AddManuallyConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    const std::vector<uint8_t>& ppd_contents,
    DBus::Error* error) {
  // Scoped temp dir will cleanup the directory when we're done.
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    LOG(ERROR) << "Could not create temp directory";
    return false;
  }

  base::FilePath temp_ppd;
  if (!WriteTempPPDFile(temp_dir.path(), ppd_contents, &temp_ppd, error)) {
    return false;
  }

  if (!TestPPD(temp_ppd.value(), error)) {
    LOG(ERROR) << "PPD failed validation";
    return false;
  }
  bool success = (Lpadmin({"-v", uri, "-p", name, "-P", temp_ppd.value(), "-E"},
                          error) == EXIT_SUCCESS);
  if (success) {
    return 0;
  }
  // TODO(skau) - Use this channel for better error reporting.
  return 1;
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
