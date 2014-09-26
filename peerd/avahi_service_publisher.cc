// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/avahi_service_publisher.h"

#include <vector>

#include <avahi-client/publish.h>
#include <avahi-common/address.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <dbus/bus.h>

#include "peerd/dbus_constants.h"

using chromeos::Error;
using chromeos::ErrorPtr;
using chromeos::dbus_utils::CallMethodAndBlock;
using chromeos::dbus_utils::ExtractMethodCallResults;
using std::vector;

namespace peerd {

namespace errors {
namespace avahi {

const char kRemovedUnknownService[] = "avahi.unknown_service";

}  // namespace avahi
}  // namespace errors

AvahiServicePublisher::AvahiServicePublisher(
    const std::string& lan_name,
    const scoped_refptr<dbus::Bus>& bus,
    dbus::ObjectProxy* avahi_proxy) : lan_unique_hostname_{lan_name},
                                      bus_{bus},
                                      avahi_proxy_{avahi_proxy} {
}

AvahiServicePublisher::~AvahiServicePublisher() {
  for (const auto& group : outstanding_groups_) {
    FreeGroup(nullptr, group.second);
  }
}

base::WeakPtr<AvahiServicePublisher> AvahiServicePublisher::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool AvahiServicePublisher::OnServiceUpdated(ErrorPtr* error,
                                             const Service& service) {
  const std::string service_id = service.GetServiceId();
  auto it = outstanding_groups_.find(service_id);
  dbus::ObjectProxy* group_proxy = nullptr;
  if (it == outstanding_groups_.end()) {
    // Create a new entry group for this service.
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
    outstanding_groups_[service_id] = group_proxy;
  } else {
    // Reset the entry group for this service.
    group_proxy = it->second;
    auto resp = CallMethodAndBlock(
        group_proxy, dbus_constants::avahi::kGroupInterface,
        dbus_constants::avahi::kGroupMethodReset, error);
    if (!resp || !ExtractMethodCallResults(resp.get(), error)) {
      // Failed to reset the group.  Remove the entry entirely from our DNS
      // record, and forget about that service.
      FreeGroup(error, group_proxy);
      outstanding_groups_.erase(it);
      return false;
    }
  }
  // Now add records for this service/entry group.
  if (!AddServiceToGroup(error, service, group_proxy)) {
    FreeGroup(error, group_proxy);
    outstanding_groups_.erase(service_id);  // |it| might be invalid
    return false;
  }
  return true;
}

bool AvahiServicePublisher::OnServiceRemoved(ErrorPtr* error,
                                             const std::string& service_id) {
  auto it = outstanding_groups_.find(service_id);
  if (it == outstanding_groups_.end()) {
    Error::AddToPrintf(error,
                       kPeerdErrorDomain,
                       errors::avahi::kRemovedUnknownService,
                       "Attempted to remove unknown service: %s.",
                       service_id.c_str());
    return false;
  }
  const bool remove_successful = FreeGroup(error, it->second);
  outstanding_groups_.erase(it);
  return remove_successful;
}

std::string AvahiServicePublisher::GetServiceType(const Service& service) {
  // TODO(wiley) We're hardcoding TCP here, but in theory we could advertise UDP
  //             services.  We'd have to pass that information down from our
  //             DBus interface.
  return "_" + service.GetServiceId() + "._tcp";
}

AvahiServicePublisher::TxtRecord AvahiServicePublisher::GetTxtRecord(
    const Service& service) {
  const Service::ServiceInfo& info = service.GetServiceInfo();
  TxtRecord result;
  result.reserve(info.size());
  for (const auto& kv : info) {
    result.emplace_back();
    vector<uint8_t>& record = result.back();
    record.reserve(kv.first.length() + kv.second.length() + 1);
    record.insert(record.end(), kv.first.begin(), kv.first.end());
    record.push_back('=');
    record.insert(record.end(), kv.second.begin(), kv.second.end());
  }
  return result;
}

bool AvahiServicePublisher::AddServiceToGroup(ErrorPtr* error,
                                              const Service& service,
                                              dbus::ObjectProxy* group_proxy) {
  auto resp = CallMethodAndBlock(
      group_proxy,
      dbus_constants::avahi::kGroupInterface,
      dbus_constants::avahi::kGroupMethodAddService,
      error,
      (int32_t)AVAHI_IF_UNSPEC,
      (int32_t)AVAHI_PROTO_UNSPEC,
      (uint32_t)(AvahiPublishFlags) 0,
      lan_unique_hostname_,
      GetServiceType(service),
      std::string(),  // domain.
      std::string(),  // hostname
      (uint16_t)0,  // TODO(wiley) port
      GetTxtRecord(service));
  if (!resp || !ExtractMethodCallResults(resp.get(), error)) {
    return false;
  }
  resp = CallMethodAndBlock(group_proxy,
                            dbus_constants::avahi::kGroupInterface,
                            dbus_constants::avahi::kGroupMethodCommit,
                            error);
  if (!resp || !ExtractMethodCallResults(resp.get(), error)) {
    return false;
  }
  return true;
}

bool AvahiServicePublisher::FreeGroup(ErrorPtr* error,
                                      dbus::ObjectProxy* group_proxy) {
  auto resp = CallMethodAndBlock(group_proxy,
                                 dbus_constants::avahi::kGroupInterface,
                                 dbus_constants::avahi::kGroupMethodFree,
                                 error);
  // Extract and log relevant errors.
  bool success = resp && ExtractMethodCallResults(resp.get(), error);
  // Ignore any signals we may have registered for from this proxy.
  group_proxy->Detach();
  return success;
}

}  // namespace peerd
