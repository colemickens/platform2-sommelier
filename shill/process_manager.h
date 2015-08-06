// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROCESS_MANAGER_H_
#define SHILL_PROCESS_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/files/file_path.h>
#include <base/lazy_instance.h>
#include <base/memory/weak_ptr.h>
#include <base/tracked_objects.h>
#include <chromeos/minijail/minijail.h>
#include <chromeos/process.h>
#include <chromeos/process_reaper.h>

namespace shill {

class EventDispatcher;

// The ProcessManager is a singleton providing process creation and
// asynchronous process termination. Need to initialize it once with
// Init method call.
class ProcessManager {
 public:
  virtual ~ProcessManager();

  // This is a singleton -- use ProcessManager::GetInstance()->Foo().
  static ProcessManager* GetInstance();

  // Register async signal handler and setup process reaper.
  virtual void Init(EventDispatcher* dispatcher);

  // Create and start a process for |program| with |arguments|. |enivronment|
  // variables will be setup in the child process before exec the |program|.
  // |terminate_with_parent| is used to indicate if child process should
  // self terminate if the parent process exits.  |exit_callback| will be
  // invoked when child process exits (not terminated by us).  Return -1
  // if failed to start the process, otherwise, return the pid of the child
  // process.
  virtual pid_t StartProcess(
      const tracked_objects::Location& from_here,
      const base::FilePath& program,
      const std::vector<std::string>& arguments,
      const std::map<std::string, std::string>& environment,
      bool terminate_with_parent,
      const base::Callback<void(int)>& exit_callback);

  // Same as above, except the spawned process will be started in a minijail.
  virtual pid_t StartProcessInMinijail(
      const tracked_objects::Location& from_here,
      const base::FilePath& program,
      const std::vector<std::string>& arguments,
      const std::string& user,
      const std::string& group,
      uint64_t capmask,
      const base::Callback<void(int)>& exit_callback);

  // Stop the given |pid|.  Previously registered |exit_callback| will be
  // unregistered, since the caller is not interested in this process anymore
  // and that callback might not be valid by the time this process terminates.
  // This will attempt to terminate the child process by sending a SIGTERM
  // signal first.  If the process doesn't terminate within a certain time,
  // ProcessManager will attempt to send a SIGKILL signal.  It will give up
  // with an error log If the process still doesn't terminate within a certain
  // time.
  virtual bool StopProcess(pid_t pid);

 protected:
  ProcessManager();

 private:
  friend class ProcessManagerTest;
  friend struct base::DefaultLazyInstanceTraits<ProcessManager>;

  using TerminationTimeoutCallback = base::CancelableClosure;

  // Invoked when process |pid| exited.
  void OnProcessExited(pid_t pid, const siginfo_t& info);

  // Invoked when process |pid| did not terminate within a certain timeout.
  // |kill_signal| indicates the signal used for termination. When it is set
  // to true, SIGKILL was used to terminate the process, otherwise, SIGTERM
  // was used.
  void ProcessTerminationTimeoutHandler(pid_t pid, bool kill_signal);

  // Send a termination signal to process |pid|. If |kill_signal| is set to
  // true, SIGKILL is sent, otherwise, SIGTERM is sent.  After signal is sent,
  // |pid| and timeout handler is added to |pending_termination_processes_|
  // list, to make sure process |pid| does exit in timely manner.
  bool TerminateProcess(pid_t pid, bool kill_signal);

  // Used to watch processes.
  chromeos::AsynchronousSignalHandler async_signal_handler_;
  chromeos::ProcessReaper process_reaper_;

  EventDispatcher* dispatcher_;
  chromeos::Minijail* minijail_;

  // Processes to watch for the caller.
  std::map<pid_t, base::Callback<void(int)>> watched_processes_;
  // Processes being terminated by us.  Use a timer to make sure process
  // does exit, log an error if it failed to exit within a specific timeout.
  std::map<pid_t, std::unique_ptr<TerminationTimeoutCallback>>
      pending_termination_processes_;

  base::WeakPtrFactory<ProcessManager> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ProcessManager);
};

}  // namespace shill

#endif  // SHILL_PROCESS_MANAGER_H_
