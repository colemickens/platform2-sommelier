// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_SERVICE_NAME_H_
#define SOMA_SERVICE_NAME_H_

#include <string>
#include <vector>

namespace base {
class ListValue;
}

namespace soma {
namespace parser {
namespace service_name {

extern const char kListKey[];

// Returns true if annotations can be successfully parsed into service_names
// Returns false on failure, and service_names may be in an inconsistent state.
bool ParseList(const base::ListValue* annotations,
               std::vector<std::string>* service_names);

}  // namespace service_name
}  // namespace parser
}  // namespace soma

#endif  // SOMA_SERVICE_NAME_H_
