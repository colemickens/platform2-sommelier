// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>
#include <unistd.h>  // for isatty()

#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/strings/string_util.h>
#include <brillo/http/http_proxy.h>
#include <brillo/http/http_transport.h>
#include <brillo/syslog_logging.h>
#include <dbus/bus.h>

namespace {


const char kHelp[] = "help";
const char kQuiet[] = "quiet";
const char kVerbose[] = "verbose";
// Help message to show when the --help command line switch is specified.
const char kHelpMessage[] =
    "Chromium OS Crash helper: proxy lister\n"
    "\n"
    "Available Switches:\n"
    "  --quiet      Only print the proxies\n"
    "  --verbose    Print additional messages even when not run from a TTY\n"
    "  --help       Show this help.\n";

bool ShowBrowserProxies(const std::string& url) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return false;
  }

  std::vector<std::string> proxies;
  if (!brillo::http::GetChromeProxyServers(bus, url, &proxies)) {
    return false;
  }
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

  if (!ShowBrowserProxies(url)) {
    LOG(ERROR) << "Error resolving proxies";
    LOG(INFO) << "Assuming direct proxy";
    printf("%s\n", brillo::http::kDirectProxy);
  }

  return 0;
}
