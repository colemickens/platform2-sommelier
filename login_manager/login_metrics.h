// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_LOGIN_METRICS_H_
#define LOGIN_MANAGER_LOGIN_METRICS_H_

namespace login_manager {
class LoginMetrics {
public:
  LoginMetrics();
  ~LoginMetrics();

  // Sends the type of user that logs in (guest, owner or other) and the mode
  // (developer or normal) to UMA by using the metrics library.
  virtual void SendLoginUserType(bool dev_mode, bool incognito, bool owner);

private:
  static const char kLoginUserTypeMetric[];
  static const int kGuestUser;
  static const int kOwner;
  static const int kOther;
  static const int kDevMode;
  static const int kMaxValue;

  // Returns code to send to the metrics library based on the type of user
  // (owner, guest or other) and the mode (normal or developer).
  virtual int LoginUserTypeCode(bool dev_mode, bool incognito, bool owner);

};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_LOGIN_METRICS_H_
