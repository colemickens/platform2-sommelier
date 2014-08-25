// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager_dbus_proxy.h"

#include <netinet/in.h>
#include <sys/socket.h>

#include <chromeos/async_event_sequencer.h>
#include <chromeos/dbus_utils.h>
#include <chromeos/map_utils.h>
#include <dbus/object_path.h>

#include "peerd/dbus_constants.h"
#include "peerd/dbus_data_serialization.h"
#include "peerd/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using dbus::ObjectPath;
using peerd::dbus_constants::kManagerExposeIpService;
using peerd::dbus_constants::kManagerInterface;
using peerd::dbus_constants::kManagerPing;
using peerd::dbus_constants::kManagerRemoveExposedService;
using peerd::dbus_constants::kManagerServicePath;
using peerd::dbus_constants::kManagerSetFriendlyName;
using peerd::dbus_constants::kManagerSetNote;
using peerd::dbus_constants::kManagerStartMonitoring;
using peerd::dbus_constants::kManagerStopMonitoring;
using peerd::dbus_constants::kPingResponse;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace peerd {

ManagerDBusProxy::ManagerDBusProxy(const scoped_refptr<dbus::Bus>& bus,
                                   Manager* manager)
  : bus_(bus),
    dbus_object_(nullptr, bus, ObjectPath(kManagerServicePath)),
    manager_(manager) {
}

void ManagerDBusProxy::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  chromeos::dbus_utils::DBusInterface* itf =
      dbus_object_.AddOrGetInterface(kManagerInterface);

  itf->AddMethodHandler(kManagerPing,
                        base::Unretained(this),
                        &ManagerDBusProxy::HandlePing);
  itf->AddMethodHandler(kManagerStartMonitoring,
                        base::Unretained(this),
                        &ManagerDBusProxy::HandleStartMonitoring);
  itf->AddMethodHandler(kManagerStopMonitoring,
                        base::Unretained(this),
                        &ManagerDBusProxy::HandleStopMonitoring);
  itf->AddMethodHandler(kManagerExposeIpService,
                        base::Unretained(this),
                        &ManagerDBusProxy::HandleExposeIpService);
  itf->AddMethodHandler(kManagerRemoveExposedService,
                        base::Unretained(this),
                        &ManagerDBusProxy::HandleRemoveExposedService);
  itf->AddMethodHandler(kManagerSetFriendlyName,
                        base::Unretained(this),
                        &ManagerDBusProxy::HandleSetFriendlyName);
  itf->AddMethodHandler(kManagerSetNote,
                        base::Unretained(this),
                        &ManagerDBusProxy::HandleSetNote);

  dbus_object_.RegisterAsync(completion_callback);
}

string ManagerDBusProxy::HandleStartMonitoring(
    chromeos::ErrorPtr* error,
    const chromeos::dbus_utils::Dictionary& technologies) {
  // We're going to ignore the values of |technologies| dictionary here.
  // We don't use them, but they exist to enable us to easily expand this
  // interface later.
  return manager_->StartMonitoring(error, chromeos::GetMapKeys(technologies));
}

void ManagerDBusProxy:: HandleStopMonitoring(
    chromeos::ErrorPtr* error,
    const string& monitoring_token) {
  manager_->StopMonitoring(error, monitoring_token);
}

string ManagerDBusProxy::HandleExposeIpService(
    chromeos::ErrorPtr* error,
    const string& service_id,
    const vector<sockaddr_storage>& ip_addresses,
    const map<string, string>& service_info,
    const chromeos::dbus_utils::Dictionary& options) {
  // Ignore |options| for now, since we're not accepting options.
  return manager_->ExposeIpService(error, service_id,
                                   ip_addresses, service_info);
}

void ManagerDBusProxy::HandleRemoveExposedService(
    chromeos::ErrorPtr* error,
    const string& service_token) {
  manager_->RemoveExposedService(error, service_token);
}

void ManagerDBusProxy::HandleSetFriendlyName(chromeos::ErrorPtr* error,
                                             const string& name) {
  manager_->SetFriendlyName(error, name);
}

void ManagerDBusProxy::HandleSetNote(chromeos::ErrorPtr* error,
                                     const string& note) {
  manager_->SetNote(error, note);
}

string ManagerDBusProxy::HandlePing(chromeos::ErrorPtr* error) {
  return manager_->Ping(error);
}

}  // namespace peerd
