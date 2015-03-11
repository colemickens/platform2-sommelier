// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_daemon.h"

namespace protobinder {

void BinderDaemon::OnFileCanReadWithoutBlocking(int file_descriptor) {
  LOG(INFO) << "FileCanReadWithoutBlocking";
  manager_->HandleEvent();
}

void BinderDaemon::OnFileCanWriteWithoutBlocking(int file_descriptor) {
  NOTREACHED() << "Not watching write events";
}

int BinderDaemon::Init(IBinder* binder) {
  binder_.reset(binder);
  return 0;
}

int BinderDaemon::Run() {
  int ret =
      GetServiceManager()->AddService(service_name_.c_str(), binder_.get());
  LOG(INFO) << "service ret " << ret;

  int binder_fd = 0;
  manager_->GetFdForPolling(&binder_fd);

  base::MessageLoopForIO::Mode mode = base::MessageLoopForIO::WATCH_READ;
  const bool success = base::MessageLoopForIO::current()->WatchFileDescriptor(
      binder_fd, true /* persistent */, mode, &file_descriptor_watcher_, this);
  CHECK(success) << "Unable to watch file descriptor";

  base::RunLoop run_loop;
  run_loop.Run();
  return 0;
}

}  // namespace protobinder
