// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_PROCESS_MANAGER_SERVICE_INTERFACE_H_
#define LOGIN_MANAGER_PROCESS_MANAGER_SERVICE_INTERFACE_H_

#include <string>
#include <vector>

#include <base/file_path.h>
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

  // Abort the browser process with |signal|.
  virtual void AbortBrowser(int signal) = 0;

  // Check if |pid| is the currently-managed browser process.
  virtual bool IsBrowser(pid_t pid) = 0;

  // Kill the browser, start it again with the provided arg vector.
  virtual void RestartBrowserWithArgs(const std::vector<std::string>& args,
                                      bool args_are_extra) = 0;

  // Set bookkeeping for the browser process to indicate that a session
  // has been started for the given user.
  virtual void SetBrowserSessionForUser(const std::string& username,
                                        const std::string& userhash) = 0;

  // Kick off, and manage, the policy key generation process.
  virtual void RunKeyGenerator(const std::string& username) = 0;

  // Start tracking a new, potentially running key generation job.
  virtual void AdoptKeyGeneratorJob(scoped_ptr<ChildJobInterface> job,
                                    pid_t pid,
                                    guint watcher) = 0;

  // Stop tracking key generation job.
  virtual void AbandonKeyGeneratorJob() = 0;

  // Process a newly-generated owner key for |username|, stored at |key_file|.
  virtual void ProcessNewOwnerKey(const std::string& username,
                                  const base::FilePath& key_file) = 0;
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_PROCESS_MANAGER_SERVICE_INTERFACE_H_
