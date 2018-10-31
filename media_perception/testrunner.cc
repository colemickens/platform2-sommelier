// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/threading/thread_task_runner_handle.h>
#include <brillo/flag_helper.h>
#include <brillo/message_loops/base_message_loop.h>
#include <brillo/test_helpers.h>
#include <mojo/edk/embedder/embedder.h>

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  base::AtExitManager exit_manager;

  (new brillo::BaseMessageLoop())->SetAsCurrent();

  mojo::edk::Init();
  mojo::edk::InitIPCSupport(base::ThreadTaskRunnerHandle::Get());

  return RUN_ALL_TESTS();
}
