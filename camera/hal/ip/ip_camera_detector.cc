/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/ip/ip_camera_detector.h"
#include "hal/ip/mock_frame_generator.h"

namespace cros {

int IpCameraDetector::Observer::OnDeviceConnected(IpCameraDevice* /*device*/) {
  return -1;
}

void IpCameraDetector::Observer::OnDeviceDisconnected(int /*id*/) {}

IpCameraDetector::IpCameraDetector(Observer* observer)
    : observer_(observer), thread_handle_(), cameras_() {}

IpCameraDetector::~IpCameraDetector() {
  Stop();
}

bool IpCameraDetector::Start() {
  base::PlatformThread::Create(0, this, &thread_handle_);
  return true;
}

void IpCameraDetector::Stop() {
  base::PlatformThread::Join(thread_handle_);
  while (!cameras_.empty()) {
    auto map_iter = cameras_.begin();
    delete map_iter->second;
    cameras_.erase(map_iter->first);
  }
}

void IpCameraDetector::ThreadMain() {
  base::PlatformThread::SetName("IP Camera Detector");
  // TODO(pceballos): For now just add the mock device on startup after a short
  // sleep
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(5));
  IpCameraDevice* device = new MockFrameGenerator();
  int id = observer_->OnDeviceConnected(device);
  cameras_[id] = device;
}

}  // namespace cros
