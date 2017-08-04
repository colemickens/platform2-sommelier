// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_FILE_PROVIDER_FUSE_MAIN_H_
#define VIRTUAL_FILE_PROVIDER_FUSE_MAIN_H_

#include <string>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>

namespace virtual_file_provider {

using SendReadRequestCallback = base::Callback<void(
    const std::string& id, int64_t offset, int64_t size, base::ScopedFD fd)>;

// Mounts the FUSE file system on the given path and runs the FUSE main loop.
// This doesn't exit until the FUSE main loop exits (e.g. the file system is
// unmounted, or this process is terminated).
// Returns the value returned by libfuse's fuse_main().
int FuseMain(const base::FilePath& mount_path,
             const SendReadRequestCallback& send_read_request_callback);

}  // namespace virtual_file_provider

#endif  // VIRTUAL_FILE_PROVIDER_FUSE_MAIN_H_
