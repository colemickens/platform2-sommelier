// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_
#define CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_

#include <semaphore.h>

#include <unordered_map>

#include <gtest/gtest.h>
#include <hardware/camera3.h>
#include <hardware/hardware.h>

#include "camera3_stream_fixture.h"

namespace camera3_test {

class Camera3FrameFixture : public Camera3StreamFixture {
 public:
  explicit Camera3FrameFixture(int cam_id)
      : Camera3StreamFixture(cam_id),
        request_frame_number_(0),
        result_frame_number_(0) {}

  virtual void SetUp() override;

  virtual void TearDown() override;

 protected:
  // Create and process capture request
  int CreateCaptureRequest(int type);

  // Wait for capture result with timeout
  void WaitCaptureResult(const struct timespec& timeout);

  // Take a peek at number of received capture results
  int GetNumberOfCaptureResults();

  // Callback functions from HAL device
  virtual void ProcessCaptureResult(
      const camera3_capture_result* result) override;

  // Callback functions from HAL device
  virtual void Notify(const camera3_notify_msg* msg) override;

 private:
  uint32_t request_frame_number_;

  uint32_t result_frame_number_;

  std::unordered_map<uint32_t, camera3_capture_request_t> capture_request_;

  std::unordered_map<uint32_t, camera3_capture_result_t> capture_result_;

  sem_t shutter_sem_;

  sem_t capture_result_sem_;

  DISALLOW_COPY_AND_ASSIGN(Camera3FrameFixture);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_
