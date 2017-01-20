// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_MODULE_FIXTURE_H_
#define CAMERA3_TEST_CAMERA3_MODULE_FIXTURE_H_

#include <dlfcn.h>
#include <base/logging.h>
#include <gtest/gtest.h>
#include <hardware/camera3.h>

namespace camera3_test {

const int kMaxNumCameras = 2;

class Camera3Module {
 public:
  Camera3Module();

  // Initialize
  int Initialize();

  // Get number of cameras
  int GetNumberOfCameras() const;

  // Get list of camera IDs
  std::vector<int> GetCameraIds() const;

  // Check if a stream format is supported
  bool IsFormatAvailable(int cam_id, int format) const;

  // Open camera device
  camera3_device* OpenDevice(int cam_id) const;

  // Get camera information
  int GetCameraInfo(int cam_id, camera_info* info) const;

 private:
  const camera_module_t* cam_module_;

  DISALLOW_COPY_AND_ASSIGN(Camera3Module);
};

class Camera3ModuleFixture : public testing::Test {
 public:
  Camera3ModuleFixture() {}

  virtual void SetUp() override;

 protected:
  Camera3Module cam_module_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Camera3ModuleFixture);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_MODULE_FIXTURE_H_
