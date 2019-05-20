// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DATA_TYPES_H_
#define SHILL_DATA_TYPES_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

namespace shill {

using ByteArray = std::vector<uint8_t>;
using ByteArrays = std::vector<ByteArray>;
// Note that while the RpcIdentifiers type has the same concrete
// representation as the Strings type, it may be serialized
// differently. Accordingly, PropertyStore tracks RpcIdentifiers
// separately from Strings. We create a separate typedef here, to make
// the PropertyStore-related code read more simply.
using RpcIdentifier = std::string;
using RpcIdentifiers = std::vector<std::string>;
using Strings = std::vector<std::string>;
using Stringmap = std::map<std::string, std::string>;
using Stringmaps = std::vector<Stringmap>;
using Uint16s = std::vector<uint16_t>;

}  // namespace shill

#endif  // SHILL_DATA_TYPES_H_
