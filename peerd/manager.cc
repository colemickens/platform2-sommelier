// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager.h"

#include <netinet/in.h>
#include <sys/socket.h>

#include <chromeos/dbus/exported_object_manager.h>
#include <dbus/object_path.h>

#include "peerd/dbus_constants.h"

using dbus::ObjectPath;
using chromeos::ErrorPtr;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;
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

Manager::Manager(ExportedObjectManager* object_manager)
  : dbus_object_(object_manager, object_manager->GetBus(),
                 ObjectPath(kManagerServicePath)) {
}

void Manager::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  chromeos::dbus_utils::DBusInterface* itf =
      dbus_object_.AddOrGetInterface(kManagerInterface);

  itf->AddMethodHandler(kManagerStartMonitoring,
                        base::Unretained(this),
                        &Manager::StartMonitoring);
  itf->AddMethodHandler(kManagerStopMonitoring,
                        base::Unretained(this),
                        &Manager::StopMonitoring);
  itf->AddMethodHandler(kManagerExposeIpService,
                        base::Unretained(this),
                        &Manager::ExposeIpService);
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

  dbus_object_.RegisterAsync(completion_callback);
}

string Manager::StartMonitoring(ErrorPtr* error,
                                const set<string>& technologies) {
  return "a_monitor_token";
}

void Manager::StopMonitoring(ErrorPtr* error,
                             const string& monitoring_token) {
}

string Manager::ExposeIpService(chromeos::ErrorPtr* error,
                                const string& service_id,
                                const vector<sockaddr_storage>& addresses,
                                const map<string, string>& service_info) {
  return "a_service_token";
}

void Manager::RemoveExposedService(ErrorPtr* error,
                                   const string& service_token) {
}

void Manager::SetFriendlyName(ErrorPtr* error,
                              const string& friendly_name) {
}

void Manager::SetNote(ErrorPtr* error, const string& note) {
}

string Manager::Ping(ErrorPtr* error) {
  return kPingResponse;
}

}  // namespace peerd
