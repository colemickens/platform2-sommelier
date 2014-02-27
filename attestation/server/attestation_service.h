// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_SERVER_SERVICE_H
#define ATTESTATION_SERVER_SERVICE_H

#include <string>

#include <base/callback.h>
#include <base/memory/scoped_ptr.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

#include "attestation/common/dbus_interface.h"

namespace attestation {

class AttestationService;

typedef scoped_ptr<dbus::Response> ResponsePtr;
// Pointer to a member function for handling D-Bus method calls. If an empty
// scoped_ptr is returned, an empty (but successful) response will be sent.
typedef base::Callback<ResponsePtr (dbus::MethodCall*)> DBusMethodCallHandler;

// Main class within the attestation daemon that ties other classes together.
class AttestationService {
 public:
  AttestationService();
  virtual ~AttestationService();

  // Connects to D-Bus system bus and exports methods.
  void Init();

 private:
  // Exports |method_name| and uses |handler| to handle calls.
  void ExportDBusMethod(const std::string& method_name,
                        const DBusMethodCallHandler& handler);

  // Callbacks for handling D-Bus signals and method calls.
  ResponsePtr HandleStatsMethod(dbus::MethodCall* method_call);

  base::Time start_time_;
  scoped_refptr<dbus::Bus> bus_;
  // This is owned by bus_.
  dbus::ExportedObject* attestation_dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(AttestationService);
};

} // namespace attestation

#endif // ATTESTATION_SERVER_SERVICE_H
