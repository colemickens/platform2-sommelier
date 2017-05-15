// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HAMMERD_MOCK_FMAP_UTILS_H_
#define HAMMERD_MOCK_FMAP_UTILS_H_

#include <string>

#include <gmock/gmock.h>

#include "hammerd/fmap_utils.h"

namespace hammerd {

class MockFmap : public FmapInterface {
 public:
  MOCK_METHOD2(Find, int64_t(const uint8_t* image, unsigned int len));
  MOCK_METHOD2(FindArea,
               const fmap_area*(const fmap* fmap, const std::string& name));
};

}  // namespace hammerd
#endif  // HAMMERD_MOCK_FMAP_UTILS_H_
