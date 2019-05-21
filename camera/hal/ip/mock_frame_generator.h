/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_IP_MOCK_FRAME_GENERATOR_H_
#define CAMERA_HAL_IP_MOCK_FRAME_GENERATOR_H_

#include <base/memory/shared_memory.h>

#include "hal/ip/ip_camera.h"

namespace cros {

// A mock IP camera that generates a simple test pattern
class MockFrameGenerator : public IpCamera {
 public:
  MockFrameGenerator();
  ~MockFrameGenerator();
  int Init() override;

  int32_t GetWidth() const override;
  int32_t GetHeight() const override;
  mojom::PixelFormat GetFormat() const override;
  double GetFps() const override;

 private:
  void StartStreaming() override;
  void StopStreaming() override;
  void FrameLoop();

  bool running_;
  uint8_t pattern_counter_;
  base::SharedMemory shm_;
  void* shm_y_;
  void* shm_u_;
  void* shm_v_;

  DISALLOW_COPY_AND_ASSIGN(MockFrameGenerator);
};

}  // namespace cros

#endif  // CAMERA_HAL_IP_MOCK_FRAME_GENERATOR_H_
