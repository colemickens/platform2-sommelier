// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/avahi_service_publisher.h"

#include <vector>

#include <avahi-client/publish.h>
#include <avahi-common/address.h>
#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <base/callback.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <brillo/dbus/dbus_signal_handler.h>
#include <brillo/strings/string_utils.h>
#include <dbus/bus.h>

#include "peerd/avahi_client.h"
#include "peerd/constants.h"
#include "peerd/dbus_constants.h"
#include "peerd/service.h"

using brillo::Error;
using brillo::ErrorPtr;
using brillo::dbus_utils::CallMethodAndBlock;
using brillo::dbus_utils::ExtractMethodCallResults;
using brillo::string_utils::Join;
using peerd::constants::kSerbusServiceId;
using peerd::constants::mdns::kSerbusServiceDelimiter;
using std::vector;

namespace peerd {

namespace errors {
namespace avahi {

const char kRemovedUnknownService[] = "avahi.unknown_service";
const char kInvalidServiceId[] = "avahi.invalid_service_id";

}  // namespace avahi
}  // namespace errors

AvahiServicePublisher::AvahiServicePublisher(
    const std::string& uuid,
    const std::string& unique_prefix,
    const scoped_refptr<dbus::Bus>& bus,
    dbus::ObjectProxy* avahi_proxy,
    const base::Closure& on_publish_failure)
        : uuid_{uuid},
          unique_prefix_{unique_prefix},
          bus_{bus},
          avahi_proxy_{avahi_proxy},
          on_publish_failure_{on_publish_failure} {
  VLOG(1) << "mDNS prefix is " << unique_prefix << ".";
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
  if (service.GetServiceId() == kSerbusServiceId) {
    Error::AddToPrintf(error, FROM_HERE,
                       kPeerdErrorDomain,
                       errors::avahi::kInvalidServiceId,
                       "Service name is reserved: %s.",
                       kSerbusServiceId);
    return false;
  }
  const bool updated_group = UpdateGroup(
      error, service.GetServiceId(),
      service.GetServiceInfo(), service.GetMDnsOptions());
  // Always update the master record.
  return UpdateRootService(error) && updated_group;
}

bool AvahiServicePublisher::UpdateGroup(
    ErrorPtr* error,
    const std::string& service_id,
    const Service::ServiceInfo& service_info,
    const Service::MDnsOptions& mdns_options) {
  VLOG(1) << "Modifying group for service_id=" << service_id;
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
    // If we fail to connect to the the StateChange signal for this group, just
    // report that the whole thing has failed.
    auto on_failure_to_connect_cb = [](const base::Closure& on_failure,
                                       const std::string& interface_name,
                                       const std::string& signal_name,
                                       bool success) {
      if (success) { return; }
      LOG(WARNING) << "Failed to connect to StateChange signal "
                      "from EntryGroup.";
      base::MessageLoop::current()->task_runner()->PostTask(FROM_HERE,
                                                            on_failure);
    };
    brillo::dbus_utils::ConnectToSignal(
        group_proxy,
        dbus_constants::avahi::kGroupInterface,
        dbus_constants::avahi::kGroupSignalStateChanged,
        base::Bind(&AvahiServicePublisher::HandleGroupStateChanged,
                   base::Unretained(this)),
        base::Bind(on_failure_to_connect_cb, on_publish_failure_));
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
  if (!AddServiceToGroup(error, service_id, service_info,
                         mdns_options, group_proxy)) {
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
    Error::AddToPrintf(error, FROM_HERE,
                       kPeerdErrorDomain,
                       errors::avahi::kRemovedUnknownService,
                       "Attempted to remove unknown service: %s.",
                       service_id.c_str());
    return false;
  }
  const bool remove_successful = FreeGroup(error, it->second);
  outstanding_groups_.erase(it);
  // Always update the master record.
  return UpdateRootService(error) && remove_successful;
}

AvahiServicePublisher::TxtRecord AvahiServicePublisher::GetTxtRecord(
    const Service::ServiceInfo& info) {
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

bool AvahiServicePublisher::AddServiceToGroup(
    ErrorPtr* error,
    const std::string& service_id,
    const Service::ServiceInfo& service_info,
    const Service::MDnsOptions& mdns_options,
    dbus::ObjectProxy* group_proxy) {
  auto resp = CallMethodAndBlock(
      group_proxy,
      dbus_constants::avahi::kGroupInterface,
      dbus_constants::avahi::kGroupMethodAddService,
      error,
      int32_t{AVAHI_IF_UNSPEC},
      int32_t{AVAHI_PROTO_UNSPEC},
      uint32_t{0},  // No flags.
      unique_prefix_,
      AvahiClient::GetServiceType(service_id),
      std::string{},  // domain.
      std::string{},  // hostname
      mdns_options.port,
      GetTxtRecord(service_info));
  if (!resp || !ExtractMethodCallResults(resp.get(), error)) { return false; }
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

bool AvahiServicePublisher::UpdateRootService(ErrorPtr* error) {
  std::vector<std::string> services;
  services.reserve(outstanding_groups_.size());
  for (const auto& pair : outstanding_groups_) {
    if (pair.first != kSerbusServiceId) {
      services.push_back(pair.first);
    }
  }
  if (services.empty()) {
    // If we have no services to advertise, don't even publish the root
    // record.
    bool success = true;
    auto it = outstanding_groups_.find(kSerbusServiceId);
    if (it != outstanding_groups_.end()) {
      success = FreeGroup(error, it->second);
      outstanding_groups_.erase(it);
    }
    return success;
  }
  Service::ServiceInfo service_info{
    {constants::mdns::kSerbusVersion, "1.0"},
    {constants::mdns::kSerbusPeerId, uuid_},
    {constants::mdns::kSerbusServiceList, Join(kSerbusServiceDelimiter,
                                               services)},
  };
  return UpdateGroup(
      error, kSerbusServiceId, service_info, Service::MDnsOptions{});
}

void AvahiServicePublisher::HandleGroupStateChanged(
    int32_t state,
    const std::string& error_message) {
  if (state == AVAHI_ENTRY_GROUP_COLLISION ||
      state == AVAHI_ENTRY_GROUP_FAILURE) {
    base::MessageLoop::current()->task_runner()->PostTask(FROM_HERE,
                                                          on_publish_failure_);
  }
}

}  // namespace peerd
