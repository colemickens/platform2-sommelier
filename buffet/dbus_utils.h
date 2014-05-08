// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_DBUS_UTILS_H_
#define BUFFET_DBUS_UTILS_H_

#include <string>

#include <base/memory/scoped_ptr.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

#include "buffet/error.h"

namespace buffet {

namespace dbus_utils {

scoped_ptr<dbus::Response> GetBadArgsError(dbus::MethodCall* method_call,
                                           const std::string& message);

scoped_ptr<dbus::Response> GetDBusError(dbus::MethodCall* method_call,
                                        const chromeos::Error* error);


dbus::ExportedObject::MethodCallCallback GetExportableDBusMethod(
    base::Callback<scoped_ptr<dbus::Response>(dbus::MethodCall*)> handler);

}  // namespace dbus_utils

}  // namespace buffet

#endif  // BUFFET_DBUS_UTILS_H_

