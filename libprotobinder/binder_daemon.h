// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_DAEMON_H_
#define LIBPROTOBINDER_BINDER_DAEMON_H_

#include <string>

#include <base/logging.h>
#include <base/macros.h>
#include <chromeos/daemons/daemon.h>

#include "binder_export.h"  // NOLINT(build/include)
#include "binder_manager.h"  // NOLINT(build/include)

namespace protobinder {

class BinderHost;

class BINDER_EXPORT BinderDaemon : public chromeos::Daemon,
                                   public base::MessageLoopForIO::Watcher {
 public:
  BinderDaemon();
  virtual ~BinderDaemon();

 protected:
  // Implement chromeos::Daemon.
  int OnInit() override;

 private:
  BinderManagerInterface* manager_;

  // Implement MessageLoopForIO::Watcher.
  void OnFileCanReadWithoutBlocking(int file_descriptor) override;
  void OnFileCanWriteWithoutBlocking(int file_descriptor) override;

  base::MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;

  DISALLOW_COPY_AND_ASSIGN(BinderDaemon);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_DAEMON_H_
