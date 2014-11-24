// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libbuffet/dbus_constants.h"

namespace buffet {

namespace dbus_constants {

const char kServiceName[] = "org.chromium.Buffet";

const char kRootServicePath[] = "/org/chromium/Buffet";

const char kManagerInterface[] = "org.chromium.Buffet.Manager";
const char kManagerServicePath[] = "/org/chromium/Buffet/Manager";

const char kManagerStartDevice[]            = "StartDevice";
const char kManagerCheckDeviceRegistered[]  = "CheckDeviceRegistered";
const char kManagerGetDeviceInfo[]          = "GetDeviceInfo";
const char kManagerRegisterDevice[]         = "RegisterDevice";
const char kManagerUpdateStateMethod[]      = "UpdateState";
const char kManagerAddCommand[]             = "AddCommand";
const char kManagerTestMethod[]             = "TestMethod";

const char kCommandInterface[] = "org.chromium.Buffet.Command";
const char kCommandServicePathPrefix[] = "/org/chromium/Buffet/commands/";

const char kCommandSetResults[] = "SetResults";
const char kCommandSetProgress[] = "SetProgress";
const char kCommandAbort[] = "Abort";
const char kCommandCancel[] = "Cancel";
const char kCommandDone[] = "Done";

const char kCommandName[] = "Name";
const char kCommandCategory[] = "Category";
const char kCommandId[] = "Id";
const char kCommandStatus[] = "Status";
const char kCommandProgress[] = "Progress";
const char kCommandParameters[] = "Parameters";
const char kCommandResults[] = "Results";

}  // namespace dbus_constants

}  // namespace buffet
