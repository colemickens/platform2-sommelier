/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_IPC_UTIL_H_
#define HAL_ADAPTER_IPC_UTIL_H_

#include <string>

#include <base/files/scoped_file.h>

namespace base {
class FilePath;
}  // namespace base

namespace internal {

bool CreateServerUnixDomainSocket(const base::FilePath& socket_path,
                                  int* server_listen_fd);

bool ServerAcceptConnection(int server_listen_fd, int* server_socket);

base::ScopedFD CreateClientUnixDomainSocket(const base::FilePath& socket_path);

}  // namespace internal

#endif  // HAL_ADAPTER_IPC_UTIL_H_
