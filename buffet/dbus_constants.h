// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_DBUS_CONSTANTS_H_
#define BUFFET_DBUS_CONSTANTS_H_

namespace buffet {
namespace dbus_constants {

// The service name claimed by the Buffet daemon.
extern const char kServiceName[];

// The object at this path implements the ObjectManager interface.
extern const char kRootServicePath[];

// D-Bus object path prefix for Command objects.
extern const char kCommandServicePathPrefix[];

}  // namespace dbus_constants
}  // namespace buffet

#endif  // BUFFET_DBUS_CONSTANTS_H_
