// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/cellular_error.h"

#include <string>

#include <ModemManager/ModemManager.h>

// TODO(armansito): Once we refactor the code to handle the ModemManager D-Bus
// bindings in a dedicated class, this code should move there.
// (See crbug.com/246425)

namespace shill {

namespace {

const char kErrorGprsMissingOrUnknownApn[] =
    MM_MOBILE_EQUIPMENT_ERROR_DBUS_PREFIX ".GprsMissingOrUnknownApn";

const char kErrorGprsServiceOptionNotSubscribed[] =
    MM_MOBILE_EQUIPMENT_ERROR_DBUS_PREFIX ".GprsServiceOptionNotSubscribed";

const char kErrorGprsUserAuthenticationFailed[] =
    MM_MOBILE_EQUIPMENT_ERROR_DBUS_PREFIX ".GprsUserAuthenticationFailed";

const char kErrorIncorrectPassword[] =
    MM_MOBILE_EQUIPMENT_ERROR_DBUS_PREFIX ".IncorrectPassword";

const char kErrorSimPin[] = MM_MOBILE_EQUIPMENT_ERROR_DBUS_PREFIX ".SimPin";

const char kErrorSimPuk[] = MM_MOBILE_EQUIPMENT_ERROR_DBUS_PREFIX ".SimPuk";

const char kErrorWrongState[] = MM_CORE_ERROR_DBUS_PREFIX ".WrongState";

}  // namespace

// static
void CellularError::FromMM1ChromeosDBusError(brillo::Error* dbus_error,
                                             Error* error) {
  if (!error)
    return;

  if (!dbus_error) {
    error->Reset();
    return;
  }

  const std::string name = dbus_error->GetCode();
  const std::string msg = dbus_error->GetMessage();
  Error::Type type;

  if (name == kErrorIncorrectPassword)
    type = Error::kIncorrectPin;
  else if (name == kErrorSimPin)
    type = Error::kPinRequired;
  else if (name == kErrorSimPuk)
    type = Error::kPinBlocked;
  else if (name == kErrorGprsMissingOrUnknownApn)
    type = Error::kInvalidApn;
  else if (name == kErrorGprsServiceOptionNotSubscribed)
    type = Error::kInvalidApn;
  else if (name == kErrorGprsUserAuthenticationFailed)
    type = Error::kInvalidApn;
  else if (name == kErrorWrongState)
    type = Error::kWrongState;
  else
    type = Error::kOperationFailed;

  if (!msg.empty())
    return error->Populate(type, msg);
  else
    return error->Populate(type);
}

}  // namespace shill
