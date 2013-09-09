// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/util.h"
#include "common/constants.h"
#include "server/file_watcher.h"
#include "server/service_publisher.h"
#include "server/peer_update_manager.h"

#include <stdio.h>

#include <iostream>
#include <cassert>
#include <cerrno>

#include <gio/gio.h>
#include <avahi-client/client.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-common/error.h>
#include <avahi-client/publish.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/bind.h>
#include <metrics/metrics_library.h>

using std::ostream;
using std::cout;
using std::cerr;
using std::endl;
using std::string;

using base::FilePath;

static void Usage(ostream& ostream) {
  ostream
    << "Usage:\n"
    << "  p2p-server [OPTION..]\n"
    << "\n"
    << "Options:\n"
    << " --help            Show help options\n"
    << " --path=DIR        Where to serve from\n"
    << "\n"
    << " --port=NUMBER     TCP port number for HTTP server (default: 16725)\n"
    << " -v=NUMBER         Verbosity level (default: 0)\n"
    << "\n";
}

int main(int argc, char* argv[]) {
  int ret = 1;
  GMainLoop* loop = NULL;

  g_type_init();

  CommandLine::Init(argc, argv);

  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE,
                       logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  p2p::util::SetupSyslog(p2p::constants::kServerBinaryName,
                         false /* include_pid */);

  LOG(INFO) << p2p::constants::kServerBinaryName
            << " " PACKAGE_VERSION << " starting";

  CommandLine* cl = CommandLine::ForCurrentProcess();

  if (cl->HasSwitch("help")) {
    Usage(cout);
    return 0;
  }

  FilePath path = cl->GetSwitchValuePath("path");
  if (path.empty()) {
    path = FilePath(p2p::constants::kP2PDir);
  }
  p2p::server::FileWatcher* file_watcher =
      p2p::server::FileWatcher::Construct(path, ".p2p");

  uint16_t http_port = p2p::constants::kHttpServerDefaultPort;
  string http_port_str = cl->GetSwitchValueNative("port");
  if (http_port_str.size() > 0) {
    char* endp;
    http_port = strtol(http_port_str.c_str(), &endp, 0);
    if (*endp != '\0') {
      cerr << "Error parsing `" << http_port_str << "' as port number"
                << endl;
      exit(1);
    }
  }
  MetricsLibrary metrics_lib;
  metrics_lib.Init();

  p2p::server::HttpServer* http_server =
      p2p::server::HttpServer::Construct(&metrics_lib, path, http_port);

  p2p::server::ServicePublisher* service_publisher =
      p2p::server::ServicePublisher::Construct(http_port);
  if (service_publisher == NULL) {
    cerr << "Error constructing ServicePublisher." << endl;
    exit(1);
  }

  p2p::server::PeerUpdateManager manager(
      file_watcher, service_publisher, http_server, &metrics_lib);
  manager.Init();

  loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);

  ret = 0;

  g_main_loop_unref(loop);

  delete service_publisher;
  delete http_server;
  delete file_watcher;

  return ret;
}
