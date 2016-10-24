// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_ERRORS_H_
#define AUTHPOLICY_ERRORS_H_

namespace errors {

// Brillo error domain, see brillo::Error.
extern const char kAuthPolicy[];

// Brillo error codes, see brillo::Error.
extern const char kPreg[];      // PReg parser error.
extern const char kAuth[];      // Error authenticating.
extern const char kJoin[];      // Error joining the AD domain.
extern const char kFetch[];     // Error fetching policy.
extern const char kDownload[];  // Error downloading GPO.
extern const char kCmd[];       // Error executing a command.

}  // namespace errors

#endif  // AUTHPOLICY_ERRORS_H_
