// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager.h"

#include <base/format_macros.h>
#include <base/guid.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <dbus/object_path.h>

#include "peerd/constants.h"
#include "peerd/dbus_constants.h"
#include "peerd/peer_manager_impl.h"
#include "peerd/published_peer.h"
#include "peerd/service.h"
#include "peerd/technologies.h"

using chromeos::Any;
using chromeos::Error;
using chromeos::ErrorPtr;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::DBusParamReader;
using chromeos::dbus_utils::DBusParamWriter;
using chromeos::dbus_utils::DBusServiceWatcher;
using chromeos::dbus_utils::ExportedObjectManager;
using dbus::ObjectPath;
using peerd::constants::kSerbusServiceId;
using peerd::dbus_constants::kPingResponse;
using peerd::dbus_constants::kSelfPath;
using std::map;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

namespace peerd {
namespace errors {
namespace manager {

const char kAlreadyExposed[] = "manager.already_exposed";
const char kInvalidMonitoringOption[] = "manager.option";
const char kInvalidMonitoringTechnology[] = "manager.invalid_technology";
const char kInvalidMonitoringToken[] = "manager.monitoring_token";
const char kNotOwner[] = "manager.not_owner";
const char kUnknownServiceId[] = "manager.unknown_service_id";

}  // namespace manager
}  // namespace errors

Manager::Manager(ExportedObjectManager* object_manager,
                 const string& initial_mdns_prefix)
  : Manager(object_manager->GetBus(),
            unique_ptr<DBusObject>{new DBusObject{
                object_manager, object_manager->GetBus(),
                org::chromium::peerd::ManagerAdaptor::GetObjectPath()}},
            unique_ptr<PublishedPeer>{},
            unique_ptr<PeerManagerInterface>{},
            unique_ptr<AvahiClient>{},
            initial_mdns_prefix) {
}

Manager::Manager(scoped_refptr<dbus::Bus> bus,
                 unique_ptr<DBusObject> dbus_object,
                 unique_ptr<PublishedPeer> self,
                 unique_ptr<PeerManagerInterface> peer_manager,
                 unique_ptr<AvahiClient> avahi_client,
                 const string& initial_mdns_prefix)
    : bus_{bus},
      dbus_object_{std::move(dbus_object)},
      self_{std::move(self)},
      peer_manager_{std::move(peer_manager)},
      avahi_client_{std::move(avahi_client)} {
  // If we haven't gotten mocks for these objects, make real ones.
  if (!self_) {
    self_.reset(new PublishedPeer{
        bus_, dbus_object_->GetObjectManager().get(), ObjectPath{kSelfPath}});
  }
  if (!peer_manager_) {
    peer_manager_.reset(
        new PeerManagerImpl{bus_, dbus_object_->GetObjectManager().get()});
  }
  if (!avahi_client_) {
    avahi_client_.reset( new AvahiClient{bus_, peer_manager_.get()});
    avahi_client_->AttemptToUseMDnsPrefix(initial_mdns_prefix);
  }
}


void Manager::RegisterAsync(const CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  dbus_adaptor_.RegisterWithDBusObject(dbus_object_.get());
  const bool self_success = self_->RegisterAsync(
      nullptr,
      base::GenerateGUID(),  // Every boot is a new GUID for now.
      base::Time::UnixEpoch(),
      sequencer->GetHandler("Failed exporting Self.", true));
  CHECK(self_success) << "Failed to RegisterAsync Self.";
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Failed exporting Manager.", true));
  avahi_client_->RegisterOnAvahiRestartCallback(
      base::Bind(&Manager::ShouldRefreshAvahiPublisher,
                 base::Unretained(this)));
  avahi_client_->RegisterAsync(
      sequencer->GetHandler("Failed AvahiClient.RegisterAsync().", true));
  sequencer->OnAllTasksCompletedCall({completion_callback});
}

bool Manager::StartMonitoring(
    ErrorPtr* error,
    const vector<string>& requested_technologies,
    const map<string, Any>& options,
    std::string* monitoring_token) {
  if (requested_technologies.empty())  {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::manager::kInvalidMonitoringTechnology,
                 "Expected at least one monitoring technology.");
    return false;
  }
  // We don't support any options right now.
  if (!options.empty()) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::manager::kInvalidMonitoringOption,
                 "Did not expect any options to monitoring.");
    return false;
  }
  // Translate the technologies we're given to our internal bitmap
  // representation.
  technologies::TechnologySet combined;
  for (const auto& tech_text : requested_technologies) {
    if (!technologies::add_to(tech_text, &combined)) {
      Error::AddToPrintf(error,
                         FROM_HERE,
                         kPeerdErrorDomain,
                         errors::manager::kInvalidMonitoringTechnology,
                         "Invalid monitoring technology: %s.",
                         tech_text.c_str());
      return false;
    }
  }
  // Right now we don't support bluetooth technologies.
  if (combined.test(technologies::kBT) || combined.test(technologies::kBTLE)) {
      Error::AddTo(error,
                   FROM_HERE,
                   kPeerdErrorDomain,
                   errors::manager::kInvalidMonitoringTechnology,
                   "Unsupported monitoring technology.");
      return false;
  }
  *monitoring_token = "monitoring_" +
                      std::to_string(++monitoring_tokens_issued_);
  monitoring_requests_[*monitoring_token] = combined;
  UpdateMonitoredTechnologies();
  // TODO(wiley): Monitor DBus identifier for disconnect.
  return true;
}

bool Manager::StopMonitoring(chromeos::ErrorPtr* error,
                             const string& monitoring_token) {
  auto it = monitoring_requests_.find(monitoring_token);
  if (it == monitoring_requests_.end()) {
    Error::AddToPrintf(error,
                       FROM_HERE,
                       kPeerdErrorDomain,
                       errors::manager::kInvalidMonitoringToken,
                       "Unknown monitoring token: %s.",
                       monitoring_token.c_str());
    return false;
  }
  monitoring_requests_.erase(it);
  UpdateMonitoredTechnologies();
  return true;
}


bool Manager::ExposeService(chromeos::ErrorPtr* error,
                            dbus::Message* message,
                            const string& service_id,
                            const map<string, string>& service_info,
                            const map<string, Any>& options) {
  VLOG(1) << "Exposing service '" << service_id << "'.";
  if (service_id == kSerbusServiceId) {
    Error::AddToPrintf(error,
                       FROM_HERE,
                       kPeerdErrorDomain,
                       errors::service::kInvalidServiceId,
                       "Cannot expose a service named %s",
                       kSerbusServiceId);
    return false;
  }
  auto it = exposed_services_.find(service_id);
  // Regardless of whether |it| is valid, it will become invalid in this block.
  if (it != exposed_services_.end()) {
    // TODO(wiley) We should be updating, but for now, remove and add again.
    //             brbug.com/14 .
    if (!RemoveExposedService(error, message, service_id)) {
      return false;
    }
  }
  if (!self_->AddPublishedService(error, service_id, service_info, options)) {
    return false;
  }
  // TODO(wiley) Maybe trigger an advertisement run since we have updated
  //             information.
  base::Closure on_connection_vanish = base::Bind(
      &Manager::OnDBusServiceDeath, base::Unretained(this), service_id);
  exposed_services_[service_id].reset(
      new DBusServiceWatcher{bus_, message->GetSender(), on_connection_vanish});
  return true;
}

bool Manager::RemoveExposedService(chromeos::ErrorPtr* error,
                                   dbus::Message* message,
                                   const string& service_id) {
  auto it = exposed_services_.find(service_id);
  if (it == exposed_services_.end()) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::manager::kUnknownServiceId,
                 "Unknown service id given to RemoveExposedService.");
    return false;
  }
  if (it->second->connection_name() != message->GetSender()) {
    Error::AddToPrintf(
        error, FROM_HERE, kPeerdErrorDomain, errors::manager::kNotOwner,
        "Service %s is owned by another local process.", service_id.c_str());
    return false;
  }
  bool success = self_->RemoveService(error, it->first);
  // Maybe RemoveService returned with an error, but either way, we should
  // forget this service.
  exposed_services_.erase(it);
  // TODO(wiley) Maybe trigger an advertisement run since we have updated
  //             information.
  return success;
}

string Manager::Ping() {
  return kPingResponse;
}

void Manager::ShouldRefreshAvahiPublisher() {
  LOG(INFO) << "Publishing services to mDNS";
  // The old publisher has been invalidated, and the records pulled.  We should
  // re-register the records we care about.
  self_->RegisterServicePublisher(
      avahi_client_->GetPublisher(self_->GetUUID()));
}

void Manager::UpdateMonitoredTechnologies() {
  technologies::TechnologySet combined;
  for (const auto& request : monitoring_requests_) {
    combined |= request.second;
  }
  dbus_adaptor_.SetMonitoredTechnologies(
      technologies::techs2strings(combined));
  if (combined.test(technologies::kMDNS)) {
    // Let the AvahiClient worry about if we're already monitoring.
    avahi_client_->StartMonitoring();
  } else {
    avahi_client_->StopMonitoring();
  }
}

void Manager::OnDBusServiceDeath(const std::string& service_id) {
  auto it = exposed_services_.find(service_id);
  if (it == exposed_services_.end()) { return; }
  self_->RemoveService(nullptr, it->first);
  exposed_services_.erase(it);
}

}  // namespace peerd
