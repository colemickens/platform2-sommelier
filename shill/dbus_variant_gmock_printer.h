// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_VARIANT_GMOCK_PRINTER_H_
#define SHILL_DBUS_VARIANT_GMOCK_PRINTER_H_

#include <dbus-c++/types.h>

namespace DBus {

// A GMock printer for DBus::Variant types.  This also suppresses
// GMock from attempting (and failing) to generate its own printer
// for DBus::Variant.
void PrintTo(const ::DBus::Variant& var, ::std::ostream* os);

}  // namespace DBus

#endif  // SHILL_DBUS_VARIANT_GMOCK_PRINTER_H_
