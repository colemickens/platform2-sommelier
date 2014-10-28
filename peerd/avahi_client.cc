// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/avahi_client.h"

#include <string>

#include <avahi-common/defs.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <chromeos/dbus/dbus_signal_handler.h>
#include <chromeos/strings/string_utils.h>
#include <dbus/object_proxy.h>

#include "peerd/dbus_constants.h"
#include "peerd/peer_manager_interface.h"
#include "peerd/technologies.h"

using dbus::ObjectPath;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::CallMethodAndBlock;
using chromeos::dbus_utils::ExtractMethodCallResults;
using chromeos::string_utils::SplitAtFirst;
using std::string;

namespace peerd {

AvahiClient::AvahiClient(const scoped_refptr<dbus::Bus>& bus,
                         PeerManagerInterface* peer_manager)
    : bus_{bus},
      peer_manager_(peer_manager) {
}

AvahiClient::~AvahiClient() {
  publisher_.reset();
  StopMonitoring();
  // In unittests, we don't have a server_ since we never call RegisterAsync().
  if (server_) {
    // The Bus would do this for us on destruction, but this prevents
    // callbacks from the proxy after AvahiClient dies.
    server_->Detach();
    server_ = nullptr;
  }
}

void AvahiClient::RegisterAsync(const CompletionAction& completion_callback) {
  server_ = bus_->GetObjectProxy(
      dbus_constants::avahi::kServiceName,
      ObjectPath(dbus_constants::avahi::kServerPath));
  // This callback lives for the lifetime of the ObjectProxy.
  server_->SetNameOwnerChangedCallback(
      base::Bind(&AvahiClient::OnServiceOwnerChanged,
                 weak_ptr_factory_.GetWeakPtr()));
  // Reconnect to our signals on a new Avahi instance.
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  chromeos::dbus_utils::ConnectToSignal(
      server_,
      dbus_constants::avahi::kServerInterface,
      dbus_constants::avahi::kServerSignalStateChanged,
      base::Bind(&AvahiClient::OnServerStateChanged,
                 weak_ptr_factory_.GetWeakPtr()),
      sequencer->GetExportHandler(
          dbus_constants::avahi::kServerInterface,
          dbus_constants::avahi::kServerSignalStateChanged,
          "Failed to subscribe to Avahi state change.",
          true));
  sequencer->OnAllTasksCompletedCall(
      {completion_callback,
       // Get a onetime callback with the initial state of Avahi.
       AsyncEventSequencer::WrapCompletionTask(
            base::Bind(&dbus::ObjectProxy::WaitForServiceToBeAvailable,
                       server_,
                       base::Bind(&AvahiClient::OnServiceAvailable,
                                  weak_ptr_factory_.GetWeakPtr()))),
      });
}

void AvahiClient::RegisterOnAvahiRestartCallback(
    const OnAvahiRestartCallback& cb) {
  avahi_ready_callbacks_.push_back(cb);
  if (avahi_is_up_) {
    // We're not going to see a transition from down to up, so
    // we ought to call the callback now.
    cb.Run();
  }
}

void AvahiClient::StartMonitoring() {
  if (discoverer_) { return; }  // Already monitoring for services to appear.
  should_discover_ = true;
  if (avahi_is_up_) {
    LOG(INFO) << "Starting service discovery over mDNS.";
    discoverer_.reset(new AvahiServiceDiscoverer{bus_, server_, peer_manager_});
    discoverer_->RegisterAsync(
        base::Bind(&AvahiClient::HandleDiscoveryStartupResult,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    LOG(INFO) << "Waiting for Avahi to come up before starting "
                 "service discovery.";
  }
}

void AvahiClient::StopMonitoring() {
  should_discover_ = false;
  discoverer_.reset();
}

string AvahiClient::GetServiceType(const string& service_id) {
  // TODO(wiley) We're hardcoding TCP here, but in theory we could advertise UDP
  //             services.  We'd have to pass that information down from our
  //             DBus interface.
  return "_" + service_id + "._tcp";
}

string AvahiClient::GetServiceId(const string& service_type) {
  string service_id = SplitAtFirst(service_type, '.', false).first;
  if (!service_id.empty()) { service_id = service_id.substr(1); }
  return service_id;
}

void AvahiClient::OnServiceOwnerChanged(const string& old_owner,
                                        const string& new_owner) {
  if (new_owner.empty()) {
    OnServiceAvailable(false);
    return;
  }
  OnServiceAvailable(true);
}

void AvahiClient::OnServerStateChanged(int32_t state, const string& error) {
  VLOG(1) << "OnServerStateChanged fired.";
  HandleServerStateChange(state);
}

void AvahiClient::OnServiceAvailable(bool avahi_is_on_dbus) {
  VLOG(1) << "Avahi is "  << (avahi_is_on_dbus ? "up." : "down.");
  int32_t state = AVAHI_SERVER_FAILURE;
  if (avahi_is_on_dbus) {
    auto resp = CallMethodAndBlock(
        server_, dbus_constants::avahi::kServerInterface,
        dbus_constants::avahi::kServerMethodGetState,
        nullptr);
    if (!resp || !ExtractMethodCallResults(resp.get(), nullptr, &state)) {
      LOG(WARNING) << "Failed to get avahi initial state.  Relying on signal.";
    }
  }
  VLOG(1) << "Initial Avahi state=" << state << ".";
  HandleServerStateChange(state);
}

void AvahiClient::HandleServerStateChange(int32_t state) {
  switch (state) {
    case AVAHI_SERVER_RUNNING: {
      // All host RRs have been established.
      VLOG(1) << "Avahi ready for action.";
      if (avahi_is_up_) {
        LOG(INFO) << "Ignoring redundant Avahi up event.";
        return;
      }
      avahi_is_up_ = true;
      // We're going to lazily instantiate the publisher on demand.
      for (const auto& cb : avahi_ready_callbacks_) {
        cb.Run();
      }
      if (should_discover_) {
        StartMonitoring();
      }
    } break;
    case AVAHI_SERVER_INVALID:
      // Invalid state (initial).
    case AVAHI_SERVER_REGISTERING:
      // Host RRs are being registered.
    case AVAHI_SERVER_COLLISION:
      // There is a collision with a host RR. All host RRs have been withdrawn,
      // the user should set a new host name via avahi_server_set_host_name().
    case AVAHI_SERVER_FAILURE:
      // Some fatal failure happened, the server is unable to proceed.
      avahi_is_up_ = false;
      VLOG(1) << "Avahi is down, resetting publisher, discoverer.";
      publisher_.reset();
      discoverer_.reset();
      break;
    default:
      LOG(ERROR) << "Unknown Avahi server state change to " << state;
      break;
  }
}

base::WeakPtr<ServicePublisherInterface> AvahiClient::GetPublisher(
    const std::string& uuid) {
  base::WeakPtr<ServicePublisherInterface> result;
  if (!avahi_is_up_) { return result; }
  if (!publisher_) {
    string host_name;
    if (!GetHostName(&host_name)) {
      LOG(ERROR) << "Failed to resolve local hostname.  Marking avahi down.";
      avahi_is_up_ = false;
      return result;
    }
    publisher_.reset(new AvahiServicePublisher(
          host_name, uuid, bus_, server_));
  }
  result = publisher_->GetWeakPtr();
  return result;
}

void AvahiClient::HandleDiscoveryStartupResult(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to start discovering services over mDNS.";
    discoverer_.reset();
  } else {
    VLOG(1) << "Service discovery started successfully.";
  }
}

bool AvahiClient::GetHostName(string* hostname) const {
  auto resp = CallMethodAndBlock(
      server_, dbus_constants::avahi::kServerInterface,
      dbus_constants::avahi::kServerMethodGetHostName,
      nullptr);
  return resp && ExtractMethodCallResults(resp.get(), nullptr, hostname);
}

}  // namespace peerd
