// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/cellular_error.h"

#include <string>

#include <mm/mm-modem.h>

using std::string;

#define MM_MODEM_ERROR(error)  MM_MODEM_INTERFACE "." error
#define MM_MOBILE_ERROR(error) MM_MODEM_GSM_INTERFACE "." error

namespace shill {

static const char kErrorIncorrectPassword[] =
    MM_MOBILE_ERROR(MM_ERROR_MODEM_GSM_INCORRECTPASSWORD);
static const char kErrorSimPinRequired[] =
    MM_MOBILE_ERROR(MM_ERROR_MODEM_GSM_SIMPINREQUIRED);
static const char kErrorSimPukRequired[] =
    MM_MOBILE_ERROR(MM_ERROR_MODEM_GSM_SIMPUKREQUIRED);
static const char kErrorGprsNotSubscribed[] =
    MM_MOBILE_ERROR(MM_ERROR_MODEM_GSM_GPRSNOTSUBSCRIBED);

// static
void CellularError::FromChromeosDBusError(brillo::Error* dbus_error,
                                          Error* error) {
  if (!error)
    return;

  if (!dbus_error) {
    error->Reset();
    return;
  }

  const string name = dbus_error->GetCode();
  const string msg = dbus_error->GetMessage();
  Error::Type type;

  if (name == kErrorIncorrectPassword)
    type = Error::kIncorrectPin;
  else if (name == kErrorSimPinRequired)
    type = Error::kPinRequired;
  else if (name == kErrorSimPukRequired)
    type = Error::kPinBlocked;
  else if (name == kErrorGprsNotSubscribed)
    type = Error::kInvalidApn;
  else
    type = Error::kOperationFailed;

  if (!msg.empty())
    return error->Populate(type, msg);
  else
    return error->Populate(type);
}

}  // namespace shill
