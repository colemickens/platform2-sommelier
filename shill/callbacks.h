// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CALLBACKS_H_
#define SHILL_CALLBACKS_H_

#include <string>
#include <vector>

#include <base/callback.h>

#include "shill/accessor_interface.h"
#include "shill/dbus_properties.h"

namespace shill {

class Error;
// Convenient typedefs for some commonly used callbacks.
typedef base::Callback<void(const Error &)> ResultCallback;
typedef base::Callback<void(const Error &, bool)> ResultBoolCallback;
typedef base::Callback<void(const Error &,
                            const std::string &)> ResultStringCallback;
typedef base::Callback<void(const Error &)> EnabledStateChangedCallback;
typedef base::Callback<void(const DBusPropertiesMap &,
                            const Error &)> DBusPropertyMapCallback;
typedef base::Callback<void(const std::vector<DBusPropertiesMap> &,
                            const Error &)> DBusPropertyMapsCallback;
typedef base::Callback<void(const DBusEnumValueMap &,
                            const Error &)> DBusEnumValueMapCallback;
typedef base::Callback<void(const DBus::Path &,
                            const Error &)> DBusPathCallback;
typedef base::Callback<void(
    const std::vector<DBus::Path> &, const Error &)> DBusPathsCallback;
typedef base::Callback<void(const std::string &, const Error &)> StringCallback;
typedef base::Callback<void(uint32_t, uint32_t, const DBusPropertiesMap &)>
    ActivationStateSignalCallback;
typedef base::Callback<void(const Stringmaps &,
                            const Error &)> ResultStringmapsCallback;

}  // namespace shill

#endif  // SHILL_CALLBACKS_H_
