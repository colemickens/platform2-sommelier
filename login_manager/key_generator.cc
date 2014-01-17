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
#include "login_manager/system_utils.h"

namespace login_manager {

using base::FilePath;
using std::string;
using std::vector;

// static
const char KeyGenerator::kTemporaryKeyFilename[] = "key.pub";

KeyGenerator::Delegate::~Delegate() {}

KeyGenerator::KeyGenerator(uid_t uid, SystemUtils *utils)
    : uid_(uid),
      utils_(utils),
      delegate_(NULL),
      factory_(new GeneratorJob::Factory),
      generating_(false) {
}

KeyGenerator::~KeyGenerator() {}

bool KeyGenerator::Start(const string& username) {
  DCHECK(!generating_) << "Must call Reset() between calls to Start()!";
  FilePath user_path(chromeos::cryptohome::home::GetUserPath(username));
  FilePath temporary_key_path(user_path.AppendASCII(kTemporaryKeyFilename));
  if (!file_util::Delete(temporary_key_path, false)) {
    PLOG(ERROR) << "Old keygen state still present; can't generate keys: ";
    return false;
  }
  key_owner_username_ = username;
  temporary_key_filename_ = temporary_key_path.value();
  keygen_job_ = factory_->Create(temporary_key_filename_, user_path,
                                 uid_, utils_);
  if (!keygen_job_->RunInBackground())
    return false;
  pid_t pid = keygen_job_->CurrentPid();
  if (pid < 0)
    return false;
  DLOG(INFO) << "Generating key at " << temporary_key_filename_
             << " using nssdb under " << user_path.value();

  generating_ = true;
  return true;
}

bool KeyGenerator::IsManagedJob(pid_t pid) {
  return (keygen_job_ &&
          keygen_job_->CurrentPid() > 0 &&
          keygen_job_->CurrentPid() == pid);
}

void KeyGenerator::HandleExit(const siginfo_t& info) {
  CHECK(delegate_) << "Must set a delegate before exit can be handled.";
  if (info.si_status == 0) {
    FilePath key_file(temporary_key_filename_);
    delegate_->OnKeyGenerated(key_owner_username_, key_file);
  } else {
    DLOG(WARNING) << "Key generation failed with " << info.si_status;
  }
  Reset();
}

void KeyGenerator::RequestJobExit() {
  if (keygen_job_ && keygen_job_->CurrentPid() > 0)
    keygen_job_->Kill(SIGTERM, "");
}

void KeyGenerator::EnsureJobExit(base::TimeDelta timeout) {
  if (keygen_job_ && keygen_job_->CurrentPid() > 0)
    keygen_job_->WaitAndAbort(timeout);
}

void KeyGenerator::InjectJobFactory(
    scoped_ptr<GeneratorJobFactoryInterface> factory) {
  factory_ = factory.Pass();
}

void KeyGenerator::Reset() {
  key_owner_username_.clear();
  temporary_key_filename_.clear();
  generating_ = false;
}

}  // namespace login_manager
