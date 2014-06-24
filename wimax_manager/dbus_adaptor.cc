// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/dbus_adaptor.h"

#include <chromeos/dbus/service_constants.h>

using std::string;

namespace wimax_manager {

DBusAdaptor::DBusAdaptor(DBus::Connection *connection,
                         const string& object_path)
    : DBus::ObjectAdaptor(*connection, object_path) {
}

DBusAdaptor::~DBusAdaptor() {
}

// static
void DBusAdaptor::SetError(DBus::Error *error, const string &message) {
  error->set(kWiMaxManagerServiceError, message.c_str());
}

}  // namespace wimax_manager
