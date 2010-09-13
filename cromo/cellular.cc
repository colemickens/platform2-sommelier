// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cellular.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sstream>

#include <glog/logging.h>


namespace cellular {

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
}  // namespace celluar
