// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGE_BURNER_IMAGE_BURNER_TEST_UTILS_H_
#define IMAGE_BURNER_IMAGE_BURNER_TEST_UTILS_H_

#include <stdint.h>

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "image-burner/image_burner_utils_interfaces.h"

namespace imageburn {

class MockFileSystemWriter : public FileSystemWriter {
 public:
  MOCK_METHOD1(Open, bool(const char*));
  MOCK_METHOD0(Close, bool());
  MOCK_METHOD2(Write, int(char*, int));
};

class MockFileSystemReader : public FileSystemReader {
 public:
  MOCK_METHOD1(Open, bool(const char*));
  MOCK_METHOD0(Close, bool());
  MOCK_METHOD0(GetSize, int64_t());
  MOCK_METHOD2(Read, int(char*, int));
};

class MockSignalSender : public SignalSender {
 public:
  MOCK_METHOD3(SendFinishedSignal, void(const char*, bool, const char*));
  MOCK_METHOD3(SendProgressSignal, void(int64_t, int64_t, const char*));
};

class MockRootPathGetter : public RootPathGetter {
 public:
  MOCK_METHOD1(GetRootPath, bool(std::string*));
};

}  // namespace imageburn

#endif  // IMAGE_BURNER_IMAGE_BURNER_TEST_UTILS_H_
