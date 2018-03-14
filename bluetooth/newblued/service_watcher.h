// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_SERVICE_WATCHER_H_
#define BLUETOOTH_NEWBLUED_SERVICE_WATCHER_H_

#include <string>

#include <base/memory/weak_ptr.h>
#include <dbus/object_proxy.h>

namespace bluetooth {

// This is a helper class to watch the availability of another D-Bus service.
class ServiceWatcher {
 public:
  using ServiceAvailabilityChangedCallback =
      base::Callback<void(bool is_available)>;

  // |object_proxy| is the service and object to watch for.
  explicit ServiceWatcher(dbus::ObjectProxy* object_proxy);
  ~ServiceWatcher() = default;

  // Clients can register a watcher by calling ServiceWatcher::RegisterWatcher
  // and availability events will be delivered without needing to care where
  // those events come from.
  void RegisterWatcher(ServiceAvailabilityChangedCallback callback);

 private:
  // Called when ownership of the service's D-Bus service name changes.
  // Invokes HandleServiceAvailableOrRestarted if the service is running.
  void ServiceNameOwnerChangedReceived(const std::string& old_owner,
                                       const std::string& new_owner);

  // Called when the service is initially available or restarted.
  void HandleServiceAvailableOrRestarted(bool is_available);

  // The callback to be called when service availability changes.
  ServiceAvailabilityChangedCallback service_availability_changed_callback_;

  // The ObjectProxy to watch for.
  dbus::ObjectProxy* object_proxy_;

  // Keeps the availability state of the last change. Needed to filter out
  // duplicate available events.
  bool last_is_available_ = false;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<ServiceWatcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWatcher);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_SERVICE_WATCHER_H_
