// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/dpsl/internal/dpsl_thread_context_impl.h"

#include <utility>

#include <base/bind.h>
#include <base/lazy_instance.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/threading/thread_local.h>
#include <base/time/time.h>
#include <brillo/bind_lambda.h>

namespace diagnostics {

namespace {

// Whether an instance of DpslThreadContextImpl was created on the current
// thread.
base::LazyInstance<base::ThreadLocalBoolean>::Leaky
    g_thread_context_impl_created = LAZY_INSTANCE_INITIALIZER;

}  // namespace

DpslThreadContextImpl::DpslThreadContextImpl()
    : thread_id_(base::PlatformThread::CurrentId()) {
  // Initialize the message loop only if there's no one yet (it could be already
  // set up by the calling code via other means, e.g., brillo::Daemon).
  if (!base::MessageLoop::current()) {
    owned_message_loop_ = std::make_unique<base::MessageLoopForIO>();
    CHECK(base::MessageLoop::current());
  }
  message_loop_ = base::MessageLoop::current();
}

DpslThreadContextImpl::~DpslThreadContextImpl() {
  CHECK(sequence_checker_.CalledOnValidSequence());
}

bool DpslThreadContextImpl::BelongsToCurrentThread() {
  return base::PlatformThread::CurrentId() == thread_id_;
}

void DpslThreadContextImpl::RunEventLoop() {
  CHECK(sequence_checker_.CalledOnValidSequence());

  CHECK(!current_run_loop_);
  base::RunLoop run_loop;
  current_run_loop_ = &run_loop;

  run_loop.Run();

  current_run_loop_ = nullptr;
}

bool DpslThreadContextImpl::IsEventLoopRunning() {
  CHECK(sequence_checker_.CalledOnValidSequence());
  return current_run_loop_ != nullptr;
}

void DpslThreadContextImpl::PostTask(std::function<void()> task) {
  message_loop_->PostTask(FROM_HERE, base::Bind(std::move(task)));
}

void DpslThreadContextImpl::PostDelayedTask(std::function<void()> task,
                                            int64_t delay_milliseconds) {
  CHECK_GE(delay_milliseconds, 0);
  message_loop_->PostDelayedTask(
      FROM_HERE, base::Bind(std::move(task)),
      base::TimeDelta::FromMilliseconds(delay_milliseconds));
}

void DpslThreadContextImpl::QuitEventLoop() {
  CHECK(sequence_checker_.CalledOnValidSequence());

  if (current_run_loop_)
    current_run_loop_->Quit();
}

// static
std::unique_ptr<DpslThreadContext> DpslThreadContext::Create(
    DpslGlobalContext* global_context) {
  CHECK(global_context);

  // Verify we're not called twice on the current thread.
  CHECK(!g_thread_context_impl_created.Pointer()->Get())
      << "Duplicate DpslThreadContext instances constructed on the same thread";
  g_thread_context_impl_created.Pointer()->Set(true);

  return std::make_unique<DpslThreadContextImpl>();
}

}  // namespace diagnostics
