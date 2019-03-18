// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/proxy_service.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/thread.h>

#include "arc/vm/vsock_proxy/proxy_base.h"

namespace arc {

ProxyService::ProxyService(std::unique_ptr<ProxyFactory> factory)
    : factory_(std::move(factory)) {}

ProxyService::~ProxyService() {
  // This is safe, although base::Unretained(this) is used in the method,
  // because stop() is blocked until the thread where the pointer is posted
  // is joined.
  Stop();
}

bool ProxyService::Start() {
  LOG(INFO) << "Starting ProxyService...";
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  auto thread = std::make_unique<base::Thread>("ProxyService");
  if (!thread->StartWithOptions(options)) {
    LOG(ERROR) << "Failed to start a ProxyService thread.";
    return false;
  }
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyService::Initialize, base::Unretained(this),
                     base::Unretained(&event)));
  event.Wait();
  thread_ = std::move(thread);
  LOG(INFO) << "ProxyService thread is ready";
  return true;
}

void ProxyService::Stop() {
  LOG(INFO) << "Stopping ProxyService...";
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyService::ShutDown, base::Unretained(this)));
  thread_.reset();
  LOG(INFO) << "ProxyService has been stopped.";
}

void ProxyService::Initialize(base::WaitableEvent* event) {
  proxy_ = factory_->Create();
  proxy_->Initialize();
  event->Signal();
}

void ProxyService::ShutDown() {
  proxy_.reset();
}

}  // namespace arc
