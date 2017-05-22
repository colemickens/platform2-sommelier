/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ARC_CAMERA_ALGORITHM_H_
#define INCLUDE_ARC_CAMERA_ALGORITHM_H_

#include <stdint.h>

// This is the interfaces that the camera algorithm library shall implement.

#define CAMERA_ALGORITHM_MODULE_INFO_SYM CAMI
#define CAMERA_ALGORITHM_MODULE_INFO_SYM_AS_STR "CAMI"

extern "C" {
typedef struct camera_algorithm_callback_ops {
  int32_t (*return_callback)(
      const struct camera_algorithm_callback_ops* callback,
      int32_t buffer_handle);
} camera_algorithm_callback_ops_t;

typedef struct {
  // This method is one-time initialization that registers a callback function
  // for the camera algorithm library to return a buffer handle. It must be
  // called before any other functions.
  //
  // Args:
  //    |callback_ops|: Pointer to callback functions.
  //
  // Returns:
  //    0 on success; corresponding error code on failure.
  int32_t (*initialize)(const camera_algorithm_callback_ops_t* callback_ops);

  // This method registers a buffer to the camera algorithm library and gets
  // the handle associated with it.
  //
  // Args:
  //    |buffer_fd|: The buffer file descriptor to register.
  //
  // Returns:
  //    A handle on success; corresponding error code on failure.
  int32_t (*register_buffer)(int buffer_fd);

  // This method posts a request for the camera algorithm library to process the
  // given buffer.
  //
  // Args:
  //    |req_header|: The request header indicating request details. The
  //      interpretation depends on the HAL implementation. This is only valid
  //      during the function call and is invalidated after the function
  //      returns.
  //    |size|: Size of request header.
  //    |buffer_handle|: Handle of the buffer to process.
  //
  // Returns:
  //    0 on success; corresponding error code on failure.
  int32_t (*request)(uint8_t req_header[],
                     uint32_t size,
                     int32_t buffer_handle);

  // This method deregisters buffers to the camera algorithm library. The camera
  // algorithm shall release all the registered buffers on return of this
  // function.
  //
  // Args:
  //    |buffer_handles|: The buffer handles to deregister. This is only valid
  //      during the function call and is invalidated after the function
  //      returns.
  //    |size|: Size of the buffer handle array.
  //
  // Returns:
  //    A handle on success; -1 on failure.
  void (*deregister_buffers)(int32_t buffer_handles[], uint32_t size);
} camera_algorithm_ops_t;
}

#endif  // INCLUDE_ARC_CAMERA_ALGORITHM_H_
