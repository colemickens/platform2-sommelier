// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/key_generator.h"

#include <sys/types.h>
#include <unistd.h>

#include <base/file_path.h>
#include <base/file_util.h>
#include <chromeos/cryptohome.h>

#include "login_manager/fake_generator_job.h"
#include "login_manager/generator_job.h"
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
    keygen_job_.reset(new GeneratorJob(keygen_argv, uid, utils_));
  }

  keygen_job_->RunInBackground();
  pid_t pid = keygen_job_->CurrentPid();
  if (pid < 0)
    return false;
  DLOG(INFO) << "Generating key at " << temporary_key_filename_
             << " using nssdb under " << user_path.value();

  generating_ = true;
  manager_->AdoptKeyGeneratorJob(keygen_job_.Pass(), pid);
  return true;
}

void KeyGenerator::HandleExit(bool success) {
  manager_->AbandonKeyGeneratorJob();
  if (success) {
    FilePath key_file(temporary_key_filename_);
    manager_->ProcessNewOwnerKey(key_owner_username_, key_file);
  }
  Reset();
}

void KeyGenerator::Reset() {
  key_owner_username_.clear();
  temporary_key_filename_.clear();
  generating_ = false;
}

void KeyGenerator::InjectMockKeygenJob(scoped_ptr<FakeGeneratorJob> keygen) {
  keygen_job_ = scoped_ptr<GeneratorJobInterface>(keygen.Pass());
}

}  // namespace login_manager
