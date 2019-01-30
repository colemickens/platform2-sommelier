// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/pipe_stream.h"

#include <string>
#include <tuple>
#include <utility>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/optional.h>
#include <gtest/gtest.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"
#include "arc/vm/vsock_proxy/message.pb.h"

namespace arc {
namespace {

TEST(PipeStreamTest, ReadWrite) {
  constexpr char kData[] = "abcdefghijklmnopqrstuvwxyz";

  arc_proxy::VSockMessage message;
  {
    auto pipes = CreatePipe();
    ASSERT_TRUE(pipes.has_value());
    base::ScopedFD read_fd;
    base::ScopedFD write_fd;
    std::tie(read_fd, write_fd) = std::move(pipes).value();
    ASSERT_TRUE(
        base::WriteFileDescriptor(write_fd.get(), kData, sizeof(kData)));
    write_fd.reset();
    ASSERT_TRUE(PipeStream(std::move(read_fd)).Read(&message));
  }

  std::string read_data;
  read_data.resize(sizeof(kData));
  {
    auto pipes = CreatePipe();
    ASSERT_TRUE(pipes.has_value());
    base::ScopedFD read_fd;
    base::ScopedFD write_fd;
    std::tie(read_fd, write_fd) = std::move(pipes).value();
    ASSERT_TRUE(PipeStream(std::move(write_fd)).Write(message.mutable_data()));
    ASSERT_TRUE(base::ReadFromFD(read_fd.get(), &read_data[0], sizeof(kData)));
  }

  EXPECT_EQ(std::string(kData, sizeof(kData)), read_data);
}

TEST(PipeStreamTest, Close) {
  auto pipes = CreatePipe();
  ASSERT_TRUE(pipes.has_value());
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  std::tie(read_fd, write_fd) = std::move(pipes).value();
  // Close the write side then, the read message should be empty.
  write_fd.reset();
  arc_proxy::VSockMessage message;
  ASSERT_TRUE(PipeStream(std::move(read_fd)).Read(&message));
  EXPECT_TRUE(message.has_close());
}

}  // namespace
}  // namespace arc
