// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_APPFUSE_DATA_FILTER_H_
#define ARC_APPFUSE_DATA_FILTER_H_

#include <stdint.h>

#include <deque>
#include <map>
#include <vector>

#include <base/callback.h>
#include <base/files/scoped_file.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <base/task_runner.h>
#include <base/threading/thread.h>

namespace arc {
namespace appfuse {

// DataFilter verifies input from /dev/fuse and reject unexpected data.
class DataFilter : public base::MessageLoopForIO::Watcher {
 public:
  DataFilter();
  ~DataFilter() override;

  // The given callback will be run when this filter stops.
  void set_on_stopped_callback(const base::Closure& callback) {
    on_stopped_callback_ = callback;
  }

  // Starts watching the given /dev/fuse FD and returns a filtered FD.
  base::ScopedFD Start(base::ScopedFD fd_dev);

  // MessageLoopForIO::Watcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  // Starts watching the file descriptors on the watch thread.
  void StartWatching();

  // Aborts watching the file descriptors.
  void AbortWatching();

  // Returns true if fd == fd_dev_.
  bool IsDevFuseFD(int fd) { return fd == fd_dev_.get(); }

  // Filters data from /dev/fuse and forwards it to the socket.
  bool FilterDataFromDev(std::vector<char>* data);

  // Filters data from the socket and forwards it to /dev/fuse.
  bool FilterDataFromSocket(std::vector<char>* data);

  base::Thread watch_thread_;
  base::ScopedFD fd_dev_;
  base::ScopedFD fd_socket_;
  base::MessageLoopForIO::FileDescriptorWatcher watcher_dev_;
  base::MessageLoopForIO::FileDescriptorWatcher watcher_socket_;

  std::deque<std::vector<char>> pending_data_to_dev_;
  std::deque<std::vector<char>> pending_data_to_socket_;

  std::map<uint64_t, uint32_t> unique_to_opcode_;

  scoped_refptr<base::TaskRunner> origin_task_runner_;
  base::Closure on_stopped_callback_;

  DISALLOW_COPY_AND_ASSIGN(DataFilter);
};

}  // namespace appfuse
}  // namespace arc

#endif  // ARC_APPFUSE_DATA_FILTER_H_
