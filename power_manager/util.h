// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_UTIL_H_
#define POWER_MANAGER_UTIL_H_

namespace power_manager {

namespace util {

// interface names
extern const char* kLowerPowerManagerInterface;

// powerd -> powerm constants
extern const char* kRestartSignal;
extern const char* kRequestCleanShutdown;
extern const char* kSuspendSignal;
extern const char* kShutdownSignal;

// powerm -> powerd constants
extern const char* kLidClosed;
extern const char* kLidOpened;
extern const char* kPowerButtonDown;
extern const char* kPowerButtonUp;

// broadcast signals
extern const char* kPowerStateChanged;

bool LoggedIn();
void Launch(const char* cmd);

// Send a message |signal| to the session manager.
void SendSignalToSessionManager(const char* signal);

// Send a message |signal| to the privileged power daemon.
void SendSignalToPowerM(const char* signal);

// Send a message |signal| to the unprivileged power daemon.
void SendSignalToPowerD(const char* signal);

}  // namespace util

}  // namespace power_manager

#endif  // POWER_MANAGER_UTIL_H_
