// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/key_generator.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <base/file_path.h>
#include <base/file_util.h>
#include <chromeos/cryptohome.h>

#include "login_manager/child_job.h"
#include "login_manager/process_manager_service_interface.h"
#include "login_manager/system_utils.h"

namespace login_manager {

using base::FilePath;
using std::string;
using std::vector;

// static
const char KeyGenerator::kKeygenExecutable[] = "/sbin/keygen";
// static
const char KeyGenerator::kTemporaryKeyFilename[] = "key.pub";

KeyGenerator::KeyGenerator(SystemUtils *utils,
                           ProcessManagerServiceInterface* manager)
    : utils_(utils),
      manager_(manager),
      generating_(false) {
}

KeyGenerator::~KeyGenerator() {}

bool KeyGenerator::Start(const string& username, uid_t uid) {
  DCHECK(!generating_) << "Must call Reset() between calls to Start()!";
  FilePath user_path(chromeos::cryptohome::home::GetUserPath(username));
  FilePath temporary_key_path(user_path.AppendASCII(kTemporaryKeyFilename));
  if (!file_util::Delete(temporary_key_path, false)) {
    PLOG(ERROR) << "Old keygen state still present; can't generate keys: ";
    return false;
  }
  key_owner_username_ = username;
  temporary_key_filename_ = temporary_key_path.value();
  if (!keygen_job_.get()) {
    vector<string> keygen_argv;
    keygen_argv.push_back(kKeygenExecutable);
    keygen_argv.push_back(temporary_key_filename_);
    keygen_argv.push_back(user_path.value());
    keygen_job_.reset(new ChildJob(keygen_argv, false, utils_));
  }

  if (uid != 0)
    keygen_job_->SetDesiredUid(uid);
  pid_t pid = RunJob(keygen_job_.get());
  if (pid < 0)
    return false;
  DLOG(INFO) << "Generating key at " << temporary_key_filename_
             << " using nssdb under " << user_path.value();

  generating_ = true;
  guint watcher = g_child_watch_add_full(
      G_PRIORITY_HIGH_IDLE,
      pid,
      KeyGenerator::HandleKeygenExit,
      this,
      NULL);
  manager_->AdoptKeyGeneratorJob(keygen_job_.Pass(), pid, watcher);
  return true;
}

// static
void KeyGenerator::HandleKeygenExit(GPid pid,
                                    gint status,
                                    gpointer data) {
  KeyGenerator* generator = static_cast<KeyGenerator*>(data);
  generator->manager_->AbandonKeyGeneratorJob();

  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    FilePath key_file(generator->temporary_key_filename_);
    generator->manager_->ProcessNewOwnerKey(generator->key_owner_username_,
                                            key_file);
  } else {
    if (WIFSIGNALED(status))
      LOG(ERROR) << "keygen exited on signal " << WTERMSIG(status);
    else
      LOG(ERROR) << "keygen exited with exit code " << WEXITSTATUS(status);
  }
  generator->Reset();
}

void KeyGenerator::Reset() {
  key_owner_username_.clear();
  temporary_key_filename_.clear();
  generating_ = false;
}

int KeyGenerator::RunJob(ChildJobInterface* job) {
  pid_t pid = utils_->fork();
  if (pid == 0) {
    job->Run();
    exit(1);  // Run() is not supposed to return.
  }
  return pid;
}

void KeyGenerator::InjectMockKeygenJob(MockChildJob* keygen) {
  keygen_job_.reset(reinterpret_cast<ChildJobInterface*>(keygen));
}

}  // namespace login_manager
