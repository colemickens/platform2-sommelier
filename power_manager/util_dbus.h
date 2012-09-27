// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_UTIL_DBUS_H_
#define POWER_MANAGER_UTIL_DBUS_H_

#include <string>

#include "base/basictypes.h"

struct DBusConnection;
struct DBusMessage;

namespace power_manager {
namespace util {

// Queries session manager to see if any user (including guest) has started a
// session by logging into Chrome.
bool IsSessionStarted();

// Sends a message |signal| to the session manager.
void SendSignalToSessionManager(const char* signal);

// Sends a message |signal| to the privileged power daemon.
void SendSignalToPowerM(const char* signal);

// Sends a message |signal| with |value| to the privileged power daemon.
void SendSignalWithUintToPowerM(const char* signal, uint32 value);

// Sends a message |signal| and |string| to the privileged power daemon.
void SendSignalWithStringToPowerM(const char* signal_name, const char* string);

// Sends a message |signal| to the unprivileged power daemon.
void SendSignalToPowerD(const char* signal);

// Sends a message |signal| and int32 to the unprivileged power daemon.
void SendSignalWithIntToPowerD(const char* signal, int value);

// Calls a method |method_name| in powerd that takes an arbitrary
// array of bytes and returns an integer.
// This is a blocking operation.
bool CallMethodInPowerD(const char* method_name,
                        const char* data,
                        uint32 size,
                        int* return_value);

// Creates an empty reply to a D-Bus message.
DBusMessage* CreateEmptyDBusReply(DBusMessage* message);

// Creates an error reply to a D-Bus message
DBusMessage* CreateDBusErrorReply(DBusMessage* message,
                                  const char* error_name,
                                  const char* error_message);

// Parse out the error message and log it for debugging.
void LogDBusError(DBusMessage* message);

}  // namespace util
}  // namespace power_manager

#endif  // POWER_MANAGER_UTIL_DBUS_H_
