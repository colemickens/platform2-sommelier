// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the set of well-known settings keys that influence the behavior
// of settingsd. Most of these are concerned with the trust configuration, i.e.
// definitions of configuration sources.

#ifndef SETTINGSD_SETTINGS_KEYS_H_
#define SETTINGSD_SETTINGS_KEYS_H_

#include <initializer_list>
#include <string>

namespace settingsd {
namespace keys {

// The prefix for all keys that affect settingsd configuration.
extern const char kSettingsdPrefix[];

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

}  // namespace sources
}  // namespace keys
}  // namespace settingsd

#endif  // SETTINGSD_SETTINGS_KEYS_H_
