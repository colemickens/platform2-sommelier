/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_IP_MOCK_FRAME_GENERATOR_H_
#define CAMERA_HAL_IP_MOCK_FRAME_GENERATOR_H_

#include "hal/ip/ip_camera_device.h"

namespace cros {

// A mock IP camera that generates a simple test pattern
class MockFrameGenerator : public IpCameraDevice,
                           public base::PlatformThread::Delegate {
 public:
  MockFrameGenerator();
  ~MockFrameGenerator();

  int GetFormat() const override;
  int GetWidth() const override;
  int GetHeight() const override;
  double GetFPS() const override;

  RequestQueue* StartStreaming() override;
  void StopStreaming() override;

 private:
  void ThreadMain() override;
  void FillBuffer(buffer_handle_t* buffer);

  base::PlatformThreadHandle thread_;

  base::Lock lock_;
  bool stopping_;

  uint8_t pattern_counter_;

  DISALLOW_COPY_AND_ASSIGN(MockFrameGenerator);
};

}  // namespace cros

#endif  // CAMERA_HAL_IP_MOCK_FRAME_GENERATOR_H_
