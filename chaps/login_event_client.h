// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_LOGIN_EVENT_CLIENT_H_
#define CHAPS_LOGIN_EVENT_CLIENT_H_

#include <string>

#include <base/basictypes.h>

namespace chaps {

class ChapsProxyImpl;

// Sends login events to the Chaps daemon. Example usage:
//   LoginEventClient client;
//   if (!client.Init())
//     ...
//   client.FireLoginEvent(...);
class LoginEventClient {
 public:
  LoginEventClient();
  virtual ~LoginEventClient();

  // Initializes the client instance. Returns true on success.
  bool Init();

  // Sends a login event. The Chaps daemon will insert a token for the user.
  //  path - The path to the user's token directory.
  //  auth_data - Authorization data to unlock the token.
  void FireLoginEvent(const std::string& path, const std::string& auth_data);

  // Sends a logout event. The Chaps daemon will remove the user's token.
  //  path - The path to the user's token directory.
  void FireLogoutEvent(const std::string& path);

  // Notifies Chaps that a token's authorization data has been changed. The
  // Chaps daemon will re-protect the token with the new data.
  //  path - The path to the user's token directory.
  //  old_auth_data - Authorization data to unlock the token as it is.
  //  new_auth_data - The new authorization data.
  void FireChangeAuthDataEvent(const std::string& path,
                               const std::string& old_auth_data,
                               const std::string& new_auth_data);
 private:
  ChapsProxyImpl* proxy_;

  DISALLOW_COPY_AND_ASSIGN(LoginEventClient);
};

}  // namespace chaps

#endif  // CHAPS_LOGIN_EVENT_CLIENT_H_
