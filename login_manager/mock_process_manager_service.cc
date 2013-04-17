// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/mock_process_manager_service.h"

#include <base/memory/scoped_ptr.h>
#include <gmock/gmock.h>

#include "login_manager/child_job.h"

namespace login_manager {

MockProcessManagerService::MockProcessManagerService() : generator_pid_(0) {}
MockProcessManagerService::~MockProcessManagerService() {}

void MockProcessManagerService::AdoptKeyGeneratorJob(
    scoped_ptr<ChildJobInterface> job,
    pid_t pid,
    guint watcher) {
  if (generator_pid_ != pid) {
    ADD_FAILURE() << "Incorrect pid offered for adoption: "
                  << "Expected " << generator_pid_ << " got " << pid;
  }
  EXPECT_CALL(*this, AbandonKeyGeneratorJob()).Times(1);
}

void MockProcessManagerService::ExpectAdoptAndAbandon(
    pid_t expected_generator_pid) {
  generator_pid_ = expected_generator_pid;
}

}  // namespace login_manager
