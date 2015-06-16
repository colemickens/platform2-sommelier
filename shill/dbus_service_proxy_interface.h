// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_SERVICE_PROXY_INTERFACE_H_
#define SHILL_DBUS_SERVICE_PROXY_INTERFACE_H_

#include <string>

#include "shill/callbacks.h"

namespace shill {

// These are the methods that a DBus service proxy must support. The interface
// is provided so that it can be mocked in tests.
class DBusServiceProxyInterface {
 public:
  typedef base::Callback<
      void(const std::string& name,
           const std::string& old_owner,
           const std::string& new_owner)> NameOwnerChangedCallback;

  virtual ~DBusServiceProxyInterface() {}

  virtual void GetNameOwner(const std::string& name,
                            Error* error,
                            const StringCallback& callback,
                            int timeout) = 0;

  virtual void set_name_owner_changed_callback(
      const NameOwnerChangedCallback& callback) = 0;
};

}  // namespace shill

#endif  // SHILL_DBUS_SERVICE_PROXY_INTERFACE_H_
