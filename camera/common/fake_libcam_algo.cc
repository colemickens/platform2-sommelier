// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <unordered_map>

#include <base/bind.h>
#include <base/logging.h>
#include <base/threading/thread.h>

#include "common/libcab_test_internal.h"
#include "cros-camera/camera_algorithm.h"
#include "cros-camera/future.h"

namespace libcab_test {

class CameraAlgorithmImpl {
 public:
  static CameraAlgorithmImpl* GetInstance() {
    static CameraAlgorithmImpl impl;
    return &impl;
  }

  int32_t Initialize(const camera_algorithm_callback_ops_t* callback_ops) {
    if (!callback_ops) {
      return -EINVAL;
    }
    callback_ops_ = callback_ops;
    return 0;
  }

  int32_t RegisterBuffer(int buffer_fd) {
    if (handles_.find(buffer_fd) != handles_.end()) {
      LOGF(ERROR) << "Buffer already registered";
      return -EINVAL;
    }
    struct stat sb;
    if (fstat(buffer_fd, &sb) == -1) {
      LOGF(ERROR) << "Failed to get buffer status";
      return -EBADFD;
    }
    void* addr = mmap(0, sb.st_size, PROT_WRITE, MAP_SHARED, buffer_fd, 0);
    if (!addr) {
      LOGF(ERROR) << "Failed to map buffer";
      return -EBADFD;
    }
    int32_t handle = -1;
    static unsigned int seed = time(NULL) + getpid();
    do {
      handle = rand_r(&seed);
    } while (shm_info_map_.find(handle) != shm_info_map_.end());
    handles_[buffer_fd] = handle;
    shm_info_map_[handle].fd = buffer_fd;
    shm_info_map_[handle].addr = addr;
    shm_info_map_[handle].size = sb.st_size;
    return handle;
  }

  void Request(const uint8_t req_header[],
               uint32_t size,
               int32_t buffer_handle) {
    uint32_t status = 0;
    switch (req_header[0]) {
      case REQUEST_TEST_COMMAND_NORMAL:
        if (shm_info_map_.find(buffer_handle) == shm_info_map_.end()) {
          LOGF(ERROR) << "Invalid buffer handle";
          status = -EBADF;
        }
        break;
      case REQUEST_TEST_COMMAND_VERIFY_STATUS:
        status = SimpleHash(req_header, size);
        break;
      default:
        status = -EINVAL;
    }
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&CameraAlgorithmImpl::ReturnCallback,
                              base::Unretained(this), status, buffer_handle));
  }

  void DeregisterBuffers(const int32_t buffer_handles[], uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
      if (shm_info_map_.find(buffer_handles[i]) == shm_info_map_.end()) {
        LOGF(ERROR) << "Invalid buffer handle (" << buffer_handles[i] << ")";
        continue;
      }
      handles_.erase(shm_info_map_[buffer_handles[i]].fd);
      munmap(shm_info_map_[buffer_handles[i]].addr,
             shm_info_map_[buffer_handles[i]].size);
      close(shm_info_map_[buffer_handles[i]].fd);
      shm_info_map_.erase(buffer_handles[i]);
    }
  }

 private:
  CameraAlgorithmImpl()
      : thread_("Camera Algorithm Thread"), callback_ops_(nullptr) {
    thread_.Start();
  }

  void ReturnCallback(uint32_t status, int32_t buffer_handle) {
    (*callback_ops_->return_callback)(callback_ops_, status, buffer_handle);
  }

  base::Thread thread_;

  const camera_algorithm_callback_ops_t* callback_ops_;

  typedef struct {
    int32_t fd;
    void* addr;
    size_t size;
  } ShmInfo;

  // Store shared memory fd and mapped address with handle as the key
  std::unordered_map<int32_t, ShmInfo> shm_info_map_;

  // Store handles with fd as the key
  std::unordered_map<int32_t, int32_t> handles_;
};

static int32_t Initialize(const camera_algorithm_callback_ops_t* callback_ops) {
  return CameraAlgorithmImpl::GetInstance()->Initialize(callback_ops);
}

static int32_t RegisterBuffer(int32_t buffer_fd) {
  return CameraAlgorithmImpl::GetInstance()->RegisterBuffer(buffer_fd);
}

static void Request(const uint8_t req_header[],
                    uint32_t size,
                    int32_t buffer_handle) {
  CameraAlgorithmImpl::GetInstance()->Request(req_header, size, buffer_handle);
}

static void DeregisterBuffers(const int32_t buffer_handles[], uint32_t size) {
  CameraAlgorithmImpl::GetInstance()->DeregisterBuffers(buffer_handles, size);
}

}  // namespace libcab_test

extern "C" {
camera_algorithm_ops_t CAMERA_ALGORITHM_MODULE_INFO_SYM
    __attribute__((__visibility__("default"))) = {
        .initialize = libcab_test::Initialize,
        .register_buffer = libcab_test::RegisterBuffer,
        .request = libcab_test::Request,
        .deregister_buffers = libcab_test::DeregisterBuffers};
}
