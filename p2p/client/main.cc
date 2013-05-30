// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/util.h"
#include "common/constants.h"
#include "client/service_finder.h"

#include <stdio.h>
#include <cassert>
#include <cerrno>
#include <iostream>

#include <gio/gio.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/bind.h>
#include <base/rand_util.h>

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
          << "\n";
}

// Lists all URLs discovered via |finder|. If |id| is not the empty
// string then only lists URLs matching it.
static void ListUrls(p2p::client::ServiceFinder* finder,
                     const std::string &id) {
  map<string, size_t> files = finder->AvailableFiles();

  for (auto const& i : files) {
    string file_name = i.first;
    if (id == "" || file_name == id) {
      cout << file_name << endl;
      vector<p2p::client::Peer*> peers = finder->GetPeersForFile(file_name);
      for (auto const& peer : peers) {
        cout << " address " << peer->address << ", port " << peer->port
             << ", size " << peer->files[file_name] << ", num_connections "
             << peer->num_connections << endl;
      }
    }
  }
}

// Type used for std::sort()
struct SortPeerBySize {
  explicit SortPeerBySize(const std::string& id) : id_(id) {}

  bool operator() (p2p::client::Peer *a, p2p::client::Peer *b) {
    size_t size_a = a->files[id_];
    size_t size_b = b->files[id_];
    return size_a > size_b;
  }

  string id_;
};

// Returns a suitable URL using |finder| for |id|. Returns the empty
// string if one could not be found.
static string PickUrlForId(p2p::client::ServiceFinder* finder,
                           const string& id) {
  map<string, size_t> files = finder->AvailableFiles();

  for (auto const& i : files) {
    string file_name = i.first;
    if (file_name == id) {
      vector<p2p::client::Peer*> peers = finder->GetPeersForFile(id);

      if (peers.size() > 0) {
        // Sort according to size (largest file size first)
        std::sort(peers.begin(), peers.end(), SortPeerBySize(file_name));

        // Don't consider peers with file size 0
        int num_nonempty_files = 0;
        for (auto const& peer : peers) {
          if (peer->files[file_name] > 0) {
            ++num_nonempty_files;
          }
        }
        peers.resize(num_nonempty_files);

        // If we have any files left, pick randomly from the top 33%
        if (peers.size() > 0) {
          int victim_number = 0;
          int num_possible_victims = peers.size()/3 - 1;
          if (num_possible_victims > 1)
            victim_number = base::RandInt(0, num_possible_victims - 1);
          p2p::client::Peer* victim = peers[victim_number];
          string address = victim->address;
          if (victim->is_ipv6)
            address = "[" + address + "]";
          return string("http://") + address + ":" +
            std::to_string(victim->port) + "/" + id;
        }
      }
    }
  }

  return "";
}

// Finds a URL using |id| and waits until the number of connections in
// the LAN has dropped below the required threshold. Returns "" if the
// URL could not be found.
static string GetUrlAndWait(p2p::client::ServiceFinder* finder,
                            const string& id) {
  LOG(INFO) << "Requesting URL in the LAN for ID " << id;

  string url = PickUrlForId(finder, id);
  int num_retries = 0;

  do {
    // If we didn't find a peer, fail
    if (url.size() == 0) {
      LOG(INFO) << "Returning error - no peer for the given ID.";
      return "";
    }

    // Only return the peer if the number of connections in the LAN
    // is below the threshold
    int num_total_conn = finder->NumTotalConnections();
    if (num_total_conn < p2p::constants::kMaxSimultaneousDownloads) {
      LOG(INFO) << "Returning URL " << url << " after " << num_retries
                << " retries.";
      return url;
    }

    LOG(INFO) << "Found peer for the given ID but there are already "
              << num_total_conn << " download(s) in the LAN which exceeds "
              << "the threshold of "
              << p2p::constants::kMaxSimultaneousDownloads << " download(s). "
              << "Sleeping "
              << p2p::constants::kMaxSimultaneousDownloadsPollTimeSeconds
              << " seconds until retrying.";

    sleep(p2p::constants::kMaxSimultaneousDownloadsPollTimeSeconds);

    // OK, now that we've slept for a while, the URL may not be
    // valid anymore... so we do the lookup again
    finder->Lookup();
    url = PickUrlForId(finder, id);
    num_retries++;
  } while (true);
}

int main(int argc, char* argv[]) {
  p2p::client::ServiceFinder* finder = NULL;

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
  finder = p2p::client::ServiceFinder::Construct();
  if (finder == NULL)
    return 1;

  if (cl->HasSwitch("list-all")) {
    finder->Lookup();
    ListUrls(finder, "");
  } else if (cl->HasSwitch("num-connections")) {
    finder->Lookup();
    int num_connections = finder->NumTotalConnections();
    cout << num_connections << endl;
  } else if (cl->HasSwitch("get-url")) {
    string id = cl->GetSwitchValueNative("get-url");
    finder->Lookup();
    string url = GetUrlAndWait(finder, id);
    if (url == "") {
      return 1;
    }
    cout << url << endl;
  } else if (cl->HasSwitch("list-urls")) {
    string id = cl->GetSwitchValueNative("list-urls");
    finder->Lookup();
    ListUrls(finder, id);
  } else {
    Usage(cerr);
    return 1;
  }

  return 0;
}
