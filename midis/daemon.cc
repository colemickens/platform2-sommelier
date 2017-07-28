// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>
#include <base/memory/ptr_util.h>
#include <dbus/bus.h>
#include <dbus/message.h>

#include "midis/daemon.h"

namespace {

// TODO(pmalani): Move these into system_api once names are finalized.
constexpr char kMidisServiceName[] = "org.chromium.Midis";
constexpr char kMidisServicePath[] = "/org/chromium/Midis";
constexpr char kMidisInterfaceName[] = "org.chromium.Midis";
constexpr char kBootstrapMojoConnectionMethod[] = "BootstrapMojoConnection";

}  // namespace

namespace midis {

Daemon::Daemon()
    : device_tracker_(base::MakeUnique<DeviceTracker>()),
      client_tracker_(base::MakeUnique<ClientTracker>()),
      weak_factory_(this) {}

Daemon::~Daemon() {}

int Daemon::OnInit() {
  if (!device_tracker_->InitDeviceTracker()) {
    return -1;
  }

  if (!client_tracker_->InitClientTracker(device_tracker_.get())) {
    return -1;
  }

  InitDBus();
  return 0;
}

void Daemon::InitDBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  CHECK(bus->Connect());
  dbus::ExportedObject* exported_object =
      bus->GetExportedObject(dbus::ObjectPath(kMidisServicePath));

  CHECK(exported_object);
  CHECK(exported_object->ExportMethodAndBlock(
      kMidisInterfaceName,
      kBootstrapMojoConnectionMethod,
      base::Bind(&Daemon::BootstrapMojoConnection,
                 weak_factory_.GetWeakPtr())));
  CHECK(bus->RequestOwnershipAndBlock(kMidisServiceName,
                                      dbus::Bus::REQUIRE_PRIMARY));
  LOG(INFO) << "D-Bus Registration succeeded";
}

void Daemon::BootstrapMojoConnection(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  LOG(INFO) << "Successfully received call from D-Bus client";
  // TODO(pmalani): Actually bootstrap mojo interface in ClientTracker
  // handle.value() contains the good stuff.
}

}  // namespace midis
