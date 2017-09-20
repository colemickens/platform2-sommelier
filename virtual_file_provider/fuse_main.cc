// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "virtual_file_provider/fuse_main.h"

#include <string>
#include <utility>

#include <fuse/fuse.h>

#include <base/files/scoped_file.h>
#include <base/posix/eintr_wrapper.h>

#include "virtual_file_provider/util.h"

namespace virtual_file_provider {

namespace {

constexpr char kFileSystemName[] = "virtual-file-provider";

struct Callbacks {
  Callbacks() = default;
  ~Callbacks() = default;

  SendReadRequestCallback send_read_request_callback;
  ReleaseCallback release_callback;
};

const Callbacks& GetCallbacks() {
  return *static_cast<const Callbacks*>(fuse_get_context()->private_data);
}

int GetAttr(const char* path, struct stat* stat) {
  // Everything except the root is a file.
  if (path == std::string("/")) {
    stat->st_mode = S_IFDIR;
    stat->st_nlink = 2;
  } else {
    stat->st_mode = S_IFREG;
    stat->st_nlink = 1;
  }
  return 0;
}

int Open(const char* path, struct fuse_file_info* fi) {
  // Use direct_io as we don't provide the file size in GetAttr().
  fi->direct_io = 1;
  return 0;
}

int Read(const char* path,
         char* buf,
         size_t size,
         off_t off,
         struct fuse_file_info* fi) {
  DCHECK_EQ('/', path[0]);
  // File name is the ID.
  std::string id(path + 1);

  // Create a pipe to receive data from chrome. By using pipe instead of D-Bus
  // to receive data, we can reliably avoid deadlock at read(), provided chrome
  // doesn't leak the file descriptor of the write end.
  int fds[2] = {-1, -1};
  if (pipe(fds) != 0) {
    PLOG(ERROR) << "pipe() failed.";
    return -EIO;
  }
  base::ScopedFD read_end(fds[0]), write_end(fds[1]);

  // Send read request to chrome with the write end of the pipe.
  GetCallbacks().send_read_request_callback.Run(
      id, off, size, std::move(write_end));

  // Read the data from the read end of the pipe.
  size_t result = 0;
  while (result < size) {
    ssize_t r = HANDLE_EINTR(read(read_end.get(), buf + result, size - result));
    if (r < 0) {
      return -EIO;
    }
    if (r == 0) {
      break;
    }
    result += r;
  }
  return result;
}

int Release(const char* path, struct fuse_file_info* fi) {
  DCHECK_EQ('/', path[0]);
  // File name is the ID.
  std::string id(path + 1);

  GetCallbacks().release_callback.Run(id);
  return 0;
}

int ReadDir(const char* path,
            void* buf,
            fuse_fill_dir_t filler,
            off_t offset,
            struct fuse_file_info* fi) {
  filler(buf, ".", nullptr, 0);
  filler(buf, "..", nullptr, 0);
  return 0;
}

void* Init(struct fuse_conn_info* conn) {
  CHECK(ClearCapabilities());
  // FUSE will overwrite the context's private_data with the return value.
  // Just return the current private_data.
  return fuse_get_context()->private_data;
}

}  // namespace

int FuseMain(const base::FilePath& mount_path,
             const SendReadRequestCallback& send_read_request_callback,
             const ReleaseCallback& release_callback) {
  const std::string path_str = mount_path.AsUTF8Unsafe();
  const char* fuse_argv[] = {
      kFileSystemName,
      path_str.c_str(),
      "-f",  // "-f" for foreground.
      // "-s" for single thread, as multi-threading may allow misbehaving
      // applications to exhaust finite resource of this process.
      "-s",
  };
  constexpr struct fuse_operations operations = {
      .getattr = GetAttr,
      .open = Open,
      .read = Read,
      .release = Release,
      .readdir = ReadDir,
      .init = Init,
  };
  Callbacks callbacks;
  callbacks.send_read_request_callback = send_read_request_callback;
  callbacks.release_callback = release_callback;
  void* private_data = &callbacks;
  return fuse_main(arraysize(fuse_argv),
                   const_cast<char**>(fuse_argv),
                   &operations,
                   private_data);
}

}  // namespace virtual_file_provider
