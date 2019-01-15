// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tool to manipulate CUPS.
#include "debugd/src/cups_tool.h"

#include <errno.h>
#include <ftw.h>
#include <signal.h>
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

int StopCups() {
  int result;

  result = ProcessWithOutput::RunProcess("initctl", {"stop", kJobName},
                                         true,     // requires root
                                         false,    // disable_sandbox
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
              const std::vector<uint8_t>* std_input = nullptr,
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

  // Prepares a buffer with standard input.
  std::vector<char> buf;
  if (std_input != nullptr) {
    buf.reserve(std_input->size());
    for (uint8_t byte : *std_input) {
      buf.push_back(static_cast<char>(byte));
    }
  }

  // Starts a process, writes data from the buffer to its standard input and
  // waits for the process to finish.
  int result = ProcessWithOutput::kRunError;
  process.RedirectUsingPipe(STDIN_FILENO, true);
  if (process.Start()) {
    int stdin_fd = process.GetPipe(STDIN_FILENO);
    // Kill the process if writing to or closing the pipe fails.
    if (!base::WriteFileDescriptor(stdin_fd, buf.data(), buf.size()) ||
        IGNORE_EINTR(close(stdin_fd)) < 0) {
      process.Kill(SIGKILL, 0);
    }
    result = process.Wait();
  }

  if (result != 0) {
    std::string error_msg;
    process.GetError(&error_msg);
    PLOG(ERROR) << "Child process failed" << error_msg;
  }

  return result;
}

// Runs cupstestppd on |ppd_content| returns the result code.  0 is the expected
// success code.
int TestPPD(const std::vector<uint8_t> & ppd_content) {
  return RunAsUser(kLpadminUser, kLpadminGroup, kTestPPDCommand,
                   kTestPPDSeccompPolicy, {"-"}, &(ppd_content),
                   true /* root_mount_ns */);
}

// Runs lpadmin with the provided |arg_list| and |std_input|.
int Lpadmin(const ProcessWithOutput::ArgList& arg_list,
            bool inherit_usergroups = false,
            const std::vector<uint8_t>* std_input = nullptr) {
  // Run in lp group so we can read and write /run/cups/cups.sock.
  return RunAsUser(kLpadminUser, kLpGroup, kLpadminCommand,
                   kLpadminSeccompPolicy, arg_list, std_input,
                   false, inherit_usergroups);
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

// Evaluate true when a URI begins with a known scheme and has trailing
// characters (i.e. is not an empty URI).
bool UriHasKnownScheme(const std::string& uri) {
  // Enumerate known printing URIs. Values are lifted from Chrome browser's
  // Printer::GetProtocol().
  const std::vector<std::string> known_schemes = {
    "usb://",
    "ipp://",
    "ipps://",
    "http://",
    "https://",
    "socket://",
    "lpd://",
    "ippusb://"
  };

  for (const std::string& scheme : known_schemes) {
    if (base::StartsWith(uri, scheme, base::CompareCase::INSENSITIVE_ASCII) &&
        (scheme.length() < uri.length())) {
      return true;
    }
  }
  return false;
}

// Determine whether a URI comprises mostly alphanumeric ASCII.
// Logic mirrors Chrome browser's CupsURIEscape.
bool UriIsGoodAscii(const std::string& uri) {
  for (const char c : uri) {
    if ((c == ' ') || (c == '%') || (c & 0x80)) {
      return false;
    }
  }
  return true;
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

  if (!CupsTool::UriSeemsReasonable(uri)) {
    LOG(WARNING) << "Invalid URI: " << uri;
    return CupsResult::CUPS_BAD_URI;
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

  int result = TestPPD(ppd_contents);
  if (result != EXIT_SUCCESS) {
    LOG(ERROR) << "PPD failed validation";
    return CupsResult::CUPS_INVALID_PPD;
  }

  if (!CupsTool::UriSeemsReasonable(uri)) {
    LOG(WARNING) << "Invalid URI: " << uri;
    return CupsResult::CUPS_BAD_URI;
  }

  // lpadmin only returns 0 for success and 1 for failure.
  result = Lpadmin({"-v", uri, "-p", name, "-P", "-", "-E"}, false,
                   &ppd_contents);
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

// Check if a URI starts with a known scheme and comprises only
// non-whitespace ASCII.
bool CupsTool::UriSeemsReasonable(const std::string& uri) {
  return UriHasKnownScheme(uri) && UriIsGoodAscii(uri);
}

}  // namespace debugd
