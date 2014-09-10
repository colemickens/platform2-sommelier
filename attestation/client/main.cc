// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom data serialization header must be included before any D-Bus
// headers from libchromeos.
#include "attestation/common/dbus_data_serialization.h"

#include <memory>

#include <base/memory/ref_counted.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <chromeos/errors/error.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "attestation/common/dbus_interface.h"

int main(int argc, char* argv[]) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  dbus::ObjectProxy* object = bus->GetObjectProxy(
      attestation::kAttestationServiceName,
      dbus::ObjectPath(attestation::kAttestationServicePath));

  chromeos::ErrorPtr error;
  auto response = chromeos::dbus_utils::CallMethodAndBlock(
      object,
      attestation::kAttestationInterface,
      attestation::kStatsMethod,
      &error);

  attestation::StatsResponse stats;
  if (response &&
      chromeos::dbus_utils::ExtractMethodCallResults(response.get(),
                                                     &error,
                                                     &stats)) {
    printf("Attestation has been up for %u seconds.\n", stats.uptime());
  } else {
    printf("Error occurred: %s.\n", error->GetMessage().c_str());
  }

  bus->ShutdownAndBlock();

  return 0;
}
