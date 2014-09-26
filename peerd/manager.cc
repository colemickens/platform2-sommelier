// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager.h"

#include <base/format_macros.h>
#include <base/guid.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <dbus/object_path.h>

#include "peerd/dbus_constants.h"
#include "peerd/ip_addr.h"
#include "peerd/peer.h"

using chromeos::Error;
using chromeos::ErrorPtr;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;
using dbus::ObjectPath;
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
using std::vector;

namespace peerd {

namespace errors {
namespace manager {

const char kInvalidServiceToken[] = "manager.service_token";
const char kInvalidMonitoringToken[] = "manager.monitoring_token";

}  // namespace manager
}  // namespace errors

Manager::Manager(ExportedObjectManager* object_manager)
  : dbus_object_{object_manager, object_manager->GetBus(),
                 ObjectPath(kManagerServicePath)},
    avahi_client_{object_manager->GetBus()} {
}

void Manager::RegisterAsync(const CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  chromeos::dbus_utils::DBusInterface* itf =
      dbus_object_.AddOrGetInterface(kManagerInterface);

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
  self_ = Peer::MakePeer(
      &error,
      dbus_object_.GetObjectManager().get(),
      ObjectPath(kSelfPath),
      base::GenerateGUID(),  // Every boot is a new GUID for now.
      "CrOS Core Device",  // TODO(wiley): persist name to disk.
      "",  // TODO(wiley): persist note to disk.
      0,  // Last seen time for self is defined to be 0.
      sequencer->GetHandler("Failed exporting Self.", true));
  CHECK(self_) << "Failed to construct Peer for Self.";
  dbus_object_.RegisterAsync(
      sequencer->GetHandler("Failed exporting Manager.", true));
  avahi_client_.RegisterOnAvahiRestartCallback(
      base::Bind(&Manager::ShouldRefreshAvahiPublisher,
                 base::Unretained(this)));
  avahi_client_.RegisterAsync(
      sequencer->GetHandler("Failed AvahiClient.RegisterAsync().", true));
  sequencer->OnAllTasksCompletedCall({completion_callback});
}

string Manager::StartMonitoring(ErrorPtr* error,
                                const set<string>& technologies) {
  return "a_monitor_token";
}

void Manager::StopMonitoring(ErrorPtr* error,
                             const string& monitoring_token) {
}

string Manager::ExposeService(chromeos::ErrorPtr* error,
                              const string& service_id,
                              const map<string, string>& service_info) {
  VLOG(1) << "Exposing service '" << service_id << "'.";
  string token;
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
  self_->RegisterServicePublisher(avahi_client_.GetPublisher());
}

}  // namespace peerd
