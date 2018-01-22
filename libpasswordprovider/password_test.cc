// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libpasswordprovider/password.h"

#include <string>

#include <base/files/file_util.h>
#include <gtest/gtest.h>

namespace password_provider {

namespace {

// Write the given data to a pipe. Returns the read end of the pipe.
int WriteSizeAndDataToPipe(const std::string& data) {
  int write_pipe[2];
  EXPECT_EQ(0, pipe(write_pipe));

  EXPECT_TRUE(
      base::WriteFileDescriptor(write_pipe[1], data.c_str(), data.size()));

  return write_pipe[0];
}

}  // namespace

// Basic memory allocation should succeed.
TEST(Password, CreatePasswordWithMemoryAllocation) {
  Password password;
  EXPECT_TRUE(password.Init());

  // Expect Password buffer size to be 1 page minus 1 byte reserved for the null
  // terminator
  size_t page_size = sysconf(_SC_PAGESIZE);
  EXPECT_EQ(page_size - 1, password.max_size());

  EXPECT_EQ(0, password.size());

  EXPECT_TRUE(password.GetRaw());
}

// Creating a Password object without memory allocation should do nothing.
TEST(Password, CreatePasswordWithNoMemoryAllocation) {
  Password password;
  EXPECT_EQ(0, password.size());
  EXPECT_EQ(0, password.max_size());
  // Should not segfault due to freeing memory not allocated.
}

TEST(Password, CreatePasswordFromFileDescriptor) {
  const std::string kTestStringPassword("mypassword");
  int fd = WriteSizeAndDataToPipe(kTestStringPassword);
  EXPECT_NE(-1, fd);

  auto password =
      Password::CreateFromFileDescriptor(fd, kTestStringPassword.size());
  ASSERT_TRUE(password);
  EXPECT_EQ(kTestStringPassword.size(), password->size());
  EXPECT_EQ(0, strncmp(kTestStringPassword.c_str(), password->GetRaw(),
                       password->size()));
}

TEST(Password, CreatePasswordGreaterThanMaxSize) {
  const std::string kTestStringPassword("mypassword");
  int fd = WriteSizeAndDataToPipe(kTestStringPassword);
  EXPECT_NE(-1, fd);

  // (page size - 1) is the max size of the Password buffer.
  size_t page_size = sysconf(_SC_PAGESIZE);
  auto password = Password::CreateFromFileDescriptor(fd, page_size);
  EXPECT_EQ(nullptr, password);
}

}  // namespace password_provider
