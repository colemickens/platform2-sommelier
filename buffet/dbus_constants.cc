// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/dbus_constants.h"

namespace buffet {

namespace dbus_constants {

const char kServiceName[] = "org.chromium.Buffet";

const char kRootInterface[] = "org.chromium.Buffet";
const char kRootServicePath[] = "/org/chromium/Buffet";

const char kRootTestMethod[] = "TestMethod";

const char kManagerInterface[] = "org.chromium.Buffet.Manager";
const char kManagerServicePath[] = "/org/chromium/Buffet/Manager";

const char kManagerUpdateStateMethod[] = "UpdateState";
const char kManagerRegisterDeviceMethod[] = "RegisterDevice";

}  // namespace dbus_constants

}  // namespace buffet
