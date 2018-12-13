// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/vsock_stream.h"

#include <tuple>
#include <utility>

#include <base/files/scoped_file.h>
#include <gtest/gtest.h>
#include <google/protobuf/util/message_differencer.h>

#include "arc/vm/vsock_proxy/file_descriptor_util.h"
#include "arc/vm/vsock_proxy/message.pb.h"

namespace arc {
namespace {

TEST(VSockStreamTest, ReadWrite) {
  auto sockpair = CreateSocketPair();
  ASSERT_TRUE(sockpair.has_value());
  base::ScopedFD fd1;
  base::ScopedFD fd2;
  std::tie(fd1, fd2) = std::move(sockpair).value();

  arc_proxy::Message message;
  message.set_handle(10);
  message.set_data("abcde");
  {
    VSockStream stream(std::move(fd1));
    ASSERT_TRUE(stream.Write(arc_proxy::Message(message)));
  }

  auto read_message = VSockStream(std::move(fd2)).Read();
  ASSERT_TRUE(read_message.has_value());

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      message, read_message.value()))
      << "expect: " << message.DebugString()
      << ", actual: " << message.DebugString();
}

}  // namespace
}  // namespace arc
