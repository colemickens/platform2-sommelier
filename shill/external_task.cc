// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/external_task.h"

#include <signal.h>
#include <sys/prctl.h>

#include <base/bind.h>
#include <base/bind_helpers.h>

#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/process_killer.h"

namespace shill {

using base::FilePath;
using std::map;
using std::string;
using std::vector;

ExternalTask::ExternalTask(
    ControlInterface* control,
    GLib* glib,
    const base::WeakPtr<RPCTaskDelegate>& task_delegate,
    const base::Callback<void(pid_t, int)>& death_callback)
    : control_(control),
      glib_(glib),
      process_killer_(ProcessKiller::GetInstance()),
      task_delegate_(task_delegate),
      death_callback_(death_callback),
      pid_(0),
      child_watch_tag_(0) {
  CHECK(task_delegate_);
}

ExternalTask::~ExternalTask() {
  ExternalTask::Stop();
}

void ExternalTask::DestroyLater(EventDispatcher* dispatcher) {
  // Passes ownership of |this| to Destroy.
  dispatcher->PostTask(base::Bind(&Destroy, this));
}

bool ExternalTask::Start(const FilePath& program,
                         const vector<string>& arguments,
                         const map<string, string>& environment,
                         bool terminate_with_parent,
                         Error* error) {
  CHECK(!pid_);
  CHECK(!child_watch_tag_);
  CHECK(!rpc_task_);

  std::unique_ptr<RPCTask> local_rpc_task(new RPCTask(control_, this));

  // const_cast is safe here, because exec*() (and SpawnAsync) do not
  // modify the strings passed to them. This isn't captured in the
  // exec*() prototypes, due to limitations in ISO C.
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/exec.html
  vector<char*> process_args;
  process_args.push_back(const_cast<char*>(program.value().c_str()));
  for (const auto& option : arguments) {
    process_args.push_back(const_cast<char*>(option.c_str()));
  }
  process_args.push_back(nullptr);

  vector<char*> process_env;
  vector<string> env_vars(local_rpc_task->GetEnvironment());
  for (const auto& env_pair : environment) {
    env_vars.push_back(string(env_pair.first + "=" + env_pair.second));
  }
  for (const auto& env_var : env_vars) {
    // See above regarding const_cast.
    process_env.push_back(const_cast<char*>(env_var.c_str()));
  }
  process_env.push_back(nullptr);

  GSpawnChildSetupFunc child_setup_func =
      terminate_with_parent ? SetupTermination : nullptr;

  if (!glib_->SpawnAsync(nullptr,
                         process_args.data(),
                         process_env.data(),
                         G_SPAWN_DO_NOT_REAP_CHILD,
                         child_setup_func,
                         nullptr,
                         &pid_,
                         nullptr)) {
    Error::PopulateAndLog(FROM_HERE, error, Error::kInternalError,
                          string("Unable to spawn: ") + process_args[0]);
    return false;
  }
  child_watch_tag_ = glib_->ChildWatchAdd(pid_, OnTaskDied, this);
  rpc_task_.reset(local_rpc_task.release());
  return true;
}

void ExternalTask::Stop() {
  if (child_watch_tag_) {
    glib_->SourceRemove(child_watch_tag_);
    child_watch_tag_ = 0;
  }
  if (pid_) {
    process_killer_->Kill(pid_, base::Closure());
    pid_ = 0;
  }
  rpc_task_.reset();
}

void ExternalTask::GetLogin(string* user, string* password) {
  return task_delegate_->GetLogin(user, password);
}

void ExternalTask::Notify(const string& event,
                          const map<string, string>& details) {
  return task_delegate_->Notify(event, details);
}

// static
void ExternalTask::OnTaskDied(GPid pid, gint status, gpointer data) {
  LOG(INFO) << __func__ << "(" << pid << ", "  << status << ")";
  ExternalTask* me = reinterpret_cast<ExternalTask*>(data);
  me->child_watch_tag_ = 0;
  CHECK_EQ(pid, me->pid_);
  me->pid_ = 0;
  me->death_callback_.Run(pid, status);
}

// static
void ExternalTask::Destroy(ExternalTask* task) {
  delete task;
}

// static
void ExternalTask::SetupTermination(gpointer /*glib_user_data*/) {
  prctl(PR_SET_PDEATHSIG, SIGTERM);
}

}  // namespace shill
