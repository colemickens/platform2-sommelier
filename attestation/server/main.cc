// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/command_line.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/syslog_logging.h>

#include "attestation/server/attestation_service.h"

using chromeos::dbus_utils::AsyncEventSequencer;

class AttestationDaemon : public chromeos::DBusServiceDaemon {
 public:
  AttestationDaemon()
      : chromeos::DBusServiceDaemon(attestation::kAttestationServiceName) {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    service_.reset(new attestation::AttestationService(bus_));
    service_->RegisterAsync(
        sequencer->GetHandler("Service.RegisterAsync() failed.", true));
  }

 private:
  std::unique_ptr<attestation::AttestationService> service_;

  DISALLOW_COPY_AND_ASSIGN(AttestationDaemon);
};

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  AttestationDaemon daemon;
  return daemon.Run();
}
