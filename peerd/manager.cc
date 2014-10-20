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
#include "peerd/ip_addr.h"
#include "peerd/peer_manager_impl.h"
#include "peerd/published_peer.h"
#include "peerd/service.h"
#include "peerd/technologies.h"

using chromeos::Error;
using chromeos::ErrorPtr;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::ExportedObjectManager;
using dbus::ObjectPath;
using peerd::constants::kSerbusServiceId;
using peerd::dbus_constants::kManagerExposeService;
using peerd::dbus_constants::kManagerInterface;
using peerd::dbus_constants::kManagerPing;
using peerd::dbus_constants::kManagerRemoveExposedService;
using peerd::dbus_constants::kManagerServicePath;
using peerd::dbus_constants::kManagerSetFriendlyName;
using peerd::dbus_constants::kManagerSetNote;
using peerd::dbus_constants::kManagerStartMonitoring;
using peerd::dbus_constants::kManagerStopMonitoring;
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

const char kInvalidServiceToken[] = "manager.service_token";
const char kInvalidMonitoringTechnology[] = "manager.monitoring_technology";
const char kInvalidMonitoringToken[] = "manager.monitoring_token";

}  // namespace manager
}  // namespace errors

Manager::Manager(ExportedObjectManager* object_manager)
  : Manager(unique_ptr<DBusObject>{
                new DBusObject{object_manager, object_manager->GetBus(),
                               ObjectPath{kManagerServicePath}}},
            unique_ptr<PublishedPeer>{},
            unique_ptr<PeerManagerInterface>{},
            unique_ptr<AvahiClient>{}) {
}

Manager::Manager(unique_ptr<DBusObject> dbus_object,
                 unique_ptr<PublishedPeer> self,
                 unique_ptr<PeerManagerInterface> peer_manager,
                 unique_ptr<AvahiClient> avahi_client)
    : dbus_object_{std::move(dbus_object)},
      self_{std::move(self)},
      peer_manager_{std::move(peer_manager)},
      avahi_client_{std::move(avahi_client)} {
  // If we haven't gotten mocks for these objects, make real ones.
  if (!self_) {
    self_.reset(
        new PublishedPeer{dbus_object_->GetObjectManager()->GetBus(),
                          dbus_object_->GetObjectManager().get(),
                          ObjectPath{kSelfPath}});
  }
  if (!peer_manager_) {
    peer_manager_.reset(
        new PeerManagerImpl{dbus_object_->GetObjectManager()->GetBus(),
                            dbus_object_->GetObjectManager().get()});
  }
  if (!avahi_client_) {
    avahi_client_.reset(
        new AvahiClient{dbus_object_->GetObjectManager()->GetBus(),
                        peer_manager_.get()});
  }
}


void Manager::RegisterAsync(const CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  chromeos::dbus_utils::DBusInterface* itf =
      dbus_object_->AddOrGetInterface(kManagerInterface);

  itf->AddMethodHandler(kManagerStartMonitoring,
                        base::Unretained(this),
                        &Manager::StartMonitoring);
  itf->AddMethodHandler(kManagerStopMonitoring,
                        base::Unretained(this),
                        &Manager::StopMonitoring);
  itf->AddMethodHandler(kManagerExposeService,
                        base::Unretained(this),
                        &Manager::ExposeService);
  itf->AddMethodHandler(kManagerRemoveExposedService,
                        base::Unretained(this),
                        &Manager::RemoveExposedService);
  itf->AddMethodHandler(kManagerSetFriendlyName,
                        base::Unretained(this),
                        &Manager::SetFriendlyName);
  itf->AddMethodHandler(kManagerSetNote,
                        base::Unretained(this),
                        &Manager::SetNote);
  itf->AddMethodHandler(kManagerPing,
                        base::Unretained(this),
                        &Manager::Ping);
  chromeos::ErrorPtr error;
  const bool self_success = self_->RegisterAsync(
      &error,
      base::GenerateGUID(),  // Every boot is a new GUID for now.
      "CrOS Core Device",  // TODO(wiley): persist name to disk.
      "",  // TODO(wiley): persist note to disk.
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

string Manager::StartMonitoring(
    ErrorPtr* error,
    const vector<technologies::tech_t>& requested_technologies) {
  string token;
  if (requested_technologies.empty())  {
    Error::AddTo(error,
                 kPeerdErrorDomain,
                 errors::manager::kInvalidMonitoringTechnology,
                 "Expected at least one monitoring technology.");
    return token;
  }
  technologies::tech_t combined = 0;
  for (technologies::tech_t tech : requested_technologies) {
    if (tech != technologies::kAll &&
        tech != technologies::kMDNS) {
      Error::AddToPrintf(error,
                         kPeerdErrorDomain,
                         errors::manager::kInvalidMonitoringTechnology,
                         "Invalid monitoring technology: %d.", tech);
      return token;
    }
    combined |= tech;
  }
  token = "monitoring_" + std::to_string(++monitoring_tokens_issued_);
  monitoring_requests_[token] = combined;
  if (((technologies::kAll | technologies::kMDNS) & combined) != 0) {
    // Let the AvahiClient worry about if we're already monitoring.
    avahi_client_->StartMonitoring();
  }
  // TODO(wiley): Monitor DBus identifier for disconnect.
  return token;
}

void Manager::StopMonitoring(ErrorPtr* error,
                             const string& monitoring_token) {
  auto it = monitoring_requests_.find(monitoring_token);
  if (it == monitoring_requests_.end()) {
      Error::AddToPrintf(error,
                         kPeerdErrorDomain,
                         errors::manager::kInvalidMonitoringToken,
                         "Unknown monitoring token: %s.",
                         monitoring_token.c_str());
     return;
  }
  monitoring_requests_.erase(it);
  technologies::tech_t combined = 0;
  for (const auto& request : monitoring_requests_) {
    combined |= request.second;
  }
  if (combined & technologies::kAll) {
    // Carry on, everything we had going on should continue.
    return;
  }
  if (!(combined & technologies::kMDNS)) {
    avahi_client_->StopMonitoring();
  }
}

string Manager::ExposeService(chromeos::ErrorPtr* error,
                              const string& service_id,
                              const map<string, string>& service_info) {
  VLOG(1) << "Exposing service '" << service_id << "'.";
  string token;
  if (service_id == kSerbusServiceId) {
    Error::AddToPrintf(error,
                       kPeerdErrorDomain,
                       errors::service::kInvalidServiceId,
                       "Cannot expose a service named %s",
                       kSerbusServiceId);
    return token;
  }
  if (!self_->AddService(error, service_id, {}, service_info)) {
    return token;
  }
  token = "service_token_" + std::to_string(++services_added_);
  service_token_to_id_.emplace(token, service_id);
  // TODO(wiley) Maybe trigger an advertisement run since we have updated
  //             information.
  return token;
}

void Manager::RemoveExposedService(ErrorPtr* error,
                                   const string& service_token) {
  auto it = service_token_to_id_.find(service_token);
  if (it == service_token_to_id_.end()) {
    Error::AddTo(error,
                 kPeerdErrorDomain,
                 errors::manager::kInvalidServiceToken,
                 "Invalid service token given to RemoveExposedService.");
    return;
  }
  self_->RemoveService(error, it->second);
  // Maybe RemoveService returned with an error, but either way, we should
  // forget this service token.
  service_token_to_id_.erase(it);
  // TODO(wiley) Maybe trigger an advertisement run since we have updated
  //             information.
}

void Manager::SetFriendlyName(ErrorPtr* error,
                              const string& friendly_name) {
  self_->SetFriendlyName(error, friendly_name);
}

void Manager::SetNote(ErrorPtr* error, const string& note) {
  self_->SetNote(error, note);
}

string Manager::Ping(ErrorPtr* error) {
  return kPingResponse;
}

void Manager::ShouldRefreshAvahiPublisher() {
  LOG(INFO) << "Publishing services to mDNS";
  // The old publisher has been invalidated, and the records pulled.  We should
  // re-register the records we care about.
  self_->RegisterServicePublisher(avahi_client_->GetPublisher(
        self_->GetUUID(), self_->GetFriendlyName(), self_->GetNote()));
}

}  // namespace peerd
