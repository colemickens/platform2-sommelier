// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/avahi_service_publisher.h"

#include <chromeos/dbus/dbus_method_invoker.h>
#include <dbus/bus.h>

#include "peerd/dbus_constants.h"

using chromeos::dbus_utils::CallMethodAndBlock;
using chromeos::dbus_utils::ExtractMethodCallResults;

namespace peerd {

AvahiServicePublisher::AvahiServicePublisher(
    const scoped_refptr<dbus::Bus>& bus,
    dbus::ObjectProxy* avahi_proxy) : bus_{bus}, avahi_proxy_{avahi_proxy} {
}

AvahiServicePublisher::~AvahiServicePublisher() {
  for (const auto& group : outstanding_groups_) {
    auto resp = CallMethodAndBlock(group.second,
                                   dbus_constants::avahi::kGroupInterface,
                                   dbus_constants::avahi::kGroupMethodFree,
                                   nullptr);  // Ignore errors.
    // The response is empty, but log any errors we might find.
    if (resp) {
      ExtractMethodCallResults(resp.get(), nullptr);
    }
  }
}

base::WeakPtr<AvahiServicePublisher> AvahiServicePublisher::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool AvahiServicePublisher::OnServiceUpdated(chromeos::ErrorPtr* error,
                                             const Service& service) {
  auto it = outstanding_groups_.find(service.GetServiceId());
  dbus::ObjectProxy* group_proxy = nullptr;
  if (it == outstanding_groups_.end()) {
    auto resp = CallMethodAndBlock(
        avahi_proxy_, dbus_constants::avahi::kServerInterface,
        dbus_constants::avahi::kServerMethodEntryGroupNew,
        error);
    dbus::ObjectPath group_path;
    if (!resp || !ExtractMethodCallResults(resp.get(), error, &group_path)) {
      return false;
    }
    group_proxy = bus_->GetObjectProxy(dbus_constants::avahi::kServiceName,
                                       group_path);
    outstanding_groups_[group_path.value()] = group_proxy;
  } else {
    group_proxy = it->second;
  }

  // TODO(wiley) actually add records to this group, and commit it.
  return true;
}

bool AvahiServicePublisher::OnServiceRemoved(chromeos::ErrorPtr* error,
                                             const std::string& service_id) {
  // TODO(wiley) Find and remove the given service/group pair.
  return true;
}

}  // namespace peerd
