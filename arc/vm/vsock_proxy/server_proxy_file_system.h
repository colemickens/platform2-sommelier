// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_SERVER_PROXY_FILE_SYSTEM_H_
#define ARC_VM_VSOCK_PROXY_SERVER_PROXY_FILE_SYSTEM_H_

#include <fuse/fuse.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <memory>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/observer_list.h>
#include <base/optional.h>
#include <base/synchronization/lock.h>

#include "arc/vm/vsock_proxy/proxy_file_system.h"
#include "arc/vm/vsock_proxy/proxy_service.h"

namespace base {
class TaskRunner;
class WaitableEvent;
}  // namespace base

namespace arc {

class VSockProxy;

// FUSE implementation to support regular file descriptor passing over VSOCK.
// This is designed to be used only in the host side.
class ServerProxyFileSystem : public ProxyFileSystem {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when initialization is completed.
    virtual void OnInit() {}
  };

  // |mount_path| is the path to the mount point.
  explicit ServerProxyFileSystem(const base::FilePath& mount_path);
  ~ServerProxyFileSystem() override;

  // Adds the |observer| to be notified on events.
  void AddObserver(Observer* observer);

  // Removes the |observer|.
  void RemoveObserver(Observer* observer);

  // Starts the fuse file system in foreground. Returns on fuse termination
  // such as unmount of the file system.
  int Run(std::unique_ptr<ProxyService::ProxyFactory> factory);

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
  void Init(struct fuse_conn_info* conn);

  // ProxyFileSystem overrides.
  // Operations for the returned file descriptor will be directed to the
  // fuse operation implementation declared above.
  base::ScopedFD RegisterHandle(int64_t handle) override;

  // Runs an operation interacting with VSockProxy instance on the dedicated
  // thread. This is a blocking operation, so wait for the |callback|
  // completion.
  void RunWithVSockProxyInSyncForTesting(
      base::OnceCallback<void(VSockProxy* proxy)> callback);

 private:
  // Helper to operate GetAtt(). Called on the |task_runner_|.
  void GetAttrInternal(base::WaitableEvent* event,
                       int64_t handle,
                       int* return_value,
                       off_t* size);

  // Helper to operate Read(). Called on the |task_runner_|.
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

  const base::FilePath mount_path_;

  // During the initialization, temporarily keep the |factory| instance,
  // which will be passed to ProxyService on creation.
  std::unique_ptr<ProxyService::ProxyFactory> factory_;

  // ProxyService serving ServerProxy. Initialized in Init() callback.
  // Should be touched on the initialization thread, or on |task_runner_|.
  std::unique_ptr<ProxyService> proxy_service_;

  // TaskRunner to run a task interract with ServerProxy.
  // Initialized with |proxy_service_|.
  scoped_refptr<base::TaskRunner> task_runner_;

  // Registered |handle|s to its opened/not-yet-opened state.
  // The access to |handle_map_| needs to be guarded by |handle_map_lock_|,
  // because fuse starts as many threads as needed so this can be accessed
  // from multiple threads.
  std::map<int64_t, State> handle_map_;
  base::Lock handle_map_lock_;

  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ServerProxyFileSystem);
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_SERVER_PROXY_FILE_SYSTEM_H_
