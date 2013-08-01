// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/util.h"
#include "client/service_finder.h"

#include <glib.h>
#include <avahi-client/client.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-common/error.h>
#include <avahi-client/lookup.h>

#include <stdexcept>
#include <set>

#include <base/logging.h>
#include <base/memory/ref_counted.h>

using std::vector;
using std::map;
using std::set;
using std::string;

namespace p2p {

namespace client {

class ServiceFinderAvahi : public ServiceFinder {
 public:
  ServiceFinderAvahi();
  virtual ~ServiceFinderAvahi();

  vector<const Peer*> GetPeersForFile(const string& file) const;

  vector<string> AvailableFiles() const;

  int NumTotalConnections() const;

  void Lookup();

  static ServiceFinderAvahi* Construct();

 private:
  static void on_avahi_changed(AvahiClient* client,
                               AvahiClientState state,
                               void* user_data);

  static void service_resolve_cb(AvahiServiceResolver* r,
                                 AvahiIfIndex interface,
                                 AvahiProtocol protocol,
                                 AvahiResolverEvent event,
                                 const char* name,
                                 const char* type,
                                 const char* domain,
                                 const char* host_name,
                                 const AvahiAddress* a,
                                 uint16_t port,
                                 AvahiStringList* txt,
                                 AvahiLookupResultFlags flags,
                                 void* user_data);

  bool IsOwnService(const char *name);

  void HandleResolverEvent(const AvahiAddress* a,
                           uint16_t port,
                           AvahiStringList* txt);

  static void on_service_browser_changed(AvahiServiceBrowser* b,
                                         AvahiIfIndex interface,
                                         AvahiProtocol protocol,
                                         AvahiBrowserEvent event,
                                         const char* name,
                                         const char* type,
                                         const char* domain,
                                         AvahiLookupResultFlags flags,
                                         void* user_data);

  virtual bool Initialize();
  void BrowserCheckIfDone();

  AvahiGLibPoll* poll_;
  AvahiClient* client_;
  bool running_;
  vector<Peer*> peers_;
  map<string, vector<Peer*> > file_to_servers_;
  AvahiServiceBrowser* lookup_browser_;
  bool lookup_all_for_now_;
  set<AvahiServiceResolver*> lookup_pending_resolvers_;
  GMainLoop* lookup_loop_;

  DISALLOW_COPY_AND_ASSIGN(ServiceFinderAvahi);
};

ServiceFinderAvahi::ServiceFinderAvahi()
    : poll_(NULL),
      client_(NULL),
      running_(false),
      lookup_browser_(NULL),
      lookup_all_for_now_(false),
      lookup_loop_(NULL) {}

ServiceFinderAvahi::~ServiceFinderAvahi() {
  for (auto const& i : peers_) {
    delete i;
  }

  CHECK(lookup_browser_ == NULL);
  CHECK(lookup_pending_resolvers_.size() == 0);
  CHECK(lookup_loop_ == NULL);

  if (client_ != NULL)
    avahi_client_free(client_);
  if (poll_ != NULL)
    avahi_glib_poll_free(poll_);
}

vector<string> ServiceFinderAvahi::AvailableFiles() const {
  vector<string> ret;
  for (auto const& i : file_to_servers_)
    ret.push_back(i.first);
  return ret;
}

int ServiceFinderAvahi::NumTotalConnections() const {
  int sum = 0;
  for (auto const& peer : peers_)
    sum += peer->num_connections;
  return sum;
}

vector<const Peer*> ServiceFinderAvahi::GetPeersForFile(
    const string& file) const {
  map<string, vector<Peer*> >::const_iterator it =
    file_to_servers_.find(file);
  if (it == file_to_servers_.end())
    return vector<const Peer*>();
  return vector<const Peer*>(it->second.begin(), it->second.end());
}

void ServiceFinderAvahi::HandleResolverEvent(const AvahiAddress* a,
                                             uint16_t port,
                                             AvahiStringList* txt) {
  Peer* peer = NULL;
  AvahiStringList* l;
  // 64 bytes is enough to hold any literal IPv4 and IPv6 addresses
  char buf[64];

  avahi_address_snprint(buf, sizeof buf, a);

  peer = new Peer();
  peer->address = string(buf);
  peer->is_ipv6 = (a->proto == AVAHI_PROTO_INET6);
  peer->port = port;

  for (l = txt; l != NULL; l = l->next) {
    string txt((const char*)l->text, l->size);
    const char* s = txt.c_str();
    const char* e = strrchr(s, '=');

    VLOG(1) << " TXT: len=" << l->size << " data=" << txt;

    if (e == NULL || strlen(e + 1) < 1) {
      LOG(WARNING) << "Attribute `" << txt
                   << "` is malformed (malformed value)";
      continue;
    }

    if (strncasecmp(s, "id_", strlen("id_")) == 0) {
      char* endp = NULL;
      size_t file_size = strtol(e + 1, &endp, 10);
      string file_name = txt.substr(strlen("id_"), e - s - strlen("id_"));

      if (*endp != '\0') {
        LOG(WARNING) << "Attribute `" << txt
                     << "` is malformed (value not a decimal number)";
        continue;
      }

      peer->files[file_name] = file_size;

    } else if (strncasecmp(s, "num_connections=", strlen("num_connections=")) ==
               0) {
      char* endp = NULL;
      int parsed_value = strtol(s + strlen("num_connections="), &endp, 10);
      if (endp != NULL) {
        peer->num_connections = parsed_value;
      }
    }
  }

  peers_.push_back(peer);
  for (auto const& file : peer->files) {
    vector<Peer*>& per_file = file_to_servers_[file.first];
    per_file.push_back(peer);
  }
}

void ServiceFinderAvahi::service_resolve_cb(AvahiServiceResolver* r,
                                            AvahiIfIndex interface,
                                            AvahiProtocol protocol,
                                            AvahiResolverEvent event,
                                            const char* name,
                                            const char* type,
                                            const char* domain,
                                            const char* host_name,
                                            const AvahiAddress* a,
                                            uint16_t port,
                                            AvahiStringList* txt,
                                            AvahiLookupResultFlags flags,
                                            void* user_data) {
  ServiceFinderAvahi* finder = reinterpret_cast<ServiceFinderAvahi*>(user_data);

  if (event == AVAHI_RESOLVER_FAILURE) {
    LOG(ERROR) << "Resolver failure: "
               << avahi_strerror(avahi_client_errno(finder->client_));
  } else {
    finder->HandleResolverEvent(a, port, txt);
  }

  if (finder->lookup_pending_resolvers_.erase(r) != 1)
    NOTREACHED();
  avahi_service_resolver_free(r);

  finder->BrowserCheckIfDone();
}

bool ServiceFinderAvahi::IsOwnService(const char *name) {
  // Here we rely on the implementation detail that the DNS-SD name
  // used is the D-Bus machine-id.
  return g_strcmp0(name, p2p::util::GetDBusMachineId()) == 0;
}

void ServiceFinderAvahi::on_service_browser_changed(
    AvahiServiceBrowser* b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char* name,
    const char* type,
    const char* domain,
    AvahiLookupResultFlags flags,
    void* user_data) {
  ServiceFinderAvahi* finder = reinterpret_cast<ServiceFinderAvahi*>(user_data);

  // Can be called directly by avahi_service_browser_new() so the browser_
  // member may not be set just yet...
  if (finder->lookup_browser_ == NULL)
    finder->lookup_browser_ = b;

  VLOG(1) << "on_browser_changed: event=" << event
          << " name=" << (name != NULL ? name : "(nil)") << " type=" << type
          << " domain=" << (domain != NULL ? domain : "(nil)")
          << " flags=" << flags;

  // Never return results from ourselves
  if (finder->IsOwnService(name)) {
    VLOG(1) << "Ignoring results from ourselves.";
    return;
  }

  switch (event) {
    case AVAHI_BROWSER_FAILURE:
      LOG(ERROR) << "Browser failure: " << avahi_strerror(avahi_client_errno(
                                               finder->client_));
      break;

    case AVAHI_BROWSER_NEW: {
      AvahiServiceResolver* resolver =
          avahi_service_resolver_new(finder->client_,
                                     interface,
                                     protocol,
                                     name,
                                     type,
                                     domain,
                                     AVAHI_PROTO_UNSPEC,
                                     (AvahiLookupFlags) 0,
                                     service_resolve_cb,
                                     finder);
      finder->lookup_pending_resolvers_.insert(resolver);
    } break;

    case AVAHI_BROWSER_REMOVE:
      break;

    case AVAHI_BROWSER_CACHE_EXHAUSTED:
      break;

    case AVAHI_BROWSER_ALL_FOR_NOW:
      finder->lookup_all_for_now_ = TRUE;
      finder->BrowserCheckIfDone();
      break;
  }
}

void ServiceFinderAvahi::BrowserCheckIfDone() {
  if (!lookup_all_for_now_)
    return;

  if (lookup_pending_resolvers_.size() > 0)
    return;

  CHECK(lookup_loop_ != NULL);
  g_main_loop_quit(lookup_loop_);
}

void ServiceFinderAvahi::Lookup() {
  CHECK(lookup_loop_ == NULL);

  // Clear existing data, if any
  peers_.clear();
  file_to_servers_.clear();
  lookup_all_for_now_ = false;

  lookup_loop_ = g_main_loop_new(NULL, FALSE);
  lookup_browser_ = avahi_service_browser_new(client_,
                                              AVAHI_IF_UNSPEC,
                                              AVAHI_PROTO_UNSPEC,
                                              "_cros_p2p._tcp",
                                              NULL, /* domain */
                                              (AvahiLookupFlags) 0,
                                              on_service_browser_changed,
                                              this);
  g_main_loop_run(lookup_loop_);
  g_main_loop_unref(lookup_loop_);
  lookup_loop_ = NULL;

  avahi_service_browser_free(lookup_browser_);
  lookup_browser_ = NULL;
}

// -----------------------------------------------------------------------------

void ServiceFinderAvahi::on_avahi_changed(AvahiClient* client,
                                          AvahiClientState state,
                                          void* user_data) {
  ServiceFinderAvahi* finder = reinterpret_cast<ServiceFinderAvahi*>(user_data);
  VLOG(1) << "on_avahi_changed, state=" << state;
  if (state == AVAHI_CLIENT_S_RUNNING) {
    finder->running_ = true;
  } else {
    finder->running_ = false;
  }
}

bool ServiceFinderAvahi::Initialize() {
  int error;

  // Note that if Avahi is not running and can't be activated,
  // avahi_client_new() may block for up to 25 seconds because it's
  // doing a sync D-Bus method call... short of fixing libavahi-client
  // there's really no way around this :-/
  poll_ = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
  client_ = avahi_client_new(avahi_glib_poll_get(poll_),
                             (AvahiClientFlags) 0,
                             on_avahi_changed,
                             this,
                             &error);
  if (client_ == NULL) {
    LOG(ERROR) << "Error constructing AvahiClient: " << error;
    return false;
  }

  if (!running_) {
    LOG(ERROR) << "Avahi daemon is not running";
    return false;
  }

  return true;
}

ServiceFinderAvahi* ServiceFinderAvahi::Construct() {
  ServiceFinderAvahi* client = new ServiceFinderAvahi();
  if (!client->Initialize()) {
    delete client;
    return NULL;
  }
  return client;
}

ServiceFinder* ServiceFinder::Construct() {
  return ServiceFinderAvahi::Construct();
}

}  // namespace client

}  // namespace p2p
