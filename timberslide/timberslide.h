// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIMBERSLIDE_TIMBERSLIDE_H_
#define TIMBERSLIDE_TIMBERSLIDE_H_

#include <string>

#include <base/files/file_util.h>
#include <brillo/daemons/daemon.h>

namespace timberslide {

class TimberSlide : public brillo::Daemon,
                    public base::MessageLoopForIO::Watcher {
 public:
  TimberSlide(const std::string& ec_type,
              base::File device_file,
              base::File uptime_file,
              const base::FilePath& log_dir);

 private:
  int OnInit() override;

  void OnFileCanWriteWithoutBlocking(int fd) override;
  void OnFileCanReadWithoutBlocking(int fd) override;

  bool GetEcUptime(int64_t* ec_uptime_ms);

  void RotateLogs(const base::FilePath& previous_log,
                  const base::FilePath& current_log);

  base::File device_file_;
  base::FilePath current_log_;
  base::FilePath previous_log_;
  base::MessageLoopForIO::FileDescriptorWatcher fd_watcher_;
  int total_size_ = 0;
  base::File uptime_file_;
  bool uptime_file_valid_ = false;
};

}  // namespace timberslide

#endif  // TIMBERSLIDE_TIMBERSLIDE_H_
