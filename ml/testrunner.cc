// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/command_line.h>
#include <base/threading/thread_task_runner_handle.h>
#include <brillo/flag_helper.h>
#include <brillo/message_loops/base_message_loop.h>
#include <brillo/test_helpers.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/edk/embedder/process_delegate.h>

// A ProcessDelegate that does nothing upon IPC system shutdown.
class DoNothingProcessDelegate : public mojo::edk::ProcessDelegate {
 public:
  void OnShutdownComplete() override {}
};

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);

  (new brillo::BaseMessageLoop())->SetAsCurrent();

  mojo::edk::Init();
  mojo::edk::InitIPCSupport(new DoNothingProcessDelegate(),
                            base::ThreadTaskRunnerHandle::Get());

  return RUN_ALL_TESTS();
}
