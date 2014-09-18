// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/avahi_client.h"

#include <string>

#include <avahi-common/defs.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <dbus/object_proxy.h>

#include "peerd/dbus_constants.h"

using dbus::ObjectPath;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::CallMethodAndBlock;
using chromeos::dbus_utils::ExtractMethodCallResults;
using std::string;

namespace peerd {

AvahiClient::AvahiClient(const scoped_refptr<dbus::Bus>& bus)
    : weak_ptr_factory_(this) {
  server_ = bus->GetObjectProxy(
      dbus_constants::avahi::kServiceName,
      ObjectPath(dbus_constants::avahi::kServerPath));
}

AvahiClient::~AvahiClient() {
  // The Bus would do this for us on destruction, but this prevents
  // callbacks from the proxy after AvahiClient dies.
  server_->Detach();
}

void AvahiClient::RegisterAsync(const CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  server_->ConnectToSignal(dbus_constants::avahi::kServerInterface,
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
       base::Bind(&AvahiClient::ReadInitialState,
                  weak_ptr_factory_.GetWeakPtr())});
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

void AvahiClient::ReadInitialState(bool ignored_success) {
  auto resp = CallMethodAndBlock(
      server_, dbus_constants::avahi::kServerInterface,
      dbus_constants::avahi::kServerMethodGetState,
      nullptr);
  int32_t state;
  // We don't particularly care about errors.  If Avahi happens to be down
  // we'll wait for it to come up and rely on the signals it sends.
  if (ExtractMethodCallResults(resp.get(), nullptr, &state)) {
    HandleServerStateChange(state);
  }
}

void AvahiClient::OnServerStateChanged(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  int32_t state;
  string error;
  if (!reader.PopInt32(&state)) { return; }
  if (!reader.PopString(&error)) {
    LOG(WARNING) << "Failed to parse Avahi error from signal "
                 << dbus_constants::avahi::kServerSignalStateChanged;
    return;
  }
  HandleServerStateChange(state);
}

void AvahiClient::HandleServerStateChange(int32_t state) {
  switch (state) {
    case AVAHI_SERVER_RUNNING:
      // All host RRs have been established.
      avahi_is_up_ = true;
      for (const auto& cb : avahi_ready_callbacks_) {
        cb.Run();
      }
      break;
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
      break;
    default:
      LOG(ERROR) << "Unknown Avahi server state change to " << state;
      break;
  }
}

}  // namespace peerd
