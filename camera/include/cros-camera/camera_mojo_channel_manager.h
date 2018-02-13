/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_CROS_CAMERA_CAMERA_MOJO_CHANNEL_MANAGER_H_
#define INCLUDE_CROS_CAMERA_CAMERA_MOJO_CHANNEL_MANAGER_H_

namespace cros {

// There are many places that need to initialize Mojo and use related channels.
// This class is used to manage them together.
class CameraMojoChannelManager {
 public:
  // Gets the singleton instance. Returns nullptr if any error occurrs during
  // instance creation.
  static CameraMojoChannelManager* GetInstance();

  virtual ~CameraMojoChannelManager() {}

  // TODO(mojahsu): Add GetJpegDecodeAccelerator dispatcher API.
  // TODO(mojahsu): Add GetCameraAlgorithmOps API
};

}  // namespace cros

#endif  // INCLUDE_CROS_CAMERA_CAMERA_MOJO_CHANNEL_MANAGER_H_
