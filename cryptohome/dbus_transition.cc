// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// See dbus_transition.h for details.

#include "dbus_transition.h"

namespace cryptohome {

const char* kDBusErrorResponseEventType = "DBusErrorResponse";
const char* kDBusNoArgResponseEventType = "DBusNoArgResponse";


DBusNoArgResponse::DBusNoArgResponse(DBusGMethodInvocation* context) :
  context_(context) {
}

DBusErrorResponse::DBusErrorResponse(DBusGMethodInvocation* context,
                                     GError* error) :
  context_(context), error_(error) {
}

}  // namespace cryptohome
