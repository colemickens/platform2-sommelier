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
using chromeos::dbus_utils::DBusParamReader;
using chromeos::dbus_utils::DBusParamWriter;

namespace peerd {

namespace {

template<typename... Args>
void RedirectDbusCall(Manager* mgr,
                      bool (Manager::*fnc)(chromeos::ErrorPtr* /*error*/,
                                           const std::string& /*sender_addr*/,
                                           Args...),
                      dbus::MethodCall* method_call,
                      chromeos::dbus_utils::ResponseSender sender) {
  chromeos::dbus_utils::DBusMethodResponseBase method_response(method_call,
                                                               sender);
  auto invoke_callback =
      [mgr, fnc, method_call, &method_response](const Args&... args) {
    ErrorPtr error;
    if (!(mgr->*fnc)(&error, method_call->GetSender(), args...)) {
      method_response.ReplyWithError(error.get());
    } else {
      auto response = method_response.CreateCustomResponse();
      dbus::MessageWriter writer(response.get());
      DBusParamWriter::AppendDBusOutParams(&writer, args...);
      method_response.SendRawResponse(response.Pass());
    }
  };
  ErrorPtr param_reader_error;
  dbus::MessageReader reader(method_call);
  if (!DBusParamReader<true, Args...>::Invoke(invoke_callback, &reader,
                                              &param_reader_error)) {
    // Error parsing method arguments.
    method_response.ReplyWithError(param_reader_error.get());
  }
}

}  // namespace

namespace errors {
namespace manager {

const char kInvalidMonitoringOption[] = "manager.option";
const char kInvalidMonitoringTechnology[] = "manager.invalid_technology";
const char kInvalidMonitoringToken[] = "manager.monitoring_token";
const char kInvalidServiceToken[] = "manager.service_token";

}  // namespace manager
}  // namespace errors

Manager::Manager(ExportedObjectManager* object_manager,
                 const string& initial_mdns_prefix)
  : Manager(unique_ptr<DBusObject>{new DBusObject{
                object_manager, object_manager->GetBus(),
                org::chromium::peerd::ManagerAdaptor::GetObjectPath()}},
            unique_ptr<PublishedPeer>{},
            unique_ptr<PeerManagerInterface>{},
            unique_ptr<AvahiClient>{},
            initial_mdns_prefix) {
}

Manager::Manager(unique_ptr<DBusObject> dbus_object,
                 unique_ptr<PublishedPeer> self,
                 unique_ptr<PeerManagerInterface> peer_manager,
                 unique_ptr<AvahiClient> avahi_client,
                 const string& initial_mdns_prefix)
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


void Manager::ExposeService(dbus::MethodCall* method_call,
                            chromeos::dbus_utils::ResponseSender sender) {
  RedirectDbusCall(this, &Manager::ExposeServiceImpl, method_call, sender);
}

bool Manager::ExposeServiceImpl(chromeos::ErrorPtr* error,
                                const std::string& sender,
                                const string& service_id,
                                const map<string, string>& service_info,
                                const map<string, Any>& options,
                                std::string* service_token) {
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
  if (!self_->AddPublishedService(error, service_id, service_info, options)) {
    return false;
  }
  *service_token = "service_token_" + std::to_string(++services_added_);
  service_token_to_id_.emplace(*service_token, service_id);
  // TODO(wiley) Maybe trigger an advertisement run since we have updated
  //             information.
  return true;
}

bool Manager::RemoveExposedService(chromeos::ErrorPtr* error,
                                   const string& service_token) {
  auto it = service_token_to_id_.find(service_token);
  if (it == service_token_to_id_.end()) {
    Error::AddTo(error,
                 FROM_HERE,
                 kPeerdErrorDomain,
                 errors::manager::kInvalidServiceToken,
                 "Invalid service token given to RemoveExposedService.");
    return false;
  }
  bool success = self_->RemoveService(error, it->second);
  // Maybe RemoveService returned with an error, but either way, we should
  // forget this service token.
  service_token_to_id_.erase(it);
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

}  // namespace peerd
