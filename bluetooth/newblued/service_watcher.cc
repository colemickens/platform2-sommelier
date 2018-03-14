// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/service_watcher.h"

#include <string>

#include <base/bind.h>
#include <base/memory/weak_ptr.h>
#include <dbus/object_proxy.h>

namespace bluetooth {

ServiceWatcher::ServiceWatcher(dbus::ObjectProxy* object_proxy)
    : object_proxy_(object_proxy), weak_ptr_factory_(this) {}

void ServiceWatcher::RegisterWatcher(
    ServiceAvailabilityChangedCallback callback) {
  CHECK(service_availability_changed_callback_.is_null())
      << "Watcher is already registered";
  service_availability_changed_callback_ = callback;
  object_proxy_->WaitForServiceToBeAvailable(
      base::Bind(&ServiceWatcher::HandleServiceAvailableOrRestarted,
                 weak_ptr_factory_.GetWeakPtr()));
  object_proxy_->SetNameOwnerChangedCallback(
      base::Bind(&ServiceWatcher::ServiceNameOwnerChangedReceived,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ServiceWatcher::ServiceNameOwnerChangedReceived(
    const std::string& old_owner, const std::string& new_owner) {
  VLOG(1) << "D-Bus service ownership of object "
          << object_proxy_->object_path().value() << " changed from \""
          << old_owner << "\" to \"" << new_owner << "\"";

  bool is_available = !new_owner.empty();
  HandleServiceAvailableOrRestarted(is_available);
}

void ServiceWatcher::HandleServiceAvailableOrRestarted(bool is_available) {
  if (is_available == last_is_available_) {
    // There is no change of availability state since the last change.
    return;
  }
  last_is_available_ = is_available;
  service_availability_changed_callback_.Run(is_available);
}

}  // namespace bluetooth
