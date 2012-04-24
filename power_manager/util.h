// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_UTIL_H_
#define POWER_MANAGER_UTIL_H_

#include <dbus/dbus-glib-lowlevel.h>

#include <string>

#include "base/basictypes.h"
#include "cros/chromeos_wm_ipc_enums.h"

class FilePath;

namespace power_manager {
namespace util {

bool LoggedIn();
bool OOBECompleted();

// Issue command asynchronously.
void Launch(const char* cmd);

// Send a message |signal| to the session manager.
void SendSignalToSessionManager(const char* signal);

// Send a message |signal| to the privileged power daemon.
void SendSignalToPowerM(const char* signal);
void SendSignalToPowerM(const char* signal, uint32 value);

// Send a message |signal| to the unprivileged power daemon.
void SendSignalToPowerD(const char* signal);

// Send a message |signal| and int32 to the unprivileged power daemon.
void SendSignalWithIntToPowerD(const char* signal, int value);

// Call a method |method_name| in powerd that takes an arbitrary
// array of bytes and returns an integer.
// This is a blocking operation.
bool CallMethodInPowerD(const char* method_name, const char* data,
                        unsigned int size, int* return_value);

// Create an empty reply to a D-Bus message.
DBusMessage* CreateEmptyDBusReply(DBusMessage* message);

// Create an error reply to a D-Bus message
DBusMessage* CreateDBusErrorReply(DBusMessage* message, const char* error_name,
                                  const char* error_message);

// Status file creation and removal.
void CreateStatusFile(const FilePath& file);
void RemoveStatusFile(const FilePath& file);

// Get the current wakeup count from sysfs
bool GetWakeupCount(unsigned int *value);

}  // namespace util
}  // namespace power_manager

#endif  // POWER_MANAGER_UTIL_H_
