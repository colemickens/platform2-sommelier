// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/server/attestation_service.h"

#include <string>

#include <base/time/time.h>
#include <dbus/bus.h>
#include <dbus/object_path.h>

namespace attestation {

AttestationService::AttestationService(const scoped_refptr<dbus::Bus>& bus)
    : start_time_{base::Time()},
      dbus_object_{nullptr, bus, dbus::ObjectPath{kAttestationServicePath}} {
}

void AttestationService::RegisterAsync(const CompletionAction& callback) {
  chromeos::dbus_utils::DBusInterface* itf =
      dbus_object_.AddOrGetInterface(kAttestationInterface);

  itf->AddMethodHandler(kStatsMethod,
                        base::Unretained(this),
                        &AttestationService::HandleStatsMethod);

  dbus_object_.RegisterAsync(callback);

  start_time_ = base::Time::Now();
}

StatsResponse AttestationService::HandleStatsMethod(chromeos::ErrorPtr* error) {
  LOG(INFO) << "Received call to stats method.";
  StatsResponse stats;
  stats.set_uptime((base::Time::Now() - start_time_).InSeconds());
  return stats;
}

}  // namespace attestation
