// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/key_generator.h"

#include <unistd.h>

#include <base/file_util.h>

#include "login_manager/child_job.h"
#include "login_manager/session_manager_service.h"
#include "login_manager/system_utils.h"

namespace login_manager {

// static
const char KeyGenerator::kKeygenExecutable[] = "/sbin/keygen";
// static
const char KeyGenerator::kTemporaryKeyFilename[] = "key.pub";

KeyGenerator::KeyGenerator(SystemUtils *utils)
    : utils_(utils),
      temporary_key_filename_(file_util::GetHomeDir().AppendASCII(
          kTemporaryKeyFilename).value()) {
}

KeyGenerator::~KeyGenerator() {}

bool KeyGenerator::Start(uid_t uid,
                         SessionManagerService* manager) {
  SystemUtils utils;
  if (!file_util::Delete(FilePath(temporary_key_filename_), false)) {
    PLOG(ERROR) << "Old keygen state still present; can't generate keys: ";
    return false;
  }
  if (!keygen_job_.get()) {
    LOG(INFO) << "Creating keygen job";
    std::vector<std::string> keygen_argv;
    keygen_argv.push_back(kKeygenExecutable);
    keygen_argv.push_back(temporary_key_filename_);
    keygen_job_.reset(new ChildJob(keygen_argv, &utils));
  }

  if (uid != 0)
    keygen_job_->SetDesiredUid(uid);
  int pid = RunJob(keygen_job_.get());
  if (pid < 0)
    return false;

  g_child_watch_add_full(G_PRIORITY_HIGH_IDLE,
                         pid,
                         SessionManagerService::HandleKeygenExit,
                         manager,
                         NULL);
  manager->AdoptChild(keygen_job_.release(), pid);
  return true;
}

int KeyGenerator::RunJob(ChildJobInterface* job) {
  int pid = utils_->fork();
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
