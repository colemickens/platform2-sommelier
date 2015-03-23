// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_daemon.h"

#include <sysexits.h>

#include <base/message_loop/message_loop.h>

namespace protobinder {

BinderDaemon::BinderDaemon(const std::string& service_name,
                           scoped_ptr<IBinder> binder)
    : manager_(BinderManagerInterface::Get()),
      service_name_(service_name),
      binder_(binder.Pass()) {}

BinderDaemon::~BinderDaemon() {}

int BinderDaemon::OnInit() {
  int return_code = Daemon::OnInit();
  if (return_code != EX_OK)
    return return_code;

  int ret =
      GetServiceManager()->AddService(service_name_.c_str(), binder_.get());
  VLOG(1) << "GetServiceManager()->AddService() returned " << ret;

  int binder_fd = 0;
  manager_->GetFdForPolling(&binder_fd);

  base::MessageLoopForIO::Mode mode = base::MessageLoopForIO::WATCH_READ;
  bool success = base::MessageLoopForIO::current()->WatchFileDescriptor(
      binder_fd, true /* persistent */, mode, &file_descriptor_watcher_, this);
  CHECK(success) << "Unable to watch binder file descriptor";

  return return_code;
}

void BinderDaemon::OnFileCanReadWithoutBlocking(int file_descriptor) {
  VLOG(1) << "FileCanReadWithoutBlocking";
  manager_->HandleEvent();
}

void BinderDaemon::OnFileCanWriteWithoutBlocking(int file_descriptor) {
  NOTREACHED() << "Not watching write events";
}

}  // namespace protobinder
