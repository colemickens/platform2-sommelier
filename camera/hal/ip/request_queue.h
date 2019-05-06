/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_IP_REQUEST_QUEUE_H_
#define CAMERA_HAL_IP_REQUEST_QUEUE_H_

#include <deque>
#include <memory>

#include <base/macros.h>
#include <base/synchronization/condition_variable.h>

#include <hardware/camera3.h>

namespace cros {

class CaptureRequest {
 public:
  explicit CaptureRequest(camera3_capture_request_t* request);
  ~CaptureRequest();

  const uint32_t GetFrameNumber() const;
  const camera3_stream_buffer_t* GetOutputBuffer() const;

  void SetErrorBufferStatus();

 private:
  const uint32_t frame_number_;
  buffer_handle_t buffer_handle_;
  camera3_stream_buffer_t output_stream_buffer_;

  DISALLOW_COPY_AND_ASSIGN(CaptureRequest);
};

// This class provides its own locking and is therefore thread-safe. It is
// intended to be used by a single producer and a single consumer.
class RequestQueue {
 public:
  RequestQueue();
  ~RequestQueue();

  // Must be called before using any other functionality of the RequestQueue.
  void SetCallbacks(const camera3_callback_ops_t* callback_ops);

  // Queues a request
  void Push(camera3_capture_request_t* request);

  // If no request is available this will block until one does become available.
  // This can return null if CancelPop is used. This shouldn't be called a
  // second time if the first call has not yet returned.
  std::unique_ptr<CaptureRequest> Pop();

  // Causes any pending call to Pop to return null.
  void CancelPop();

  // Waits until any requests that have already been popped are completed, then
  // cancels any other pending requests.
  void Flush();

  // Returns a popped request back to the queue, this should be called after the
  // request has been filled.
  void NotifyCapture(std::unique_ptr<CaptureRequest> request);

 private:
  void NotifyShutter(uint32_t frame_number, uint64_t timestamp);
  void CancelRequestLocked(std::unique_ptr<CaptureRequest> request);
  void NotifyCaptureInternal(std::unique_ptr<CaptureRequest> request);

  // This lock protects the queue, associated flags/counters, and condition
  // variables.
  base::Lock lock_;
  std::deque<std::unique_ptr<CaptureRequest>> queue_;
  base::ConditionVariable new_request_available_;
  base::ConditionVariable request_filled_;
  int requests_being_filled_;
  bool flushing_;
  bool cancel_next_pop_;

  const camera3_callback_ops_t* callback_ops_;

  DISALLOW_COPY_AND_ASSIGN(RequestQueue);
};

}  // namespace cros

#endif  // CAMERA_HAL_IP_REQUEST_QUEUE_H_
