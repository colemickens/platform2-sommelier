// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_ANNOTATIONS_H_
#define SOMA_LIB_SOMA_ANNOTATIONS_H_

#include <string>
#include <vector>

namespace base {
class ListValue;
}

namespace soma {
namespace parser {
namespace annotations {

extern const char kListKey[];
extern const char kPersistentKey[];

std::string MakeServiceNameKey(size_t index);

// Returns true if annotations can be successfully parsed into service_names.
// Returns false on failure, and service_names may be in an inconsistent state.
bool ParseServiceNameList(const base::ListValue& annotations,
                          std::vector<std::string>* service_names);

// Returns true if annotations indicates persistence.
// Returns false if not.
bool IsPersistent(const base::ListValue& annotations);

}  // namespace annotations
}  // namespace parser
}  // namespace soma

#endif  // SOMA_LIB_SOMA_ANNOTATIONS_H_
