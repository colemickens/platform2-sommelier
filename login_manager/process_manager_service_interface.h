// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_PROCESS_MANAGER_SERVICE_INTERFACE_H_
#define LOGIN_MANAGER_PROCESS_MANAGER_SERVICE_INTERFACE_H_

#include <string>

#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/abstract_dbus_service.h>

namespace login_manager {

class ChildJobInterface;

class ProcessManagerServiceInterface {
 public:
  ProcessManagerServiceInterface() {}
  virtual ~ProcessManagerServiceInterface() {}

  // Enqueue a QuitClosure.
  virtual void ScheduleShutdown() = 0;

  // Fork, then call browser_.job->Run() in the child and set a
  // babysitter in the parent's glib default context that calls
  // HandleBrowserExit when the child is done.
  virtual void RunBrowser() = 0;

  // Abort the browser process.
  virtual void AbortBrowser() = 0;

  // Check if |pid| is the currently-managed browser process.
  virtual bool IsBrowser(pid_t pid) = 0;

  // Kill the browser, start it again with the provided arg vector.
  virtual void RestartBrowserWithArgs(const std::vector<std::string>& args,
                                      bool args_are_extra) = 0;

  // Set bookkeeping for the browser process to indicate that a session
  // has been started for the given user.
  virtual void SetBrowserSessionForUser(const std::string& user) = 0;

  // Kick off, and manage, the policy key generation process.
  virtual void RunKeyGenerator() = 0;
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_PROCESS_MANAGER_SERVICE_INTERFACE_H_
