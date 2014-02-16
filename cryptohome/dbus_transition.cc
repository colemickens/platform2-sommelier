// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// See dbus_transition.h for details.

#include "dbus_transition.h"

namespace cryptohome {

const char* kDBusErrorReplyEventType = "DBusErrorReply";
const char* kDBusReplyEventType = "DBusReply";


DBusReply::DBusReply(DBusGMethodInvocation* context, std::string* reply) :
  context_(context), reply_(reply) {
}

DBusErrorReply::DBusErrorReply(DBusGMethodInvocation* context,
                                     GError* error) :
  context_(context), error_(error) {
}

}  // namespace cryptohome
