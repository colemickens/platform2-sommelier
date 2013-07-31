// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "client/clock.h"
#include "client/peer_selector.h"
#include "client/service_finder.h"
#include "common/constants.h"
#include "common/util.h"

#include <stdio.h>
#include <cassert>
#include <cerrno>
#include <iostream>

#include <gio/gio.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/rand_util.h>
#include <base/string_number_conversions.h>

using std::cout;
using std::cerr;
using std::endl;
using std::map;
using std::ostream;
using std::string;
using std::vector;

static void Usage(ostream& ostream) {
  ostream << "Usage:\n"
          << "  p2p-client [OPTION..]\n"
          << "\n"
          << "Options:\n"
          << " --help             Show help options\n"
          << " --list-all         Scan network and list available files\n"
          << " --list-urls=ID     Like --list-all but only show peers for ID\n"
          << " --get-url=ID       Scan for ID and pick a suitable peer\n"
          << " --num-connections  Show total number of connections in the LAN\n"
          << " -v=NUMBER          Verbosity level (default: 0)\n"
          << " --minimum-size=NUM When used with --get-url, scans for files\n"
          << "                    with at least NUM bytes (default: 1).\n"
          << "\n";
}

// Lists all URLs discovered via |finder|. If |id| is not the empty
// string then only lists URLs matching it.
static void ListUrls(p2p::client::ServiceFinder* finder,
                     const std::string &id) {
  vector<string> files = finder->AvailableFiles();

  for (auto const& file_name : files) {
    if (id == "" || file_name == id) {
      cout << file_name << endl;
      vector<const p2p::client::Peer*> peers =
          finder->GetPeersForFile(file_name);
      for (auto const& peer : peers) {
        map<string, size_t>::const_iterator file_size_it =
            peer->files.find(file_name);
        cout << " address " << peer->address << ", port " << peer->port
             << ", size "
             << (file_size_it == peer->files.end() ? -1 : file_size_it->second )
             << ", num_connections " << peer->num_connections << endl;
      }
    }
  }
}

int main(int argc, char* argv[]) {
  scoped_ptr<p2p::client::ServiceFinder> finder;

  g_type_init();
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE,
                       logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  p2p::util::SetupSyslog("p2p-client", true /* include_pid */);

  CommandLine* cl = CommandLine::ForCurrentProcess();

  // If help is requested, show usage and exit immediately
  if (cl->HasSwitch("help")) {
    Usage(cout);
    return 0;
  }

  // Get us a ServiceFinder and look up all peers - this takes a couple
  // of seconds. This can fail if e.g. avahi-daemon is not running.
  finder.reset(p2p::client::ServiceFinder::Construct());
  if (finder == NULL)
    return 1;

  p2p::client::Clock clock;
  p2p::client::PeerSelector peer_selector(finder.get(), &clock);

  if (cl->HasSwitch("list-all")) {
    finder->Lookup();
    ListUrls(finder.get(), "");
  } else if (cl->HasSwitch("num-connections")) {
    finder->Lookup();
    int num_connections = finder->NumTotalConnections();
    cout << num_connections << endl;
  } else if (cl->HasSwitch("get-url")) {
    string id = cl->GetSwitchValueNative("get-url");
    uint64 minimum_size = 1;
    if (cl->HasSwitch("minimum-size")) {
      string minimum_size_str = cl->GetSwitchValueNative("minimum-size");
      if (!base::StringToUint64(minimum_size_str, &minimum_size)) {
        LOG(ERROR) << "Invalid --minimum-size argument";
        return 1;
      }
    }
    finder->Lookup();
    string url = peer_selector.GetUrlAndWait(id, minimum_size);
    if (url == "") {
      return 1;
    }
    cout << url << endl;
  } else if (cl->HasSwitch("list-urls")) {
    string id = cl->GetSwitchValueNative("list-urls");
    finder->Lookup();
    ListUrls(finder.get(), id);
  } else {
    Usage(cerr);
    return 1;
  }

  return 0;
}
