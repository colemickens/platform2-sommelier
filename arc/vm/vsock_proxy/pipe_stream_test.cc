// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/pipe_stream.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/optional.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_piece.h>
#include <gtest/gtest.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"

namespace arc {
namespace {

class PipeStreamTest : public testing::Test {
 public:
  PipeStreamTest() = default;
  ~PipeStreamTest() override = default;

  void SetUp() override {
    auto pipes = CreatePipe();
    ASSERT_TRUE(pipes.has_value());
    std::tie(read_fd_, write_fd_) = std::move(pipes.value());
  }

 protected:
  base::ScopedFD read_fd_;
  base::ScopedFD write_fd_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PipeStreamTest);
};

TEST_F(PipeStreamTest, Read) {
  constexpr char kData[] = "abcdefghijklmnopqrstuvwxyz";
  ASSERT_TRUE(base::WriteFileDescriptor(write_fd_.get(), kData, sizeof(kData)));

  auto read_result = PipeStream(std::move(read_fd_)).Read();
  EXPECT_EQ(0, read_result.error_code);
  EXPECT_EQ(base::StringPiece(kData, sizeof(kData)), read_result.blob);
  EXPECT_TRUE(read_result.fds.empty());
}

TEST_F(PipeStreamTest, ReadEOF) {
  // Close the write end immediately.
  write_fd_.reset();

  auto read_result = PipeStream(std::move(read_fd_)).Read();
  EXPECT_EQ(0, read_result.error_code);
  EXPECT_TRUE(read_result.blob.empty());
  EXPECT_TRUE(read_result.fds.empty());
}

TEST_F(PipeStreamTest, ReadError) {
  // Pass invalid FD.
  auto read_result = PipeStream(base::ScopedFD()).Read();
  EXPECT_EQ(EBADF, read_result.error_code);
}

TEST_F(PipeStreamTest, Write) {
  constexpr char kData[] = "abcdefghijklmnopqrstuvwxyz";
  ASSERT_TRUE(PipeStream(std::move(write_fd_))
                  .Write(std::string(kData, sizeof(kData)), {}));

  std::string read_data;
  read_data.resize(sizeof(kData));
  ASSERT_TRUE(base::ReadFromFD(read_fd_.get(), &read_data[0], sizeof(kData)));
  EXPECT_EQ(std::string(kData, sizeof(kData)), read_data);
}

TEST_F(PipeStreamTest, WriteFD) {
  base::ScopedFD attached_fd(HANDLE_EINTR(open("/dev/null", O_RDONLY)));
  ASSERT_TRUE(attached_fd.is_valid());

  constexpr char kData[] = "abcdefghijklmnopqrstuvwxyz";
  std::vector<base::ScopedFD> fds;
  fds.emplace_back(std::move(attached_fd));
  // Not supported.
  ASSERT_FALSE(PipeStream(std::move(write_fd_))
                   .Write(std::string(kData, sizeof(kData)), std::move(fds)));
}

}  // namespace
}  // namespace arc
