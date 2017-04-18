// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>
#include <unistd.h>  // for isatty()

#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_tokenizer.h>
#include <base/strings/string_util.h>
#include <base/values.h>
#include <brillo/syslog_logging.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>
#include <chromeos/dbus/service_constants.h>

namespace {

const char kNoProxy[] = "direct://";

const int kTimeoutDefaultSeconds = 5;

const char kHelp[] = "help";
const char kQuiet[] = "quiet";
const char kTimeout[] = "timeout";
const char kVerbose[] = "verbose";
// Help message to show when the --help command line switch is specified.
const char kHelpMessage[] =
    "Chromium OS Crash helper: proxy lister\n"
    "\n"
    "Available Switches:\n"
    "  --quiet      Only print the proxies\n"
    "  --verbose    Print additional messages even when not run from a TTY\n"
    "  --timeout=N  Set timeout for browser resolving proxies (default is 5)\n"
    "  --help       Show this help.\n";

// Copied from src/update_engine/chrome_browser_proxy_resolver.cc
// Parses the browser's answer for resolved proxies.  It returns a
// list of strings, each of which is a resolved proxy.
std::vector<std::string> ParseProxyString(const std::string& input) {
  std::vector<std::string> ret;
  // Some of this code taken from
  // http://src.chromium.org/svn/trunk/src/net/proxy/proxy_server.cc and
  // http://src.chromium.org/svn/trunk/src/net/proxy/proxy_list.cc
  base::StringTokenizer entry_tok(input, ";");
  while (entry_tok.GetNext()) {
    std::string token = entry_tok.token();
    base::TrimWhitespaceASCII(token, base::TRIM_ALL, &token);

    // Start by finding the first space (if any).
    std::string::iterator space;
    for (space = token.begin(); space != token.end(); ++space) {
      if (base::IsAsciiWhitespace(*space)) {
        break;
      }
    }

    std::string scheme = base::ToLowerASCII(std::string(token.begin(), space));
    // Chrome uses "socks" to mean socks4 and "proxy" to mean http.
    if (scheme == "socks")
      scheme += "4";
    else if (scheme == "proxy")
      scheme = "http";
    else if (scheme != "https" &&
             scheme != "socks4" &&
             scheme != "socks5" &&
             scheme != "direct")
      continue;  // Invalid proxy scheme

    std::string host_and_port = std::string(space, token.end());
    base::TrimWhitespaceASCII(host_and_port, base::TRIM_ALL, &host_and_port);
    if (scheme != "direct" && host_and_port.empty())
      continue;  // Must supply host/port when non-direct proxy used.
    ret.push_back(scheme + "://" + host_and_port);
  }
  if (ret.empty() || *ret.rbegin() != kNoProxy)
    ret.push_back(kNoProxy);
  return ret;
}

bool ShowBrowserProxies(const std::string& url, base::TimeDelta timeout) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return false;
  }

  dbus::MethodCall method_call(
      chromeos::kNetworkProxyServiceInterface,
      chromeos::kNetworkProxyServiceResolveProxyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(url);

  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      chromeos::kNetworkProxyServiceName,
      dbus::ObjectPath(chromeos::kNetworkProxyServicePath));
  std::unique_ptr<dbus::Response> response =
      proxy->CallMethodAndBlock(&method_call, timeout.InMilliseconds());
  if (!response) {
    LOG(ERROR) << "Failed to get response";
    return false;
  }

  dbus::MessageReader reader(response.get());
  std::string proxy_info, error;
  if (!reader.PopString(&proxy_info) || !reader.PopString(&error)) {
    LOG(ERROR) << "Invalid arguments in response";
    return false;
  }

  std::vector<std::string> proxies = ParseProxyString(proxy_info);
  LOG(INFO) << "Got proxies: " << base::JoinString(proxies, "x");
  for (const auto& proxy : proxies) {
    printf("%s\n", proxy.c_str());
  }
  return true;
}

}  // namespace

int main(int argc, char *argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(kHelp)) {
    LOG(INFO) << kHelpMessage;
    return 0;
  }

  bool quiet = cl->HasSwitch(kQuiet);
  bool verbose = cl->HasSwitch(kVerbose);

  int timeout = kTimeoutDefaultSeconds;
  std::string str_timeout = cl->GetSwitchValueASCII(kTimeout);
  if (!str_timeout.empty() && !base::StringToInt(str_timeout, &timeout)) {
    LOG(ERROR) << "Invalid timeout value: " << str_timeout;
    return 1;
  }

  // Default to logging to syslog.
  int init_flags = brillo::kLogToSyslog;
  // Log to stderr if a TTY (and "-quiet" wasn't passed), or if "-verbose"
  // was passed.

  if ((!quiet && isatty(STDERR_FILENO)) || verbose)
    init_flags |= brillo::kLogToStderr;
  brillo::InitLog(init_flags);

  std::string url;
  base::CommandLine::StringVector urls = cl->GetArgs();
  if (!urls.empty()) {
    url = urls[0];
    LOG(INFO) << "Resolving proxies for URL: " << url;
  } else {
    LOG(INFO) << "Resolving proxies without URL";
  }

  if (!ShowBrowserProxies(url, base::TimeDelta::FromSeconds(timeout))) {
    LOG(ERROR) << "Error resolving proxies";
    LOG(INFO) << "Assuming direct proxy";
    printf("%s\n", kNoProxy);
  }

  return 0;
}
