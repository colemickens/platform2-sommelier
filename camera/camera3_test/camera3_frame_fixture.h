// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_
#define CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_

#include "camera3_test/camera3_stream_fixture.h"

namespace camera3_test {

class Camera3FrameFixture : public Camera3StreamFixture {
 public:
  explicit Camera3FrameFixture(int cam_id) : Camera3StreamFixture(cam_id) {}

 protected:
  // Create and process capture request of given metadata |metadata|. The frame
  // number of the created request is returned if |frame_number| is not null.
  int CreateCaptureRequestByMetadata(const CameraMetadataUniquePtr& metadata,
                                     uint32_t* frame_number);

  // Create and process capture request of given template |type|. The frame
  // number of the created request is returned if |frame_number| is not null.
  int CreateCaptureRequestByTemplate(int type, uint32_t* frame_number);

  // Wait for shutter and capture result with timeout
  void WaitShutterAndCaptureResult(const struct timespec& timeout);

 private:
  // Create and process capture request of given metadata |metadata|. The frame
  // number of the created request is returned if |frame_number| is not null.
  int32_t CreateCaptureRequest(const camera_metadata_t& metadata,
                               uint32_t* frame_number);

  DISALLOW_COPY_AND_ASSIGN(Camera3FrameFixture);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_
