// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/fake_generator_job.h"

#include <sys/types.h>

#include <string>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/memory/scoped_ptr.h>
#include <base/time.h>

#include "login_manager/generator_job.h"
#include "login_manager/key_generator.h"

namespace login_manager {

FakeGeneratorJob::Factory::Factory(pid_t pid,
                                   const std::string& name,
                                   const std::string& key_contents)
    : pid_(pid),
      name_(name),
      key_contents_(key_contents) {
}
FakeGeneratorJob::Factory::~Factory() {}

scoped_ptr<GeneratorJobInterface> FakeGeneratorJob::Factory::Create(
    const std::string& filename,
    const base::FilePath& user_path,
    uid_t desired_uid,
    SystemUtils* utils) {
  return scoped_ptr<GeneratorJobInterface>(new FakeGeneratorJob(pid_,
                                                                name_,
                                                                key_contents_,
                                                                filename));
}

FakeGeneratorJob::FakeGeneratorJob(pid_t pid,
                                   const std::string& name,
                                   const std::string& key_contents,
                                   const std::string& filename)
    : pid_(pid),
      name_(name),
      key_contents_(key_contents),
      filename_(filename) {
}
FakeGeneratorJob::~FakeGeneratorJob() {}

bool FakeGeneratorJob::RunInBackground() {
  base::FilePath full_path(filename_);
  if (!file_util::CreateDirectory(full_path.DirName())) {
    PLOG(ERROR) << "Could not create directory " << full_path.DirName().value();
    return false;
  }
  size_t bytes_written = file_util::WriteFile(full_path,
                                              key_contents_.c_str(),
                                              key_contents_.size());
  if (bytes_written == key_contents_.size())
    return true;
  PLOG(ERROR) << "Could not write " << filename_;
  return false;
}

}  // namespace login_manager
