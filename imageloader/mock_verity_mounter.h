// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IMAGELOADER_MOCK_VERITY_MOUNTER_H_
#define IMAGELOADER_MOCK_VERITY_MOUNTER_H_

#include "verity_mounter.h"

#include <string>

#include "gmock/gmock.h"

namespace base {
void PrintTo(const base::FilePath& path, std::ostream* stream) {
  *stream << path.value();
}
}

namespace imageloader {

class MockVerityMounter : public VerityMounter {
 public:
  MockVerityMounter() {}
  MOCK_METHOD3(Mount, bool(const base::ScopedFD&, const base::FilePath&,
                           const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVerityMounter);
};

}  // namespace imageloader

#endif  // IMAGELOADER_MOCK_VERITY_MOUNTER_H_
