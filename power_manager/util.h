// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_UTIL_H_
#define POWER_MANAGER_UTIL_H_

#include "base/file_path.h"
#include "cros/chromeos_wm_ipc_enums.h"

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

// Send a message |signal| to the unprivileged power daemon.
void SendSignalToPowerD(const char* signal);

// Status file creation and removal
void CreateStatusFile(FilePath file);
void RemoveStatusFile(FilePath file);

// Send an X ClientEvent message to the window manager.
bool SendMessageToWindowManager(chromeos::WmIpcMessageType type,
                                int first_param);

}  // namespace util
}  // namespace power_manager

#endif  // POWER_MANAGER_UTIL_H_
