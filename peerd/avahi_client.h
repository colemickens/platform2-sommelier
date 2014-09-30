// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_AVAHI_CLIENT_H_
#define PEERD_AVAHI_CLIENT_H_

#include <string>
#include <vector>

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>
#include <dbus/message.h>

#include "peerd/avahi_service_publisher.h"
#include "peerd/typedefs.h"

namespace peerd {

class AvahiServicePublisher;
class ServicePublisherInterface;

// DBus client managing our interface to the Avahi daemon.
class AvahiClient {
 public:
  using OnAvahiRestartCallback = base::Closure;

  explicit AvahiClient(const scoped_refptr<dbus::Bus>& bus);
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
  virtual base::WeakPtr<ServicePublisherInterface> GetPublisher();

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
  // Ask Avahi for the current hostname.
  bool GetHostName(std::string* hostname) const;

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* server_{nullptr};
  std::vector<OnAvahiRestartCallback> avahi_ready_callbacks_;
  bool avahi_is_up_{false};
  std::unique_ptr<AvahiServicePublisher> publisher_{nullptr};
  // Must be last member to invalidate pointers before actual desctruction.
  base::WeakPtrFactory<AvahiClient> weak_ptr_factory_{this};

  friend class AvahiClientTest;
  DISALLOW_COPY_AND_ASSIGN(AvahiClient);
};

}  // namespace peerd

#endif  // PEERD_AVAHI_CLIENT_H_
