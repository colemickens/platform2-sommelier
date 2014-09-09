// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_AVAHI_CLIENT_H_
#define PEERD_AVAHI_CLIENT_H_

#include <vector>

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>
#include <dbus/message.h>

#include "peerd/typedefs.h"

namespace peerd {

// DBus client managing our interface to the Avahi daemon.
class AvahiClient {
 public:
  using OnAvahiRestartCallback = base::Closure;

  explicit AvahiClient(const scoped_refptr<dbus::Bus>& bus);
  virtual ~AvahiClient();
  void RegisterAsync(const CompletionAction& cb);
  // Register interest in Avahi daemon restarts.  For instance, Avahi
  // restarts should trigger us to re-register all exposed services,
  // since the hostname for our local host may have changed.
  // If Avahi is up right now, we'll call this callback immediately.
  // Registered callbacks are persistent for the life of AvahiClient.
  void RegisterOnAvahiRestartCallback(const OnAvahiRestartCallback& cb);

 private:
  // Watch for changes in Avahi server state.
  void OnServerStateChanged(dbus::Signal* signal);
  // Called just once to get the initial Avahi daemon state.
  void ReadInitialState(bool ignored_success);
  // Logic to react to Avahi server state changes.
  void HandleServerStateChange(int32_t state);

  dbus::ObjectProxy* server_{nullptr};
  std::vector<OnAvahiRestartCallback> avahi_ready_callbacks_;
  bool avahi_is_up_{false};
  // Must be last member to invalidate pointers before actual desctruction.
  base::WeakPtrFactory<AvahiClient> weak_ptr_factory_;

  friend class AvahiClientTest;
  DISALLOW_COPY_AND_ASSIGN(AvahiClient);
};

}  // namespace peerd

#endif  // PEERD_AVAHI_CLIENT_H_
