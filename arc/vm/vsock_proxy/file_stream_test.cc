// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/file_stream.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/posix/eintr_wrapper.h>
#include <gtest/gtest.h>

#include "arc/vm/vsock_proxy/message.pb.h"

namespace arc {
namespace {

// Make sure Pread() works for regular case.
TEST(FileStreamTest, Pread) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path = temp_dir.GetPath().Append("test_file.txt");
  constexpr char kFileContent[] = "abcdefghijklmnopqrstuvwxyz";
  // Trim trailing '\0'.
  ASSERT_EQ(sizeof(kFileContent) - 1,
            base::WriteFile(file_path, kFileContent, sizeof(kFileContent) - 1));

  base::ScopedFD fd(HANDLE_EINTR(open(file_path.value().c_str(), O_RDONLY)));
  ASSERT_TRUE(fd.is_valid());

  FileStream stream(std::move(fd));
  arc_proxy::PreadResponse response;
  ASSERT_TRUE(stream.Pread(10, 10, &response));
  EXPECT_EQ(0, response.error_code());
  EXPECT_EQ("klmnopqrst", response.blob());

  // Test for EOF. Result |blob| should contain only the available bytes.
  ASSERT_TRUE(stream.Pread(10, 20, &response));
  EXPECT_EQ(0, response.error_code());
  EXPECT_EQ("uvwxyz", response.blob());
}

TEST(FileStreamTest, PreadError) {
  // Use -1 (invalid file descriptor) to let pread(2) return error in Pread().
  FileStream stream{base::ScopedFD()};
  arc_proxy::PreadResponse response;
  ASSERT_TRUE(stream.Pread(10, 10, &response));
  EXPECT_EQ(EBADF, response.error_code());
}

}  // namespace
}  // namespace arc
