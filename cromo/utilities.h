// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities for plugins for the cromo modem manager.

#ifndef CROMO_UTILITIES_H_
#define CROMO_UTILITIES_H_

#include <base/basictypes.h>
#include <map>
#include <string>

#include <dbus/dbus.h>
#include <dbus-c++/dbus.h>

namespace utilities {

typedef std::map<std::string, DBus::Variant> DBusPropertyMap;

// Extracts the key from proprties, returning not_found_response if
// the key is not found.  If key is found, but is not a string, sets
// error and returns not_found_response.  If error.is_set() is true,
// ExtractString will not report further errors.  You can make
// multiple ExtractString calls and check error at the end.
const char *ExtractString(const DBusPropertyMap properties,
                          const char *key,
                          const char *not_found_response,
                          DBus::Error &error);

// Convert a string representing a hex ESN to one representing a
// decimal ESN.  Returns success.
bool HexEsnToDecimal(const std::string &esn_hex, std::string *out);

}  // namespace utilities

#endif  /* CROMO_UTILITIES_H_ */
