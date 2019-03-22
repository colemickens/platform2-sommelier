// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/server_proxy_file_system.h"

#include <errno.h>

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/optional.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_piece.h>
#include <base/synchronization/waitable_event.h>
#include <base/task_runner.h>

#include "arc/vm/vsock_proxy/proxy_base.h"
#include "arc/vm/vsock_proxy/server_proxy.h"
#include "arc/vm/vsock_proxy/vsock_proxy.h"

namespace arc {
namespace {

constexpr char kFileSystemName[] = "arcvm-serverproxy";

// Returns ServerProxyFileSystem assigned to the FUSE's private_data.
ServerProxyFileSystem* GetFileSystem() {
  return static_cast<ServerProxyFileSystem*>(fuse_get_context()->private_data);
}

int GetAttr(const char* path, struct stat* stat) {
  return GetFileSystem()->GetAttr(path, stat);
}

int Open(const char* path, struct fuse_file_info* fi) {
  return GetFileSystem()->Open(path, fi);
}

int Read(const char* path,
         char* buf,
         size_t size,
         off_t off,
         struct fuse_file_info* fi) {
  return GetFileSystem()->Read(path, buf, size, off, fi);
}

int Release(const char* path, struct fuse_file_info* fi) {
  return GetFileSystem()->Release(path, fi);
}

int ReadDir(const char* path,
            void* buf,
            fuse_fill_dir_t filler,
            off_t offset,
            struct fuse_file_info* fi) {
  return GetFileSystem()->ReadDir(path, buf, filler, offset, fi);
}

void* Init(struct fuse_conn_info* conn) {
  auto* file_system = GetFileSystem();
  file_system->Init(conn);
  // The libfuse overwrites its private_data by using the return value of the
  // init method.
  return file_system;
}

int FuseMain(const base::FilePath& mount_path,
             ServerProxyFileSystem* private_data) {
  const std::string path_str = mount_path.value();
  const char* fuse_argv[] = {
      kFileSystemName, path_str.c_str(),
      "-f",  // "-f" for foreground.
  };

  constexpr struct fuse_operations operations = {
      .getattr = GetAttr,
      .open = Open,
      .read = Read,
      .release = Release,
      .readdir = ReadDir,
      .init = Init,
  };
  return fuse_main(arraysize(fuse_argv), const_cast<char**>(fuse_argv),
                   &operations, private_data);
}

// Parses the given path to a handle. The path should be formatted in
// "/<handle>", where <handle> is int64_t. Returns nullopt on error.
base::Optional<int64_t> ParseHandle(const char* path) {
  if (!path || path[0] != '/')
    return base::nullopt;

  // Parse the path as int64_t excluding leading '/'.
  int64_t value = 0;
  if (!base::StringToInt64(path + 1, &value))
    return base::nullopt;

  return value;
}

}  // namespace

ServerProxyFileSystem::ServerProxyFileSystem(const base::FilePath& mount_path)
    : mount_path_(mount_path) {}

ServerProxyFileSystem::~ServerProxyFileSystem() = default;

void ServerProxyFileSystem::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ServerProxyFileSystem::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

int ServerProxyFileSystem::Run(
    std::unique_ptr<ProxyService::ProxyFactory> factory) {
  factory_ = std::move(factory);
  return FuseMain(mount_path_, this);
}

int ServerProxyFileSystem::GetAttr(const char* path, struct stat* stat) {
  if (path == base::StringPiece("/")) {
    stat->st_mode = S_IFDIR;
    stat->st_nlink = 2;
    return 0;
  }

  auto handle = ParseHandle(path);
  if (!handle.has_value()) {
    LOG(ERROR) << "Invalid path: " << path;
    return -ENOENT;
  }

  auto size = GetFileSize(handle.value());
  if (!size.has_value()) {
    LOG(ERROR) << "Handle not found: " << path;
    return -ENOENT;
  }

  stat->st_mode = S_IFREG;
  stat->st_nlink = 1;
  stat->st_size = size.value();
  return 0;
}

int ServerProxyFileSystem::Open(const char* path, struct fuse_file_info* fi) {
  auto handle = ParseHandle(path);
  if (!handle.has_value()) {
    LOG(ERROR) << "Invalid path: " << path;
    return -ENOENT;
  }

  if (!GetFileSize(handle.value()).has_value()) {
    LOG(ERROR) << "Handle not found: " << path;
    return -ENOENT;
  }

  return 0;
}

int ServerProxyFileSystem::Read(const char* path,
                                char* buf,
                                size_t size,
                                off_t off,
                                struct fuse_file_info* fi) {
  auto handle = ParseHandle(path);
  if (!handle.has_value()) {
    LOG(ERROR) << "Invalid path: " << path;
    return -ENOENT;
  }

  auto file_size = GetFileSize(handle.value());
  if (!file_size.has_value()) {
    LOG(ERROR) << "Handle not found: " << path;
    return -ENOENT;
  }

  // Returns success, if offset eqauls to or exceeds the size.
  if (file_size.value() <= off)
    return 0;

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  int return_value = -EIO;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ServerProxyFileSystem::ReadInternal,
                                base::Unretained(this), &event, handle.value(),
                                buf, size, off, &return_value));
  event.Wait();
  return return_value;
}

void ServerProxyFileSystem::ReadInternal(base::WaitableEvent* event,
                                         int64_t handle,
                                         char* buf,
                                         size_t size,
                                         off_t off,
                                         int* return_value) {
  proxy_service_->proxy()->GetVSockProxy()->Pread(
      handle, size, off,
      base::BindOnce(
          [](base::WaitableEvent* event, char* buf, int* return_value,
             int error_code, const std::string& blob) {
            if (error_code != 0) {
              *return_value = -error_code;
            } else {
              memcpy(buf, blob.data(), blob.size());
              *return_value = static_cast<int>(blob.size());
            }
            event->Signal();
          },
          event, buf, return_value));
}

int ServerProxyFileSystem::Release(const char* path,
                                   struct fuse_file_info* fi) {
  auto handle = ParseHandle(path);
  if (!handle.has_value()) {
    LOG(ERROR) << "Invalid path: " << path;
    return -ENOENT;
  }

  {
    base::AutoLock lock(size_map_lock_);
    if (size_map_.erase(handle.value()) == 0) {
      LOG(ERROR) << "Handle not found: " << path;
      return -ENOENT;
    }
  }

  // |this| outlives of |task_runner_|, so passing raw |this| pointer here is
  // safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ServerProxyFileSystem* self, int64_t handle) {
            self->proxy_service_->proxy()->GetVSockProxy()->Close(handle);
          },
          this, handle.value()));
  return 0;
}

int ServerProxyFileSystem::ReadDir(const char* path,
                                   void* buf,
                                   fuse_fill_dir_t filler,
                                   off_t offset,
                                   struct fuse_file_info* fi) {
  // Just returns as if it is empty directory.
  filler(buf, ".", nullptr, 0);
  filler(buf, "..", nullptr, 0);
  return 0;
}

void ServerProxyFileSystem::Init(struct fuse_conn_info* conn) {
  // TODO(hidehiko): Drop CAPS_SYS_ADMIN with minijail setup.
  LOG(INFO) << "Starting ServerProxy.";
  proxy_service_ = std::make_unique<ProxyService>(std::move(factory_));

  // Must succeed, otherwise ServerProxy wouldn't run. Unfortunately,
  // there's no way to return an error in case of failure, terminate the
  // process instead.
  CHECK(proxy_service_->Start()) << "Failed to start ServerProxy.";
  LOG(INFO) << "ServerProxy has been started successfully.";
  task_runner_ = proxy_service_->GetTaskRunner();

  for (auto& observer : observer_list_)
    observer.OnInit();
}

base::ScopedFD ServerProxyFileSystem::RegisterHandle(int64_t handle,
                                                     uint64_t size) {
  {
    base::AutoLock lock(size_map_lock_);
    auto result = size_map_.emplace(handle, size);
    if (!result.second) {
      LOG(ERROR) << "The handle was already registered: " << handle
                 << ", old_size: " << result.first->second
                 << ", new_size: " << size;
      return {};
    }
  }

  // Currently read-only file descriptor is only supported.
  return base::ScopedFD(HANDLE_EINTR(
      open(mount_path_.Append(base::Int64ToString(handle)).value().c_str(),
           O_RDONLY | O_CLOEXEC)));
}

void ServerProxyFileSystem::RunWithVSockProxyInSyncForTesting(
    base::OnceCallback<void(VSockProxy*)> callback) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](ServerProxyFileSystem* self, base::WaitableEvent* event,
                        base::OnceCallback<void(VSockProxy*)> callback) {
                       std::move(callback).Run(
                           self->proxy_service_->proxy()->GetVSockProxy());
                       event->Signal();
                     },
                     this, &event, std::move(callback)));
  event.Wait();
}

base::Optional<size_t> ServerProxyFileSystem::GetFileSize(int64_t handle) {
  base::AutoLock lock_(size_map_lock_);
  auto it = size_map_.find(handle);
  if (it == size_map_.end())
    return base::nullopt;
  return it->second;
}

}  // namespace arc
