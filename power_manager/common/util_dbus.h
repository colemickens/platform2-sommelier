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

// Queries session manager to see if any user (including guest) has started a
// session by logging into Chrome.
bool IsSessionStarted();

// Gets session state info.  Returns true if call to session manager was
// successful.  |state| and |user| are both optional args and can be NULL.
bool GetSessionState(std::string* state, std::string* user);

// Parses a single byte array argument from |message| into |protobuf_out|.
// Returns false if the message lacks the argument or if parsing failed.
bool ParseProtocolBufferFromDBusMessage(
    DBusMessage* message,
    google::protobuf::MessageLite* protobuf_out);

// Appends a serialized copy of |protobuf| to |message_out| as a byte array
// argument.
void AppendProtocolBufferToDBusMessage(
    const google::protobuf::MessageLite& protobuf,
    DBusMessage* message_out);

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

// Calls a method |method_name| in powerd that takes a protocol buffer and
// returns an integer.  This is a blocking operation.
bool CallMethodInPowerD(const char* method_name,
                        const google::protobuf::MessageLite& protobuf,
                        int* return_value);

// Creates an empty reply to a D-Bus message.
DBusMessage* CreateEmptyDBusReply(DBusMessage* message);

// Creates a reply to |message| containing |protobuf| as an argument.
// This is just a wrapper around CreateEmptyDBusReply() and
// AppendProtocolBufferToDBusMessage().
DBusMessage* CreateDBusProtocolBufferReply(
    DBusMessage* message,
    const google::protobuf::MessageLite& protobuf);

// Creates an INVALID_ARGS error reply to |message|.
DBusMessage* CreateDBusInvalidArgsErrorReply(DBusMessage* message);

// Creates a generic FAILED error reply to |message|.
DBusMessage* CreateDBusErrorReply(DBusMessage* message,
                                  const std::string& details);

// Parse out the error message and log it for debugging.
void LogDBusError(DBusMessage* message);

// Register the current process with dbus under the service name |name|.
void RequestDBusServiceName(const char* name);

// Returns true if the specified service is connected to D-Bus and false
// otherwise.  If |connection_name_out| is non-NULL, the connection name will be
// assigned to it.
bool IsDBusServiceConnected(const std::string& service_name,
                            const std::string& service_path,
                            const std::string& interface,
                            std::string* connection_name_out);

}  // namespace util
}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_UTIL_DBUS_H_
