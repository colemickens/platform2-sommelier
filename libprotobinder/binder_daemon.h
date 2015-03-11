// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_DAEMON_H_
#define LIBPROTOBINDER_BINDER_DAEMON_H_

#include <string>

#include <base/at_exit.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>

#include "libprotobinder/binder_export.h"
#include "libprotobinder/binder_manager.h"
#include "libprotobinder/iservice_manager.h"

namespace protobinder {

class BinderHost;

class BINDER_EXPORT BinderDaemon : base::MessageLoopForIO::Watcher {
 public:
  explicit BinderDaemon(const std::string& service_name)
      : manager_(BinderManager::GetBinderManager()),
        service_name_(service_name) {}
  virtual ~BinderDaemon() {}

  int Init(IBinder* binder);
  int Run();

 private:
  BinderManager* manager_;
  std::string service_name_;
  scoped_ptr<IBinder> binder_;

  // Implement MessageLoopForIO::Watcher.
  void OnFileCanReadWithoutBlocking(int file_descriptor) override;

  // Implement MessageLoopForIO::Watcher.
  void OnFileCanWriteWithoutBlocking(int file_descriptor) override;

  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  base::MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;

  DISALLOW_COPY_AND_ASSIGN(BinderDaemon);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_DAEMON_H_
