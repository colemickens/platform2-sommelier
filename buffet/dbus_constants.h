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

// Interface implemented by the object at kManagerServicePath.
extern const char kManagerInterface[];
extern const char kManagerServicePath[];

// Methods exposed as part of kManagerInterface.
extern const char kManagerCheckDeviceRegistered[];
extern const char kManagerGetDeviceInfo[];
extern const char kManagerStartRegisterDevice[];
extern const char kManagerFinishRegisterDevice[];
extern const char kManagerUpdateStateMethod[];
extern const char kManagerAddCommand[];
extern const char kManagerTestMethod[];

// Interface implemented by the command instance objects.
extern const char kCommandInterface[];
extern const char kCommandServicePathPrefix[];

// Methods exposed as part of kCommandInterface.
extern const char kCommandSetProgress[];
extern const char kCommandAbort[];
extern const char kCommandCancel[];
extern const char kCommandDone[];

// Properties exposed as part of kCommandInterface.
extern const char kCommandName[];
extern const char kCommandCategory[];
extern const char kCommandId[];
extern const char kCommandStatus[];
extern const char kCommandProgress[];
extern const char kCommandParameters[];

}  // namespace dbus_constants

}  // namespace buffet

#endif  // BUFFET_DBUS_CONSTANTS_H_
