// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_CELLULAR_H_
#define CROMO_CELLULAR_H_

#include <base/basictypes.h>

#include <string>

namespace cellular {
// Convert a string representing a hex ESN to one representing a
// decimal ESN.  Returns success.
bool HexEsnToDecimal(const std::string &esn_hex, std::string *out);
}  // namespace cellular

#endif  /* CROMO_CELLULAR_H_ */
