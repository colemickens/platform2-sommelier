// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/socket_stream.h"

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/optional.h>
#include <base/posix/unix_domain_socket_linux.h>
#include <gtest/gtest.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"
#include "arc/vm/vsock_proxy/message.pb.h"

namespace arc {
namespace {

TEST(SocketStreamTest, ReadWrite) {
  constexpr char kData[] = "abcdefghijklmnopqrstuvwxyz";

  arc_proxy::Message message;
  {
    auto sockets = CreateSocketPair();
    ASSERT_TRUE(sockets.has_value());
    base::ScopedFD fd1;
    base::ScopedFD fd2;
    std::tie(fd1, fd2) = std::move(sockets).value();
    ASSERT_TRUE(
        base::UnixDomainSocket::SendMsg(fd1.get(), kData, sizeof(kData), {}));
    fd1.reset();
    ASSERT_TRUE(
        SocketStream(std::move(fd2), nullptr /* vsock_proxy */).Read(&message));
  }

  ASSERT_FALSE(message.data().empty());

  std::string read_data;
  read_data.resize(sizeof(kData));
  {
    auto sockets = CreateSocketPair();
    ASSERT_TRUE(sockets.has_value());
    base::ScopedFD fd1;
    base::ScopedFD fd2;
    std::tie(fd1, fd2) = std::move(sockets).value();
    ASSERT_TRUE(
        SocketStream(std::move(fd1), nullptr /* vsock_proxy */).Write(message));
    std::vector<base::ScopedFD> fds;
    ASSERT_EQ(sizeof(kData),
              base::UnixDomainSocket::RecvMsg(fd2.get(), &read_data[0],
                                              sizeof(kData), &fds));
    EXPECT_TRUE(fds.empty());
  }

  EXPECT_EQ(std::string(kData, sizeof(kData)), read_data);
}

// TODO(hidehiko): Add test for the FD passing.

TEST(SocketStreamTest, Close) {
  auto sockets = CreateSocketPair();
  ASSERT_TRUE(sockets.has_value());
  base::ScopedFD fd1;
  base::ScopedFD fd2;
  std::tie(fd1, fd2) = std::move(sockets).value();
  // Close the write side then, the read message should be empty.
  fd1.reset();
  arc_proxy::Message message;
  ASSERT_TRUE(
      SocketStream(std::move(fd2), nullptr /* vsock_proxy */).Read(&message));
  EXPECT_TRUE(message.data().empty());
  EXPECT_EQ(0, message.transferred_fd_size());
}

}  // namespace
}  // namespace arc
