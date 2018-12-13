// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/pipe_stream.h"

#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <tuple>
#include <utility>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/optional.h>
#include <gtest/gtest.h>
#include <google/protobuf/util/message_differencer.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"
#include "arc/vm/vsock_proxy/message.pb.h"

namespace arc {
namespace {

TEST(PipeStreamTest, ReadWrite) {
  constexpr char kData[] = "abcdefghijklmnopqrstuvwxyz";

  arc_proxy::Message message;
  {
    auto pipes = CreatePipe();
    ASSERT_TRUE(pipes.has_value());
    base::ScopedFD read_fd;
    base::ScopedFD write_fd;
    std::tie(read_fd, write_fd) = std::move(pipes).value();
    ASSERT_TRUE(
        base::WriteFileDescriptor(write_fd.get(), kData, sizeof(kData)));
    write_fd.reset();

    auto read_message = PipeStream(std::move(read_fd)).Read();
    ASSERT_TRUE(read_message.has_value());
    message = std::move(read_message).value();
  }

  std::string read_data;
  read_data.resize(sizeof(kData));
  {
    auto pipes = CreatePipe();
    ASSERT_TRUE(pipes.has_value());
    base::ScopedFD read_fd;
    base::ScopedFD write_fd;
    std::tie(read_fd, write_fd) = std::move(pipes).value();
    ASSERT_TRUE(PipeStream(std::move(write_fd)).Write(std::move(message)));
    ASSERT_TRUE(base::ReadFromFD(read_fd.get(), &read_data[0], sizeof(kData)));
  }

  EXPECT_EQ(std::string(kData, sizeof(kData)), read_data);
}

}  // namespace
}  // namespace arc
