// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/socket_stream.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/optional.h>
#include <base/posix/eintr_wrapper.h>
#include <base/posix/unix_domain_socket_linux.h>
#include <base/strings/string_piece.h>
#include <gtest/gtest.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"

namespace arc {
namespace {

class SocketStreamTest : public testing::Test {
 public:
  SocketStreamTest() = default;
  ~SocketStreamTest() override = default;

  void SetUp() override {
    auto sockets = CreateSocketPair();
    ASSERT_TRUE(sockets.has_value());
    stream_ = std::make_unique<SocketStream>(std::move(sockets.value().first));
    socket_ = std::move(sockets.value().second);
  }

 protected:
  std::unique_ptr<SocketStream> stream_;  // Paired with socket_.
  base::ScopedFD socket_;                 // Paired with stream_.

 private:
  DISALLOW_COPY_AND_ASSIGN(SocketStreamTest);
};

TEST_F(SocketStreamTest, Read) {
  base::ScopedFD attached_fd(HANDLE_EINTR(open("/dev/null", O_RDONLY)));
  ASSERT_TRUE(attached_fd.is_valid());

  constexpr char kData[] = "abcdefghijklmnopqrstuvwxyz";
  ASSERT_TRUE(base::UnixDomainSocket::SendMsg(
      socket_.get(), kData, sizeof(kData), {attached_fd.get()}));

  auto read_result = stream_->Read();
  EXPECT_EQ(0, read_result.error_code);
  EXPECT_EQ(base::StringPiece(kData, sizeof(kData)), read_result.blob);
  EXPECT_EQ(1, read_result.fds.size());
}

TEST_F(SocketStreamTest, ReadEOF) {
  // Close the other side immediately.
  socket_.reset();

  auto read_result = stream_->Read();
  EXPECT_EQ(0, read_result.error_code);
  EXPECT_TRUE(read_result.blob.empty());
  EXPECT_TRUE(read_result.fds.empty());
}

TEST_F(SocketStreamTest, ReadError) {
  // Pass invalid FD.
  auto read_result = SocketStream(base::ScopedFD()).Read();
  EXPECT_EQ(EBADF, read_result.error_code);
}

TEST_F(SocketStreamTest, Write) {
  base::ScopedFD attached_fd(HANDLE_EINTR(open("/dev/null", O_RDONLY)));
  ASSERT_TRUE(attached_fd.is_valid());

  constexpr char kData[] = "abcdefghijklmnopqrstuvwxyz";
  {
    std::vector<base::ScopedFD> fds;
    fds.emplace_back(std::move(attached_fd));
    ASSERT_TRUE(
        stream_->Write(std::string(kData, sizeof(kData)), std::move(fds)));
  }
  std::string read_data;
  read_data.resize(sizeof(kData));
  std::vector<base::ScopedFD> fds;
  ASSERT_EQ(sizeof(kData),
            base::UnixDomainSocket::RecvMsg(socket_.get(), &read_data[0],
                                            sizeof(kData), &fds));
  EXPECT_EQ(1, fds.size());
}

}  // namespace
}  // namespace arc
