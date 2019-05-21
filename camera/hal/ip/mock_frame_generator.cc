/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "cros-camera/common.h"
#include "cros-camera/ipc_util.h"
#include "hal/ip/mock_frame_generator.h"

namespace cros {

MockFrameGenerator::MockFrameGenerator()
    : running_(false), pattern_counter_(0) {}

int MockFrameGenerator::Init() {
  if (!shm_.CreateAndMapAnonymous(GetWidth() * GetHeight() * 3 / 2)) {
    LOGF(ERROR) << "Unable to create/map shared memory";
    return -ENOMEM;
  }
  shm_y_ = shm_.memory();
  shm_u_ = static_cast<uint8_t*>(shm_y_) + GetWidth() * GetHeight();
  shm_v_ = static_cast<uint8_t*>(shm_u_) + (GetWidth() * GetHeight() / 4);

  // Set the Y and U planes to all 0
  memset(shm_y_, 0, GetWidth() * GetHeight());
  memset(shm_u_, 0, GetWidth() * GetHeight() / 4);

  return 0;
}

MockFrameGenerator::~MockFrameGenerator() {}

int32_t MockFrameGenerator::GetWidth() const {
  return 1920;
}

int32_t MockFrameGenerator::GetHeight() const {
  return 1080;
}

mojom::PixelFormat MockFrameGenerator::GetFormat() const {
  return mojom::PixelFormat::YUV_420;
}

double MockFrameGenerator::GetFps() const {
  return 30.0;
}

void MockFrameGenerator::StartStreaming() {
  if (!listener_) {
    LOGF(ERROR) << "Called start streaming without a frame listener";
  }
  running_ = true;
  FrameLoop();
}

void MockFrameGenerator::StopStreaming() {
  running_ = false;
}

void MockFrameGenerator::FrameLoop() {
  DCHECK(ipc_task_runner_->RunsTasksOnCurrentThread());
  if (!running_) {
    return;
  }
  if (listener_) {
    // modulate the V plane
    memset(shm_v_, pattern_counter_, GetWidth() * GetHeight() / 4);

    listener_->OnFrameCaptured(WrapPlatformHandle(dup(shm_.handle().fd)),
                               shm_.requested_size());
  }

  pattern_counter_++;

  ipc_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MockFrameGenerator::FrameLoop, base::Unretained(this)),
      base::TimeDelta::FromSecondsD(1 / GetFps()));
}

}  // namespace cros
