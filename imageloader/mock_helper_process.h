// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGELOADER_MOCK_HELPER_PROCESS_H_
#define IMAGELOADER_MOCK_HELPER_PROCESS_H_

#include "gmock/gmock.h"
#include "helper_process.h"

namespace imageloader {

// Mock helper process used for unit testing.
class MockHelperProcess : public HelperProcess {
 public:
  MockHelperProcess() {}
  ~MockHelperProcess() {}

  // Sends a message telling the helper process to mount the file backed by |fd|
  // at the |path|.
  MOCK_METHOD3(SendMountCommand,
               bool(int, const std::string&, const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHelperProcess);
};

}  // namespace imageloader

#endif // IMAGELOADER_MOCK_HELPER_PROCESS_H_
