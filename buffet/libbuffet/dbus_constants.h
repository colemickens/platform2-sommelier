// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_LIBBUFFET_DBUS_CONSTANTS_H_
#define BUFFET_LIBBUFFET_DBUS_CONSTANTS_H_

#include "libbuffet/export.h"

namespace buffet {

namespace dbus_constants {

// The service name claimed by the Buffet daemon.
LIBBUFFET_EXPORT extern const char kServiceName[];

// The object at this path implements the ObjectManager interface.
LIBBUFFET_EXPORT extern const char kRootServicePath[];

// Interface implemented by the object at kManagerServicePath.
LIBBUFFET_EXPORT extern const char kManagerInterface[];
LIBBUFFET_EXPORT extern const char kManagerServicePath[];

// Methods exposed as part of kManagerInterface.
LIBBUFFET_EXPORT extern const char kManagerStartDevice[];
LIBBUFFET_EXPORT extern const char kManagerCheckDeviceRegistered[];
LIBBUFFET_EXPORT extern const char kManagerGetDeviceInfo[];
LIBBUFFET_EXPORT extern const char kManagerStartRegisterDevice[];
LIBBUFFET_EXPORT extern const char kManagerFinishRegisterDevice[];
LIBBUFFET_EXPORT extern const char kManagerUpdateStateMethod[];
LIBBUFFET_EXPORT extern const char kManagerAddCommand[];
LIBBUFFET_EXPORT extern const char kManagerTestMethod[];

// Interface implemented by the command instance objects.
LIBBUFFET_EXPORT extern const char kCommandInterface[];
LIBBUFFET_EXPORT extern const char kCommandServicePathPrefix[];

// Methods exposed as part of kCommandInterface.
LIBBUFFET_EXPORT extern const char kCommandSetProgress[];
LIBBUFFET_EXPORT extern const char kCommandAbort[];
LIBBUFFET_EXPORT extern const char kCommandCancel[];
LIBBUFFET_EXPORT extern const char kCommandDone[];

// Properties exposed as part of kCommandInterface.
LIBBUFFET_EXPORT extern const char kCommandName[];
LIBBUFFET_EXPORT extern const char kCommandCategory[];
LIBBUFFET_EXPORT extern const char kCommandId[];
LIBBUFFET_EXPORT extern const char kCommandStatus[];
LIBBUFFET_EXPORT extern const char kCommandProgress[];
LIBBUFFET_EXPORT extern const char kCommandParameters[];

}  // namespace dbus_constants

}  // namespace buffet

#endif  // BUFFET_LIBBUFFET_DBUS_CONSTANTS_H_
