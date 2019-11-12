// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/socket_stream.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/optional.h>
#include <base/posix/eintr_wrapper.h>
#include <base/posix/unix_domain_socket_linux.h>
#include <base/run_loop.h>
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
  base::MessageLoopForIO message_loop_;
  base::FileDescriptorWatcher watcher_{&message_loop_};

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

TEST_F(SocketStreamTest, PendingWrite) {
  int sndbuf_value = 0;
  socklen_t len = sizeof(sndbuf_value);
  ASSERT_EQ(
      0, getsockopt(socket_.get(), SOL_SOCKET, SO_SNDBUF, &sndbuf_value, &len));

  const std::string data1(sndbuf_value, 'a');
  const std::string data2(sndbuf_value, 'b');
  const std::string data3(sndbuf_value, 'c');

  // Write data1, data2, and data3 to the stream.
  ASSERT_TRUE(stream_->Write(data1, {}));
  ASSERT_TRUE(stream_->Write(data2, {}));
  ASSERT_TRUE(stream_->Write(data3, {}));

  // Read data1 from the other socket.
  std::string read_data;
  read_data.resize(sndbuf_value);
  std::vector<base::ScopedFD> fds;
  ASSERT_EQ(data1.size(),
            base::UnixDomainSocket::RecvMsg(socket_.get(), &read_data[0],
                                            read_data.size(), &fds));
  read_data.resize(data1.size());
  EXPECT_EQ(data1, read_data);

  // data2 is still pending.
  ASSERT_EQ(-1, base::UnixDomainSocket::RecvMsg(socket_.get(), &read_data[0],
                                                read_data.size(), &fds));
  ASSERT_EQ(EAGAIN, errno);

  // Now the socket's buffer is empty. Let the stream write data2 to the socket.
  base::RunLoop().RunUntilIdle();

  // Read data2 from the other socket.
  read_data.resize(sndbuf_value);
  ASSERT_EQ(data2.size(),
            base::UnixDomainSocket::RecvMsg(socket_.get(), &read_data[0],
                                            read_data.size(), &fds));
  read_data.resize(data2.size());
  EXPECT_EQ(data2, read_data);

  // data3 is still pending.
  ASSERT_EQ(-1, base::UnixDomainSocket::RecvMsg(socket_.get(), &read_data[0],
                                                read_data.size(), &fds));
  ASSERT_EQ(EAGAIN, errno);

  // Let the stream write data3 to the socket.
  base::RunLoop().RunUntilIdle();

  // Read data3 from the other socket.
  read_data.resize(sndbuf_value);
  ASSERT_EQ(data3.size(),
            base::UnixDomainSocket::RecvMsg(socket_.get(), &read_data[0],
                                            read_data.size(), &fds));
  read_data.resize(data3.size());
  EXPECT_EQ(data3, read_data);
}

}  // namespace
}  // namespace arc
