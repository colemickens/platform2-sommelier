// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the set of well-known settings keys that influence the behavior
// of fides. Most of these are concerned with the trust configuration, i.e.
// definitions of configuration sources.

#ifndef FIDES_SETTINGS_KEYS_H_
#define FIDES_SETTINGS_KEYS_H_

#include <initializer_list>
#include <string>

namespace fides {
namespace keys {

// The prefix for all keys that affect fides configuration.
extern const char kFidesPrefix[];

// Source - prefix to all trust configuration.
extern const char kSources[];

// Key suffixes relevant to source definitions.
namespace sources {

// Name - friendly name for the source.
extern const char kName[];
// Status - setting status string indicating the source's status.
extern const char kStatus[];
// Type - a string identifying the type of source.
extern const char kType[];
// Access - access control rules.
extern const char kAccess[];
// BlobFormat - the blob formats used for parsing.
extern const char kBlobFormat[];
// NVRamIndex - indicates nvram space containing install attributes parameters.
extern const char kNVRamIndex[];

}  // namespace sources
}  // namespace keys
}  // namespace fides

#endif  // FIDES_SETTINGS_KEYS_H_
