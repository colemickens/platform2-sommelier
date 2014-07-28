// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_DBUS_CONSTANTS_H_
#define PEERD_DBUS_CONSTANTS_H_

namespace peerd {

namespace dbus_constants {

// The service name claimed by peerd.
extern const char kServiceName[];

// Interface implemented by the object at kManagerServicePath.
extern const char kManagerInterface[];
extern const char kManagerServicePath[];

// Methods exposed as part of kManagerInterface.
extern const char kManagerPing[];

}  // namespace dbus_constants

}  // namespace peerd

#endif  // PEERD_DBUS_CONSTANTS_H_
