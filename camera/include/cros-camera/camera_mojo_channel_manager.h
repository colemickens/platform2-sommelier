/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_CROS_CAMERA_CAMERA_MOJO_CHANNEL_MANAGER_H_
#define INCLUDE_CROS_CAMERA_CAMERA_MOJO_CHANNEL_MANAGER_H_

#include "mojo/jda/jpeg_decode_accelerator.mojom.h"

namespace cros {

// There are many places that need to initialize Mojo and use related channels.
// This class is used to manage them together.
class CameraMojoChannelManager {
 public:
  // Gets the singleton instance. Returns nullptr if any error occurrs during
  // instance creation.
  static CameraMojoChannelManager* GetInstance();

  virtual ~CameraMojoChannelManager() {}

  // Creates a new JpegDecodeAccelerator.
  virtual void CreateJpegDecodeAccelerator(
      mojom::JpegDecodeAcceleratorRequest request) = 0;

  // TODO(mojahsu): Add CreateCameraAlgorithmOps API
};

}  // namespace cros

#endif  // INCLUDE_CROS_CAMERA_CAMERA_MOJO_CHANNEL_MANAGER_H_
