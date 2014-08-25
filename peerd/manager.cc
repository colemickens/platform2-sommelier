// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager.h"

#include "peerd/dbus_constants.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::ErrorPtr;
using peerd::dbus_constants::kPingResponse;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace peerd {

Manager::Manager(const scoped_refptr<dbus::Bus>& bus) : proxy_(bus, this) {
}

void Manager::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  // Since we have no async init of our own, we can just give our
  // callback to the proxy.
  proxy_.RegisterAsync(completion_callback);
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
