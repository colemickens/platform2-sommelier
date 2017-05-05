// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CAMERA3_TEST_CAMERA3_PREVIEW_FIXTURE_H_
#define CAMERA3_TEST_CAMERA3_PREVIEW_FIXTURE_H_

#include "camera3_service.h"

namespace camera3_test {

class Camera3PreviewFixture : public testing::Test {
 public:
  explicit Camera3PreviewFixture(std::vector<int> cam_ids)
      : cam_service_(cam_ids) {}

  virtual void SetUp() override;

  virtual void TearDown() override;

 protected:
  Camera3Service cam_service_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Camera3PreviewFixture);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_PREVIEW_FIXTURE_H_
