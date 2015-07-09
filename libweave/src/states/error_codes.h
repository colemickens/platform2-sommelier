// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_STATES_ERROR_CODES_H_
#define LIBWEAVE_SRC_STATES_ERROR_CODES_H_

namespace weave {
namespace errors {
namespace state {

// Error domain for state definitions.
extern const char kDomain[];

// State-specific error codes.
extern const char kPackageNameMissing[];
extern const char kPropertyNameMissing[];
extern const char kPropertyNotDefined[];
extern const char kPropertyRedefinition[];

}  // namespace state
}  // namespace errors
}  // namespace weave

#endif  // LIBWEAVE_SRC_STATES_ERROR_CODES_H_
