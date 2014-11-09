// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_SERVER_ATTESTATION_SERVICE_H_
#define ATTESTATION_SERVER_ATTESTATION_SERVICE_H_

#include <string>

#include <base/callback.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/dbus_object.h>

#include "attestation/common/dbus_data_serialization.h"
#include "attestation/common/dbus_interface.h"

namespace attestation {

class AttestationService;
class StatsResponse;
using CompletionAction =
    chromeos::dbus_utils::AsyncEventSequencer::CompletionAction;

// Main class within the attestation daemon that ties other classes together.
class AttestationService {
 public:
  explicit AttestationService(const scoped_refptr<dbus::Bus>& bus);
  virtual ~AttestationService() = default;

  // Connects to D-Bus system bus and exports methods.
  void RegisterAsync(const CompletionAction& callback);

 private:
  // Callbacks for handling D-Bus signals and method calls.
  StatsResponse HandleStatsMethod();

  base::Time start_time_;
  chromeos::dbus_utils::DBusObject dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(AttestationService);
};

}  // namespace attestation

#endif  // ATTESTATION_SERVER_ATTESTATION_SERVICE_H_
