//
// Copyright (C) 2013 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/external_task.h"

#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>

#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/process_manager.h"

namespace shill {

using base::FilePath;
using std::map;
using std::string;
using std::vector;

ExternalTask::ExternalTask(
    ControlInterface* control,
    ProcessManager* process_manager,
    const base::WeakPtr<RPCTaskDelegate>& task_delegate,
    const base::Callback<void(pid_t, int)>& death_callback)
    : control_(control),
      process_manager_(process_manager),
      task_delegate_(task_delegate),
      death_callback_(death_callback),
      pid_(0) {
  CHECK(task_delegate_);
}

ExternalTask::~ExternalTask() {
  ExternalTask::Stop();
}

void ExternalTask::DestroyLater(EventDispatcher* dispatcher) {
  // Passes ownership of |this| to Destroy.
  dispatcher->PostTask(FROM_HERE, base::Bind(&Destroy, this));
}

bool ExternalTask::Start(const FilePath& program,
                         const vector<string>& arguments,
                         const map<string, string>& environment,
                         bool terminate_with_parent,
                         Error* error) {
  CHECK(!pid_);
  CHECK(!rpc_task_);

  // Setup full environment variables.
  auto local_rpc_task = std::make_unique<RPCTask>(control_, this);
  map<string, string> env = local_rpc_task->GetEnvironment();
  env.insert(environment.begin(), environment.end());

  // TODO(mortonm): Take this block out once we start spawning l2tpipsec_vpn
  // using ExternalTask::StartInMinijail
  map<string, string>::iterator task_service_variable =
      env.find(kRPCTaskServiceVariable);
  map<string, string>::iterator task_path_variable =
      env.find(kRPCTaskPathVariable);
  // Fails without the necessary environment variables.
  if (!base::ContainsKey(env, kRPCTaskServiceVariable) ||
      !base::ContainsKey(env, kRPCTaskPathVariable)) {
    Error::PopulateAndLog(
        FROM_HERE, error, Error::kInternalError,
        "Invalid environment variables for: " + program.value());
    return false;
  }
  // Copy input command line arguments to a new vector and add RPC identifiers.
  vector<string> args_with_rpc(arguments);
  args_with_rpc.push_back("--shill_task_service=" +
                          task_service_variable->second);
  args_with_rpc.push_back("--shill_task_path=" + task_path_variable->second);

  pid_t pid = process_manager_->StartProcess(
      FROM_HERE, program, args_with_rpc, env, terminate_with_parent,
      base::Bind(&ExternalTask::OnTaskDied, base::Unretained(this)));

  if (pid < 0) {
    Error::PopulateAndLog(FROM_HERE, error, Error::kInternalError,
                          string("Unable to spawn: ") +
                          program.value().c_str());
    return false;
  }
  pid_ = pid;
  rpc_task_ = std::move(local_rpc_task);
  return true;
}

bool ExternalTask::StartInMinijail(const FilePath& program,
                                   vector<string>& arguments,
                                   const string user,
                                   const string group,
                                   uint64_t mask,
                                   Error* error) {
  // Checks will fail if Start or StartInMinijailWithRPCIdentifiers has already
  // been called on this object.
  CHECK(!pid_);
  CHECK(!rpc_task_);

  // Passes the connection identifiers on the command line instead of through
  // environment variables.
  auto local_rpc_task = std::make_unique<RPCTask>(control_, this);
  map<string, string> env = local_rpc_task->GetEnvironment();
  map<string, string>::iterator task_service_variable =
                                env.find(kRPCTaskServiceVariable);
  map<string, string>::iterator task_path_variable =
                                env.find(kRPCTaskPathVariable);
  // Fails without the necessary environment variables.
  if (task_service_variable == env.end() || task_path_variable == env.end()) {
    Error::PopulateAndLog(FROM_HERE, error, Error::kInternalError,
                          string("Invalid environment variables for: ") +
                          program.value().c_str());
    return false;
  }
  arguments.push_back(base::StringPrintf("--shill_task_service=%s",
                      task_service_variable->second.c_str()));
  arguments.push_back(base::StringPrintf("--shill_task_path=%s",
                      task_path_variable->second.c_str()));

  pid_t pid =
      process_manager_->StartProcessInMinijail(FROM_HERE,
                                     program,
                                     arguments,
                                     user,
                                     group,
                                     mask,
                                     false, /* inherit_supplementary_groups */
                                     base::Bind(&ExternalTask::OnTaskDied,
                                                base::Unretained(this)));

  if (pid < 0) {
    Error::PopulateAndLog(FROM_HERE, error, Error::kInternalError,
                          string("Unable to spawn: ") +
                          program.value().c_str() + string(" in a minijail."));
    return false;
  }
  pid_ = pid;
  rpc_task_ = std::move(local_rpc_task);
  return true;
}

void ExternalTask::Stop() {
  if (pid_) {
    process_manager_->StopProcess(pid_);
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

void ExternalTask::OnTaskDied(int exit_status) {
  CHECK(pid_);
  LOG(INFO) << __func__ << "(" << pid_ << ", "
            << exit_status << ")";
  death_callback_.Run(pid_, exit_status);
  pid_ = 0;
  rpc_task_.reset();
}

// static
void ExternalTask::Destroy(ExternalTask* task) {
  delete task;
}

}  // namespace shill
