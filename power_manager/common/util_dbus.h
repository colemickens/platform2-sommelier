// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_UTIL_DBUS_H_
#define POWER_MANAGER_COMMON_UTIL_DBUS_H_

#include <string>

#include "base/basictypes.h"

struct DBusMessage;

struct _DBusGProxy;
typedef struct _DBusGProxy DBusGProxy;
typedef char gchar;

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace power_manager {
namespace util {

typedef void (*NameOwnerChangedHandler)(DBusGProxy*, const gchar*, const gchar*,
                                        const gchar*, void*);

// Queries session manager to see if any user (including guest) has started a
// session by logging into Chrome.
bool IsSessionStarted();

// Gets session state info.  Returns true if call to session manager was
// successful.  |state| and |user| are both optional args and can be NULL.
bool GetSessionState(std::string* state, std::string* user);

// Emits a signal containing a serialized protocol buffer.  This should be used
// to broadcast signals from powerm to notify arbitrary outside processes when
// things have happened.
void EmitPowerMSignal(const std::string& signal_name,
                      const google::protobuf::MessageLite& protobuf);

// Sends a message |signal| to the session manager.
void SendSignalToSessionManager(const char* signal);

// Sends a message |signal| to the privileged power daemon.
void SendSignalToPowerM(const char* signal);

// Sends a message |signal| with |value| to the privileged power daemon.
void SendSignalWithUintToPowerM(const char* signal, uint32 value);

// Sends a message |signal| and |string| to the privileged power daemon.
void SendSignalWithStringToPowerM(const char* signal_name, const char* string);

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

// Provide a callback for handling NamedOwnerChanged signal.
void SetNameOwnerChangedHandler(NameOwnerChangedHandler callback, void* data);

// Register the current process with dbus under the service name |name|.
void RequestDBusServiceName(const char* name);

}  // namespace util
}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_UTIL_DBUS_H_
