/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_ADAPTER_FRAME_NUMBER_MAPPER_H_
#define CAMERA_HAL_ADAPTER_FRAME_NUMBER_MAPPER_H_

#include <map>
#include <set>
#include <vector>

#include <hardware/camera3.h>

#include <base/synchronization/lock.h>

namespace cros {

class FrameNumberMapper {
 public:
  FrameNumberMapper();

  // Creates and returns a new HAL frame number every time this is called.
  uint32_t GetHalFrameNumber(uint32_t framework_frame_number);

  // Returns the framework frame number that corresponds to the HAL frame
  // number. Returns 0 if no corresponding framework frame number is found.
  uint32_t GetFrameworkFrameNumber(uint32_t hal_frame_number);

  bool IsAddedFrame(uint32_t hal_frame_number);

  // Registers a capture request. It initializes a ResultStatus of the request
  // which allows us to remove frame number mappings when possible.
  void RegisterCaptureRequest(const camera3_capture_request_t* request,
                              bool is_request_split,
                              bool is_request_added);

  // Registers a capture result. It updates the status of the result and check
  // if the request is completely fulfilled. When fulfilled, we remove its frame
  // number mapping.
  void RegisterCaptureResult(const camera3_capture_result_t* result,
                             int32_t partial_result_count);

  // Reads the input |message| and transforms it into a list of new messages for
  // Notify() following the flow below:
  //
  // We don't transform at all if the frame number doesn't belong to a split
  // request.
  // If a request is split,
  // CAMERA3_MSG_SHUTTER - Trim if the frame is an added frame.
  // CAMERA3_MSG_ERROR - Follow the logic below:
  //   CAMERA3_MSG_ERROR_DEVICE
  //     - Submit (device should close after the first one is sent)
  //   CAMERA3_MSG_ERROR_REQUEST
  //     - Convert to CAMERA3_MSG_ERROR_BUFFER + CAMERA3_MSG_ERROR_RESULT if
  //     it's the original frame
  //     - Convert to CAMERA3_MSG_ERROR_BUFFER if it's the added frame
  //   CAMERA3_MSG_ERROR_RESULT
  //     - Submit if it's the original frame
  //     - Trim if it's the added frame
  //   CAMERA3_MSG_ERROR_BUFFER
  //     - Submit
  void PreprocessNotifyMsg(const camera3_notify_msg_t* msg,
                           std::vector<camera3_notify_msg_t>* msgs,
                           camera3_stream_t* zsl_stream);

 private:
  struct ResultStatus {
    bool has_pending_input_buffer;
    uint32_t num_pending_output_buffers;
    bool has_pending_result;
  };

  // Finishes a HAL frame number gotten previously.
  void FinishHalFrameNumber(uint32_t hal_frame_number);

  // Whether this frame number belongs to a request that had been split.
  bool IsRequestSplit(uint32_t hal_frame_number);

  // Mapping from framework frame number to HAL frame number.
  std::map<uint32_t, uint32_t> frame_number_map_;
  uint32_t next_frame_number_;
  // Lock that protects |frame_number_map_| and |next_frame_number_|.
  base::Lock frame_number_lock_;

  // A map stores the status of a result which allows us to know when a frame
  // number mapping can be freed.
  std::map<uint32_t, ResultStatus> pending_result_status_;
  base::Lock pending_result_status_lock_;

  // A mapping from HAL frame number to its list of output streams.
  std::map<uint32_t, std::vector<camera3_stream_t*>> request_streams_map_;
  base::Lock request_streams_map_lock_;

  // A set that stores the HAL frame number of the added capture requests.
  std::set<uint32_t> added_frame_numbers_;
  base::Lock added_frame_numbers_lock_;
};

}  // namespace cros

#endif  // CAMERA_HAL_ADAPTER_FRAME_NUMBER_MAPPER_H_
