// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CELLULAR_ERROR_H_
#define SHILL_CELLULAR_CELLULAR_ERROR_H_

#include <chromeos/errors/error.h>

#include "shill/error.h"

namespace shill {

class CellularError {
 public:
  static void FromChromeosDBusError(chromeos::Error* dbus_error,
                                    Error* error);

  static void FromMM1ChromeosDBusError(chromeos::Error* dbus_error,
                                       Error* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(CellularError);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CELLULAR_ERROR_H_
