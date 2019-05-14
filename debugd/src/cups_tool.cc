// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tool to manipulate CUPS.
#include "debugd/src/cups_tool.h"

#include <signal.h>
#include <unistd.h>

#include <cstddef>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_piece.h>
#include <base/strings/string_util.h>
#include <chromeos/dbus/debugd/dbus-constants.h>

#include "debugd/src/constants.h"
#include "debugd/src/process_with_output.h"

namespace debugd {

namespace {

constexpr char kLpadminCommand[] = "/usr/sbin/lpadmin";
constexpr char kLpadminSeccompPolicy[] =
    "/usr/share/policy/lpadmin-seccomp.policy";
constexpr char kTestPPDCommand[] = "/usr/bin/cupstestppd";
constexpr char kTestPPDSeccompPolicy[] =
    "/usr/share/policy/cupstestppd-seccomp.policy";

constexpr char kLpadminUser[] = "lpadmin";
constexpr char kLpadminGroup[] = "lpadmin";
constexpr char kLpGroup[] = "lp";

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

// Evaluates true when |c| is in the reserved set specified in RFC 3986.
bool CharIsReserved(char c) {
  switch (c) {
    case ':':
    case '/':
    case '?':
    case '#':
    case '[':
    case ']':
    case '@':
    case '!':
    case '$':
    case '&':
    case '\'':
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
    case ';':
    case '=':
      return true;
  }
  return false;
}

// Evaluates true when |c| is in the unreserved set specified in RFC 3986.
bool CharIsUnreserved(char c) {
  switch (c) {
    case '-':
    case '.':
    case '_':
    case '~':
      return true;
  }
  return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c);
}

// Evaluates true when |c|
// *  is in the reserved set specified by RFC 3986,
// *  is in the unreserved set specified by RFC 3986, or
// *  is a literal '%' (which is in neither preceding set).
bool CharIsAllowed(char c) {
  return CharIsReserved(c) || CharIsUnreserved(c) || c == '%';
}

// Determines whether |uri| comprises mostly alphanumeric ASCII.
bool UriIsGoodAscii(const std::string& uri) {
  int expected_hex_digits = 0;
  for (const char c : uri) {
    if (expected_hex_digits > 0) {
      // We are currently processing a percent-encoded segment "%XY."
      if (!base::IsHexDigit(c)) {
        return false;
      }
      expected_hex_digits--;
    } else if (c == '%') {
      // We are not currently processing a percent-encoded segment and
      // we see the start of a percent-encoded segment.
      expected_hex_digits = 2;
    } else if (!CharIsAllowed(c)) {
      return false;
    }
  }
  return expected_hex_digits == 0;
}

// Gets the starting index of the authority from |uri_view| - that is,
// returns the index following "://". If none is found, returns npos.
// Caller must ensure |uri_view| is already properly percent-encoded.
size_t UriAuthorityStartIndex(base::StringPiece uri_view) {
  size_t scheme_ender = uri_view.find("://");
  return scheme_ender == base::StringPiece::npos ? scheme_ender
                                                 : scheme_ender + 3;
}

// Evaluates true when |scheme_view| (including the trailing colon and 2
// slashes) equals a known printing URI scheme. Caller must ensure
// |scheme_view| is already properly percent-encoded.
bool SchemeIsForPrinting(base::StringPiece scheme_view) {
  // Enumerate known printing URIs. Values are lifted from Chrome browser's
  // Printer::GetProtocol().
  const std::vector<base::StringPiece> known_schemes = {
      "usb://",   "ipp://",    "ipps://", "http://",
      "https://", "socket://", "lpd://",  "ippusb://"};
  for (const base::StringPiece scheme : known_schemes) {
    if (base::EqualsCaseInsensitiveASCII(scheme_view, scheme)) {
      return true;
    }
  }
  return false;
}

// Evaluates true when the authority portion of a printing URI appears
// reasonable. Caller must ensure |authority_view| is already properly
// percent-encoded and does not contain the slash that begins the path.
bool AuthoritySeemsReasonable(base::StringPiece authority_view) {
  if (authority_view.empty()) {
    return false;
  }

  // My reading of RFC 3986 says to me that any non-reserved character
  // in the host can be percent-encoded. I'm going to punt on decoding
  // the variety of possible hosts and focus on the port number.
  // TODO(kdlee): figure out why nobody else in platform2 uses libcurl.
  size_t last_colon = authority_view.rfind(':');
  if (last_colon == base::StringPiece::npos) {
    // We don't see a port number - punt.
    return true;
  } else if (last_colon == authority_view.length() - 1) {
    // We see a colon but no port number - this is unreasonable.
    return false;
  }

  // There are several possibilities for other placements of the colon.
  // 1. It could be inside the user info (before host and port).
  // 2. It could be inside an IP literal (e.g. if the host is an IPv6
  //    address and no port is attached to this authority).
  // 3. It could be near the end of the authority with actual numeric
  //    values following it.
  base::StringPiece port_view = authority_view.substr(last_colon + 1);
  if (port_view.find('@') != base::StringPiece::npos) {
    // AFAIK, this colon is inside user info - punt.
    return true;
  } else if (authority_view.starts_with("[") && authority_view.ends_with("]")) {
    // AFAIK, this colon is part of an IPv6 literal without a port number -
    // punt.
    return true;
  }
  // AFAIK, this must be intended to be a decimal port number.
  for (const char c : port_view) {
    if (!base::IsAsciiDigit(c)) {
      return false;
    }
  }
  size_t decimal_port;
  if (!base::StringToSizeT(port_view, &decimal_port)) {
    return false;
  }
  return (0ll <= decimal_port) && (decimal_port < 65536ll);
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

// Tests a URI's visual similarity with an HTTP URI.
// This function observes a subset of RFC 3986 but is _not_ meant to serve
// as a general-purpose URI validator (prefer Chromium's GURL).
bool CupsTool::UriSeemsReasonable(const std::string& uri) {
  if (!UriIsGoodAscii(uri)) {
    return false;
  }

  base::StringPiece uri_view = uri;
  size_t authority_starter = UriAuthorityStartIndex(uri_view);
  if (authority_starter == base::StringPiece::npos ||
      authority_starter == uri_view.length()) {
    return false;
  }

  base::StringPiece scheme = uri_view.substr(0, authority_starter);
  base::StringPiece after_scheme = uri_view.substr(authority_starter);

  if (!SchemeIsForPrinting(scheme)) {
    return false;
  }

  base::StringPiece authority = after_scheme.substr(0, after_scheme.find('/'));
  return AuthoritySeemsReasonable(authority);
}

}  // namespace debugd
