// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/ref_counted.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "attestation/common/dbus_interface.h"
#include "attestation/common/dbus_interface.pb.h"

int main(int argc, char* argv[]) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  dbus::ObjectProxy* object = bus->GetObjectProxy(
      attestation::kAttestationServiceName,
      dbus::ObjectPath(attestation::kAttestationServicePath));
  dbus::MethodCall method_call(
      attestation::kAttestationInterface,
      attestation::kStatsMethod);
  scoped_ptr<dbus::Response> response(
      object->CallMethodAndBlock(&method_call,
                                 dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  attestation::StatsResponse stats;
  dbus::MessageReader reader(response.get());
  reader.PopArrayOfBytesAsProto(&stats);

  printf("Attestation has been up for %u seconds.\n", stats.uptime());

  bus->ShutdownAndBlock();

  return 0;
}
