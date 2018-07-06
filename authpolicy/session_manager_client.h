// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_SESSION_MANAGER_CLIENT_H_
#define AUTHPOLICY_SESSION_MANAGER_CLIENT_H_

#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

namespace dbus {
class ObjectProxy;
class Response;
class Signal;
}  // namespace dbus

namespace brillo {
namespace dbus_utils {
class DBusObject;
}  // namespace dbus_utils
}  // namespace brillo

namespace authpolicy {

// Exposes methods from the Session Manager daemon.
class SessionManagerClient {
 public:
  explicit SessionManagerClient(brillo::dbus_utils::DBusObject* dbus_object);
  virtual ~SessionManagerClient();

  // Exposed Session Manager methods.
  // See Session Manager for a description of the arguments.

  // Asynchronous to achieve higher IO queue depth when writing many policies.
  void StoreUnsignedPolicyEx(
      const std::string& descriptor_blob,
      const std::string& policy_blob,
      const base::Callback<void(bool success)>& callback);

  // Blocking for convenience / code simplicity.
  bool ListStoredComponentPolicies(const std::string& descriptor_blob,
                                   std::vector<std::string>* component_ids);

  // Connect to the signal invoked when the session state changes. See
  // session_manager_impl.cc for a list of possible states.
  void ConnectToSessionStateChangedSignal(
      const base::Callback<void(const std::string& state)>& callback);

  // Retrieves the session state immediately. Returns an empty string on error.
  std::string RetrieveSessionState();

 private:
  // Callback called when StoreUnsignedPolicyEx() finishes. Prints errors and
  // calls |callback|.
  void OnPolicyStored(const base::Callback<void(bool success)>& callback,
                      dbus::Response* response);

  // Callback called on SessionStateChanged signal. Calls callback with the new
  // session state.
  void OnSessionStateChanged(
      const base::Callback<void(const std::string& state)>& callback,
      dbus::Signal* signal);

  dbus::ObjectProxy* session_manager_proxy_ = nullptr;  // Not owned.
  base::WeakPtrFactory<SessionManagerClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerClient);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_SESSION_MANAGER_CLIENT_H_
