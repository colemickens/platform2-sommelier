// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_PROXY_FILE_SYSTEM_H_
#define ARC_VM_VSOCK_PROXY_PROXY_FILE_SYSTEM_H_

#include <fuse/fuse.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <memory>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/optional.h>
#include <base/synchronization/lock.h>

#include "arc/vm/vsock_proxy/vsock_proxy.h"

namespace base {
class TaskRunner;
class WaitableEvent;
}  // namespace base

namespace arc {

class FuseMount;

// FUSE implementation to support regular file descriptor passing over VSOCK.
// This is designed to be used only in the host side.
class ProxyFileSystem {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    using PreadCallback = VSockProxy::PreadCallback;
    using FstatCallback = VSockProxy::FstatCallback;

    // Implement these methods to handle file operation requests.
    virtual void Pread(int64_t handle,
                       uint64_t count,
                       uint64_t offset,
                       PreadCallback callback) = 0;
    virtual void Close(int64_t handle) = 0;
    virtual void Fstat(int64_t handle, FstatCallback callback) = 0;
  };
  // |mount_path| is the path to the mount point.
  ProxyFileSystem(Delegate* delegate,
                  scoped_refptr<base::TaskRunner> delegate_task_runner,
                  const base::FilePath& mount_path);
  ~ProxyFileSystem();

  ProxyFileSystem(const ProxyFileSystem&) = delete;
  ProxyFileSystem& operator=(const ProxyFileSystem&) = delete;

  // Initializes this object.
  bool Init();

  // Implementation of the fuse operation callbacks.
  int GetAttr(const char* path, struct stat* stat);
  int Open(const char* path, struct fuse_file_info* fi);
  int Read(const char* path,
           char* buf,
           size_t size,
           off_t off,
           struct fuse_file_info* fi);
  int Release(const char* path, struct fuse_file_info* fi);
  int ReadDir(const char* path,
              void* buf,
              fuse_fill_dir_t filler,
              off_t offset,
              struct fuse_file_info* fi);

  // Registers the given |handle| to the file system, then returns the file
  // descriptor corresponding to the registered file.
  // Operations for the returned file descriptor will be directed to the
  // fuse operation implementation declared above.
  base::ScopedFD RegisterHandle(int64_t handle);

 private:
  // Helper to operate GetAtt(). Called on the |delegate_task_runner_|.
  void GetAttrInternal(base::WaitableEvent* event,
                       int64_t handle,
                       int* return_value,
                       off_t* size);

  // Helper to operate Read(). Called on the |delegate_task_runner_|.
  void ReadInternal(base::WaitableEvent* event,
                    int64_t handle,
                    char* buf,
                    size_t size,
                    off_t offset,
                    int* return_value);

  // Returns the opened/not-opened-yet state of the given |handle|.
  // If not registered, base::nullopt is returned.
  enum class State {
    NOT_OPENED,
    OPENED,
  };
  base::Optional<State> GetState(int64_t handle);

  Delegate* const delegate_;
  scoped_refptr<base::TaskRunner> delegate_task_runner_;
  const base::FilePath mount_path_;

  std::unique_ptr<FuseMount> fuse_mount_;

  // Registered |handle|s to its opened/not-yet-opened state.
  // The access to |handle_map_| needs to be guarded by |handle_map_lock_|,
  // because fuse starts as many threads as needed so this can be accessed
  // from multiple threads.
  std::map<int64_t, State> handle_map_;
  base::Lock handle_map_lock_;

  scoped_refptr<base::TaskRunner> init_task_runner_;
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_PROXY_FILE_SYSTEM_H_
