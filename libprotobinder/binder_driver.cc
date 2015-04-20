// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_driver.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
// Out of order due to this bad header requiring sys/types.h
#include <linux/android/binder.h>

#include <base/logging.h>

namespace protobinder {

namespace {
const size_t kBinderMappedSize = (1 * 1024 * 1024) - (4096 * 2);
}

BinderDriver::BinderDriver() {
}

BinderDriver::~BinderDriver() {
}

void BinderDriver::Init() {
  binder_fd_ = open("/dev/binder", O_RDWR | O_CLOEXEC);
  if (binder_fd_ < 0) {
    PLOG(FATAL) << "Failed to open binder";
  }

  // Check the version.
  struct binder_version vers;
  if ((ioctl(binder_fd_, BINDER_VERSION, &vers) < 0) ||
      (vers.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION)) {
    LOG(FATAL) << "Binder driver mismatch";
  }

  // mmap the user buffer.
  binder_mapped_address_ = mmap(NULL, kBinderMappedSize, PROT_READ,
                                MAP_PRIVATE | MAP_NORESERVE, binder_fd_, 0);
  if (binder_mapped_address_ == MAP_FAILED) {
    LOG(FATAL) << "Failed to mmap binder";
  }
}

int BinderDriver::GetFdForPolling() {
  return binder_fd_;
}

int BinderDriver::ReadWrite(struct binder_write_read* buffers) {
  return ioctl(binder_fd_, BINDER_WRITE_READ, buffers);
}

void BinderDriver::SetMaxThreads(int max_threads) {
  if (ioctl(binder_fd_, BINDER_SET_MAX_THREADS, &max_threads) < 0)
    PLOG(FATAL) << "ioctl(binder_fd, BINDER_SET_MAX_THREADS) failed";
}

}  // namespace protobinder
