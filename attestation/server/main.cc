// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/syslog_logging.h>
#include <dbus/bus.h>

#include "attestation/server/attestation_service.h"

void TakeServiceOwnership(const scoped_refptr<dbus::Bus>& bus, bool success) {
  CHECK(success) << "Init of one or more objects has failed.";
  CHECK(bus->RequestOwnershipAndBlock(attestation::kAttestationServiceName,
                                      dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << attestation::kAttestationServiceName;
}

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);

  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);

  base::AtExitManager at_exit_manager;  // This is for the message loop.
  base::MessageLoopForIO message_loop;
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  CHECK(bus->Connect());

  attestation::AttestationService service(bus);
  service.RegisterAsync(base::Bind(&TakeServiceOwnership, bus));

  message_loop.Run();
  bus->ShutdownAndBlock();
  return 0;
}
