// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_ANNOTATIONS_H_
#define SOMA_LIB_SOMA_ANNOTATIONS_H_

#include <string>
#include <vector>

#include <soma/soma_export.h>

namespace base {
class DictionaryValue;
class ListValue;
}

namespace soma {
namespace parser {
namespace annotations {

extern const char kListKey[];
extern const char kPersistentKey[];

// Returns a properly-formed key for a service name annotation.
std::string MakeServiceNameKey(size_t index);

// Returns true if annotations can be successfully parsed into service_names.
// Returns false on failure, and service_names may be in an inconsistent state.
bool ParseServiceNameList(const base::ListValue& annotations,
                          std::vector<std::string>* service_names);

// Returns true if annotations indicates persistence.
// Returns false if not.
bool IsPersistent(const base::ListValue& annotations);

// Adds a notation indicating persistence to |to_modify|. For use in unit tests.
// Returns false if the notation cannot be added.
SOMA_EXPORT bool AddPersistentAnnotationForTest(
    base::DictionaryValue* to_modify);

}  // namespace annotations
}  // namespace parser
}  // namespace soma

#endif  // SOMA_LIB_SOMA_ANNOTATIONS_H_
