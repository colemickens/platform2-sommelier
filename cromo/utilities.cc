// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Plugin tests link against this file, but not against the rest of
// cromo.  Therefore this file should not have dependencies on the
// rest of cromo.
#include "utilities.h"

#include <glog/logging.h>
#include <stdio.h>
#include <stdlib.h>

namespace utilities {

const char *ExtractString(const DBusPropertyMap properties,
                          const char *key,
                          const char *not_found_response,
                          DBus::Error &error) {
  DBusPropertyMap::const_iterator p;
  const char *to_return = not_found_response;
  try {
    p = properties.find(key);
    if (p != properties.end()) {
      to_return = p->second.reader().get_string();
    }
  } catch (const DBus::Error &e) {
    LOG(ERROR)<<"Bad type for: " << key;
    // Setting an already-set error causes an assert fail inside dbus-c++.
    if (!error.is_set()) {
    // NB: the copy constructor for DBus::Error causes a template to
    // be instantiated that kills our -Werror build
      error.set(e.name(), e.message());
    }
  }
  return to_return;
}

bool HexEsnToDecimal(const std::string &esn_hex, std::string *out) {
  size_t length = esn_hex.length();
  if (length > 8) {
    LOG(ERROR) << "Long ESN: " << esn_hex;
    return false;
  }
  errno = 0;
  const char *start = esn_hex.c_str();
  char *end;
  uint32_t esn = strtoul(start, &end, 16);
  if (errno != 0 || *end != '\0') {
    LOG(ERROR) << "Bad ESN: " << esn_hex;
    return false;
  }
  uint32_t mfr = (esn >> 24) & 0xff;
  uint32_t serial = esn & 0x00ffffff;

  // Decimal ESN is 11 chars
  char decimal[12];
  memset(decimal, '\0', sizeof(decimal));
  int rc = snprintf(decimal, sizeof(decimal), "%03d%08d", mfr, serial);
  if (rc != 11) {
    LOG(ERROR) << "Format failure";
    return false;
  }
  if (out) {
    *out = decimal;
  }
  return true;
}
}  // namespace utilities
