// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_ERROR_
#define SHILL_CELLULAR_ERROR_

#include <dbus-c++/error.h>

#include "shill/error.h"

namespace shill {

class CellularError {
 public:
  static void FromDBusError(const DBus::Error &dbus_error, Error *error);

 private:
  DISALLOW_COPY_AND_ASSIGN(CellularError);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_ERROR_
