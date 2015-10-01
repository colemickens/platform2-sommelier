// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_DBUS_CONSTANTS_H_
#define FIDES_DBUS_CONSTANTS_H_

namespace fides {

// The service name claimed by the Fides daemon.
extern const char kServiceName[];

// The object at this path implements the ObjectManager interface.
extern const char kRootServicePath[];

// The object at this path implements the SettingsInterface for system-wide
// configuration.
extern const char kSystemSettingsServicePath[];

}  // namespace fides

#endif  // FIDES_DBUS_CONSTANTS_H_
