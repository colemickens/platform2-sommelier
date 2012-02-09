// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_UTIL_H_
#define POWER_MANAGER_UTIL_H_

#include <string>

#include <dbus/dbus-glib-lowlevel.h>

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

// Send an empty reply to a D-Bus message.
void SendEmptyDBusReply(DBusConnection* connection, DBusMessage* message);

// Status file creation and removal.
void CreateStatusFile(const FilePath& file);
void RemoveStatusFile(const FilePath& file);

// Send an X ClientEvent message to the window manager.
bool SendMessageToWindowManager(chromeos::WmIpcMessageType type,
                                int first_param);

// Get the current wakeup count from sysfs
bool GetWakeupCount(unsigned int *value);

}  // namespace util
}  // namespace power_manager

#endif  // POWER_MANAGER_UTIL_H_
