// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_LOGIN_EVENT_LISTENER_H
#define CHAPS_LOGIN_EVENT_LISTENER_H

#include <string>

namespace chaps {

// LoginEventListener is an interface which must be implemented by objects which
// register to receive notification of login / logout events.
class LoginEventListener {
 public:
  virtual void OnLogin(const std::string& path,
                       const std::string& auth_data) = 0;
  virtual void OnLogout(const std::string& path) = 0;
  virtual void OnChangeAuthData(const std::string& path,
                                const std::string& old_auth_data,
                                const std::string& new_auth_data) = 0;
};

}  // namespace

#endif  // CHAPS_LOGIN_EVENT_LISTENER_H
