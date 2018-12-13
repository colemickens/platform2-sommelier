// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>
#include <base/files/file_descriptor_watcher_posix.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <brillo/syslog_logging.h>

#include "arc/vm/vsock_proxy/server_proxy.h"

int main() {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader |
                  brillo::kLogToStderrIfTty);

  base::MessageLoopForIO message_loop;
  base::FileDescriptorWatcher watcher(&message_loop);

  arc::ServerProxy proxy;
  message_loop.task_runner()->PostTask(
      FROM_HERE,
      base::Bind([](arc::ServerProxy* proxy) { proxy->Initialize(); }, &proxy));
  base::RunLoop().Run();
  return 0;
}
