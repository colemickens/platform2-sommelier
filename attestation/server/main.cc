// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <base/command_line.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/syslog_logging.h>

#include "attestation/common/dbus_interface.h"
#include "attestation/server/attestation_service.h"
#include "attestation/server/dbus_service.h"

using chromeos::dbus_utils::AsyncEventSequencer;

class AttestationDaemon : public chromeos::DBusServiceDaemon {
 public:
  AttestationDaemon()
      : chromeos::DBusServiceDaemon(attestation::kAttestationServiceName) {
    attestation_service_.reset(new attestation::AttestationService);
    CHECK(attestation_service_->Initialize());
  }

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    dbus_service_.reset(new attestation::DBusService(
        bus_,
        attestation_service_.get()));
    dbus_service_->Register(sequencer->GetHandler("Register() failed.", true));
  }

 private:
  std::unique_ptr<attestation::AttestationInterface> attestation_service_;
  std::unique_ptr<attestation::DBusService> dbus_service_;

  DISALLOW_COPY_AND_ASSIGN(AttestationDaemon);
};

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  AttestationDaemon daemon;
  return daemon.Run();
}
