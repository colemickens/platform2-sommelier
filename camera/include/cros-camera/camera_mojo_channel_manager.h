/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_INCLUDE_CROS_CAMERA_CAMERA_MOJO_CHANNEL_MANAGER_H_
#define CAMERA_INCLUDE_CROS_CAMERA_CAMERA_MOJO_CHANNEL_MANAGER_H_

#include <memory>

#include "mojo/algorithm/camera_algorithm.mojom.h"
#include "mojo/jda/jpeg_decode_accelerator.mojom.h"
#include "mojo/jea/jpeg_encode_accelerator.mojom.h"

namespace cros {

// There are many places that need to initialize Mojo and use related channels.
// This class is used to manage them together.
class CameraMojoChannelManager {
 public:
  static std::unique_ptr<CameraMojoChannelManager> CreateInstance();

  virtual ~CameraMojoChannelManager() {}

  // Creates a new JpegDecodeAccelerator.
  virtual void CreateJpegDecodeAccelerator(
      mojom::JpegDecodeAcceleratorRequest request) = 0;

  // Creates a new JpegEncodeAccelerator.
  virtual void CreateJpegEncodeAccelerator(
      mojom::JpegEncodeAcceleratorRequest request) = 0;

  // Create a new CameraAlgorithmOpsPtr.
  virtual mojom::CameraAlgorithmOpsPtr CreateCameraAlgorithmOpsPtr() = 0;
};

}  // namespace cros

#endif  // CAMERA_INCLUDE_CROS_CAMERA_CAMERA_MOJO_CHANNEL_MANAGER_H_
