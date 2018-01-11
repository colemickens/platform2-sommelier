// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_AVAHI_CLIENT_H_
#define PEERD_AVAHI_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>
#include <dbus/message.h>

#include "peerd/avahi_service_discoverer.h"
#include "peerd/avahi_service_publisher.h"
#include "peerd/typedefs.h"

namespace dbus {

class ObjectProxy;

}  // namespace dbus

namespace peerd {

class AvahiServicePublisher;
class PeerManagerInterface;
class ServicePublisherInterface;

// DBus client managing our interface to the Avahi daemon.
class AvahiClient {
 public:
  using OnAvahiRestartCallback = base::Closure;

  AvahiClient(const scoped_refptr<dbus::Bus>& bus,
              PeerManagerInterface* peer_manager);
  virtual ~AvahiClient();
  virtual void RegisterAsync(const CompletionAction& cb);
  // Register interest in Avahi daemon restarts.  For instance, Avahi
  // restarts should trigger us to re-register all exposed services,
  // since the hostname for our local host may have changed.
  // If Avahi is up right now, we'll call this callback immediately.
  // Registered callbacks are persistent for the life of AvahiClient.
  virtual void RegisterOnAvahiRestartCallback(const OnAvahiRestartCallback& cb);
  // Get an instance of ServicePublisherInterface that knows how to advertise
  // services on Avahi.  From time to time, this pointer will transparently
  // become invalid as the remote daemon signals that bad things have happened.
  // When we come back from these states, we'll call all
  // OnAvahiRestartCallbacks that we have.  At that point, grab a new publisher
  // and repeat.
  virtual base::WeakPtr<ServicePublisherInterface> GetPublisher(
      const std::string& uuid);

  virtual void StartMonitoring();
  virtual void StopMonitoring();

  virtual void AttemptToUseMDnsPrefix(const std::string& mdns_prefix);

  // Transform a service_id to a mDNS compatible service type.
  static std::string GetServiceType(const std::string& service_id);
  // Transform a mDNS compatible service type to a service id.
  static std::string GetServiceId(const std::string& service_type);

 private:
  // Watch for changes in Avahi server state.
  void OnServerStateChanged(int32_t state, const std::string& error);
  // ObjectProxy forces us to register a one off "ServiceAvailable"
  // callback for startup, then register to listen to service owner changes
  // in steady state.
  void OnServiceOwnerChanged(const std::string& old_owner,
                             const std::string& new_owner);
  void OnServiceAvailable(bool avahi_is_on_dbus);
  // Logic to react to Avahi server state changes.
  void HandleServerStateChange(int32_t state);
  // Logic to react to failure or success to start service discovery.
  void HandleDiscoveryStartupResult(bool success);
  // When we encounter problems publishing mDNS records, it should be
  // related to name collisions on the local subnet.  We'll just pick
  // a new unique prefix for our records and try again.
  void HandlePublishingFailure();

  scoped_refptr<dbus::Bus> bus_;
  PeerManagerInterface* peer_manager_;  // Outlives this.
  dbus::ObjectProxy* server_{nullptr};
  std::vector<OnAvahiRestartCallback> avahi_ready_callbacks_;
  bool avahi_is_up_{false};
  std::unique_ptr<AvahiServicePublisher> publisher_{nullptr};
  std::unique_ptr<AvahiServiceDiscoverer> discoverer_{nullptr};
  bool should_discover_{false};
  std::string next_mdns_prefix_;
  // Must be last member to invalidate pointers before actual destruction.
  base::WeakPtrFactory<AvahiClient> weak_ptr_factory_{this};

  friend class AvahiClientTest;
  DISALLOW_COPY_AND_ASSIGN(AvahiClient);
};

}  // namespace peerd

#endif  // PEERD_AVAHI_CLIENT_H_
