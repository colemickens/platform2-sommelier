// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_DAEMON_H_
#define LIBPROTOBINDER_BINDER_DAEMON_H_

#include <string>

#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/daemons/daemon.h>

#include "libprotobinder/binder_export.h"
#include "libprotobinder/binder_manager.h"
#include "libprotobinder/iservice_manager.h"

namespace protobinder {

class BinderHost;

class BINDER_EXPORT BinderDaemon : public chromeos::Daemon,
                                   public base::MessageLoopForIO::Watcher {
 public:
  BinderDaemon(const std::string& service_name, scoped_ptr<IBinder> binder);
  virtual ~BinderDaemon();

 private:
  BinderManagerInterface* manager_;
  std::string service_name_;
  scoped_ptr<IBinder> binder_;

  // Implement chromeos::Daemon.
  int OnInit() override;

  // Implement MessageLoopForIO::Watcher.
  void OnFileCanReadWithoutBlocking(int file_descriptor) override;
  void OnFileCanWriteWithoutBlocking(int file_descriptor) override;

  base::MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;

  DISALLOW_COPY_AND_ASSIGN(BinderDaemon);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_DAEMON_H_
