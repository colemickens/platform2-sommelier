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
#include <chromeos/dbus/debugd/dbus-constants.h>

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

// Temporary name for the ppd file, zipped and unzipped variants.
const char kTempPPDFileName[] = "tmp.ppd";
const char kGZippedTempPPDFileName[] = "tmp.ppd.gz";

// Determine whether or not the contents look like gzipped data by
// looking for the magic number in the header.
bool IsGZipped(const std::vector<uint8_t>& contents) {
  // Min header size is 10 bytes, min footer size is 8 bytes, so 18 bytes is a
  // lower bound on size for gzip contents.
  return contents.size() >= 18 && contents[0] == 0x1f && contents[1] == 0x8b;
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

int StopCups() {
  int result;

  result = ProcessWithOutput::RunProcess("initctl", {"stop", kJobName},
                                         true,     // requires root
                                         nullptr,  // stdin
                                         nullptr,  // stdout
                                         nullptr,  // stderr
                                         nullptr);

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

// Returns the exit code for the executed process.
// By default disallow root mount namespace. Passing true as optional argument
// enables root mount namespace.
int RunAsUser(const std::string& user, const std::string& group,
              const std::string& command,
              const std::string& seccomp_policy,
              const ProcessWithOutput::ArgList& arg_list,
              bool root_mount_ns = false,
              bool inherit_usergroups = false) {
  ProcessWithOutput process;
  process.set_separate_stderr(true);
  process.SandboxAs(user, group);

  if (!seccomp_policy.empty())
    process.SetSeccompFilterPolicyFile(seccomp_policy);

  if (root_mount_ns)
    process.AllowAccessRootMountNamespace();

  if (inherit_usergroups)
    process.InheritUsergroups();

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
    PLOG(ERROR) << "Child process failed" << error_msg;
  }

  return result;
}

// Runs cupstestppd on |file_name| returns the result code.  0 is the expected
// success code.
int TestPPD(const std::string& path) {
  return RunAsUser(kLpadminUser, kLpadminGroup, kTestPPDCommand,
                   kTestPPDSeccompPolicy, {path},
                   true /* root_mount_ns */);
}

// Runs lpadmin with the provided |arg_list|.
int Lpadmin(const ProcessWithOutput::ArgList& arg_list,
            bool inherit_usergroups = false) {
  // Run in lp group so we can read and write /run/cups/cups.sock.
  return RunAsUser(kLpadminUser, kLpGroup, kLpadminCommand,
                   kLpadminSeccompPolicy, arg_list, false, inherit_usergroups);
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
                      base::FilePath* temp_ppd_file) {
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

// Checks whether the scheme for the given |uri| is one of the required schemes
// for IPP Everywhere.
bool IppEverywhereURI(const std::string& uri) {
  static const char* const kValidSchemes[] = {"ipp://", "ipps://", "ippusb://"};
  for (const char* scheme : kValidSchemes) {
    if (base::StartsWith(uri, scheme, base::CompareCase::INSENSITIVE_ASCII))
      return true;
  }

  return false;
}

}  // namespace

// Invokes lpadmin with arguments to configure a new printer using '-m
// everywhere'.  Returns 0 for success, 1 for failure, 4 if configuration
// failed.
int32_t CupsTool::AddAutoConfiguredPrinter(const std::string& name,
                                           const std::string& uri) {
  if (!IppEverywhereURI(uri)) {
    LOG(WARNING) << "IPP, IPPS or IPPUSB required for IPP Everywhere: " << uri;
    return CupsResult::CUPS_FATAL;
  }

  int32_t result;
  if (base::StartsWith(uri, "ippusb://",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    // In the case of printing with the ippusb scheme, we want to run lpadmin in
    // a minijail with the inherit usergroups option set.
    result = Lpadmin({"-v", uri, "-p", name, "-m", "everywhere", "-E"}, true);
  } else {
    result = Lpadmin({"-v", uri, "-p", name, "-m", "everywhere", "-E"});
  }

  if (result != EXIT_SUCCESS) {
    return CupsResult::CUPS_AUTOCONF_FAILURE;
  }

  return CupsResult::CUPS_SUCCESS;
}

int32_t CupsTool::AddManuallyConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    const std::vector<uint8_t>& ppd_contents) {
  // Scoped temp dir will cleanup the directory when we're done.
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    LOG(ERROR) << "Could not create temp directory";
    return CupsResult::CUPS_FATAL;
  }

  base::FilePath temp_ppd;
  if (!WriteTempPPDFile(temp_dir.GetPath(), ppd_contents, &temp_ppd)) {
    return CupsResult::CUPS_FATAL;
  }

  int result = TestPPD(temp_ppd.value());
  if (result != EXIT_SUCCESS) {
    LOG(ERROR) << "PPD failed validation";
    return CupsResult::CUPS_INVALID_PPD;
  }

  // lpadmin only returns 0 for success and 1 for failure.
  result = Lpadmin({"-v", uri, "-p", name, "-P", temp_ppd.value(), "-E"});
  if (result != EXIT_SUCCESS) {
    return CupsResult::CUPS_LPADMIN_FAILURE;
  }

  return CupsResult::CUPS_SUCCESS;
}

// Invokes lpadmin with -x to delete a printer.
bool CupsTool::RemovePrinter(const std::string& name) {
  return Lpadmin({"-x", name}) == EXIT_SUCCESS;
}

// Stop cupsd and clear its state.  Needs to launch helper with root
// permissions, so we can restart Upstart jobs, and clear privileged
// directories.
void CupsTool::ResetState() {
  StopCups();

  // There's technically a race -- cups can be restarted in the meantime -- but
  // (a) we don't expect applications to be racing with this (e.g., this method
  // may be used on logout or login) and
  // (b) clearing CUPS's state while it's running should at most confuse CUPS
  // (e.g., missing printers or jobs).
  ClearCupsState();
}

}  // namespace debugd
