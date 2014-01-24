// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/fake_generator_job.h"

#include <string>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/memory/scoped_ptr.h>

#include "login_manager/generator_job.h"

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
  return file_util::WriteFile(FilePath(filename_),
                              key_contents_.c_str(),
                              key_contents_.size());
}

}  // namespace login_manager
