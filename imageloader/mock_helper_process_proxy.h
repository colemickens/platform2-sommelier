// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGELOADER_MOCK_HELPER_PROCESS_PROXY_H_
#define IMAGELOADER_MOCK_HELPER_PROCESS_PROXY_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "imageloader/helper_process_proxy.h"
#include "imageloader/imageloader_impl.h"

namespace imageloader {

// Mock helper process used for unit testing.
class MockHelperProcessProxy : public HelperProcessProxy {
 public:
  MockHelperProcessProxy() {}
  ~MockHelperProcessProxy() {}

  // Sends a message telling the helper process to mount the file backed by |fd|
  // at the |path|.
  MOCK_METHOD4(SendMountCommand,
               bool(int, const std::string&, FileSystem, const std::string&));

  MOCK_METHOD3(SendUnmountAllCommand, bool(bool,
                                           const std::string&,
                                           std::vector<std::string>* paths));

  MOCK_METHOD1(SendUnmountCommand, bool(const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHelperProcessProxy);
};

}  // namespace imageloader

#endif  // IMAGELOADER_MOCK_HELPER_PROCESS_PROXY_H_
