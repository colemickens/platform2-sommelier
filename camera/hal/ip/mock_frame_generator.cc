/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <memory>
#include <utility>

#include "hal/ip/mock_frame_generator.h"

#include <system/graphics.h>

#include "cros-camera/camera_buffer_manager.h"
#include "cros-camera/common.h"

namespace cros {

MockFrameGenerator::MockFrameGenerator()
    : thread_(), lock_(), stopping_(false), pattern_counter_(0) {}

MockFrameGenerator::~MockFrameGenerator() {
  StopStreaming();
}

int MockFrameGenerator::GetFormat() const {
  return HAL_PIXEL_FORMAT_YCbCr_420_888;
}

int MockFrameGenerator::GetWidth() const {
  return 1920;
}

int MockFrameGenerator::GetHeight() const {
  return 1080;
}

double MockFrameGenerator::GetFPS() const {
  return 30.0;
}

RequestQueue* MockFrameGenerator::StartStreaming() {
  {
    base::AutoLock l(lock_);
    stopping_ = false;
  }

  base::PlatformThread::Create(0, this, &thread_);
  return &request_queue_;
}

void MockFrameGenerator::StopStreaming() {
  {
    base::AutoLock l(lock_);
    stopping_ = true;
  }
  request_queue_.CancelPop();
}

void MockFrameGenerator::ThreadMain() {
  base::PlatformThread::SetName("Mock IP Camera");

  base::AutoLock l(lock_);
  while (!stopping_) {
    base::AutoUnlock r(lock_);
    std::unique_ptr<CaptureRequest> request = request_queue_.Pop();
    if (request) {
      FillBuffer(request->GetOutputBuffer()->buffer);
      request_queue_.NotifyCapture(std::move(request));
    }
  }
}

void MockFrameGenerator::FillBuffer(buffer_handle_t* buffer) {
  auto* buffer_manager = CameraBufferManager::GetInstance();
  buffer_manager->Register(*buffer);
  struct android_ycbcr ycbcr;
  buffer_manager->LockYCbCr(*buffer, 0, 0, 0, GetWidth(), GetHeight(), &ycbcr);

  // For some reason the buffer manager allocates this as an NV12 buffer even
  // though it's supposed to be YUV420
  memset(ycbcr.y, 127, ycbcr.ystride * GetHeight());
  memset(ycbcr.cb, pattern_counter_, ycbcr.cstride * GetHeight() / 2);
  pattern_counter_++;

  buffer_manager->Unlock(*buffer);
  buffer_manager->Deregister(*buffer);
}

}  // namespace cros
