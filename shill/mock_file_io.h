// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_FILE_IO_H_
#define SHILL_MOCK_FILE_IO_H_

#include "shill/file_io.h"

#include <gmock/gmock.h>

namespace shill {

class MockFileIO : public FileIO {
 public:
  MockFileIO() {}
  ~MockFileIO() override {}
  MOCK_METHOD3(Write, ssize_t(int fd, const void* buf, size_t count));
  MOCK_METHOD3(Read, ssize_t(int fd, void* buf, size_t count));
  MOCK_METHOD1(Close, int(int fd));
  MOCK_METHOD1(SetFdNonBlocking, int(int fd));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFileIO);
};

}  // namespace shill

#endif  // SHILL_MOCK_FILE_IO_H_
