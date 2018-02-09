// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>
#include <unistd.h>  // for isatty()

#include <string>
#include <vector>

#include <base/strings/string_util.h>
#include <brillo/flag_helper.h>
#include <brillo/http/http_proxy.h>
#include <brillo/http/http_transport.h>
#include <brillo/syslog_logging.h>
#include <dbus/bus.h>

namespace {

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

int main(int argc, char* argv[]) {
  brillo::FlagHelper::Init(argc, argv, "Crash helper: proxy lister");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (argc > 2) {
    LOG(ERROR) << "Only one argument allowed: an optional URL";
    return 1;
  }

  std::string url;
  if (argc > 1) {
    url = argv[1];
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
