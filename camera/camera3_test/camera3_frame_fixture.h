// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_
#define CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_

#include <semaphore.h>

#include <list>
#include <unordered_map>

#include <gtest/gtest.h>
#include <hardware/camera3.h>
#include <hardware/hardware.h>

#include "camera3_stream_fixture.h"

namespace camera3_test {

class Camera3FrameFixture : public Camera3StreamFixture {
 public:
  explicit Camera3FrameFixture(int cam_id)
      : Camera3StreamFixture(cam_id),
        request_frame_number_(0),
        result_frame_number_(0) {}

  virtual void SetUp() override;

  virtual void TearDown() override;

 protected:
  // Create and process capture request of given template |type|. The frame
  // number of the created request is returned if |frame_number| is not null.
  int CreateCaptureRequest(int type, uint32_t* frame_number);

  // Wait for capture result with timeout
  void WaitCaptureResult(const struct timespec& timeout);

  // Callback functions from HAL device
  virtual void ProcessCaptureResult(
      const camera3_capture_result* result) override;

  // Callback functions from HAL device
  virtual void Notify(const camera3_notify_msg* msg) override;

  // Validate capture result keys
  void ValidateCaptureResultKeys();

  // Get waiver keys per camera device hardware level and capability
  void GetWaiverKeys(std::set<int32_t>* waiver_keys) const;

  // Whether or not partial result is used
  bool UsePartialResult() const;

  // Validate partial results
  void ValidatePartialResults();

  // Validate the timestamp of the frame of number |frame_number|. The
  // timestamp is returned if |timestamp| is not null.
  void ValidateAndGetTimestamp(uint32_t frame_number, int64_t* timestamp);

 private:
  // Process and handle partial result of one callback. Return True if this is
  // the final partial result.
  bool ProcessPartialResult(const camera3_capture_result& result);

  uint32_t request_frame_number_;

  uint32_t result_frame_number_;

  // Store created capture requests with frame number as the key
  std::unordered_map<uint32_t, camera3_capture_request_t> capture_request_;

  // Store the frame numbers of capture requests that HAL has finished
  // processing
  std::set<uint32_t> completed_request_;

  struct CameraMetadataDeleter {
    inline void operator()(camera_metadata_t* metadata) {
      if (metadata) {
        free_camera_metadata(metadata);
      }
    }
  };

  typedef std::unique_ptr<camera_metadata_t, struct CameraMetadataDeleter>
      CameraMetadataUniquePtr;

  class CaptureResultInfo {
   public:
    CaptureResultInfo()
        : num_output_buffers_(0), have_result_metadata_(false) {}

    // Allocate and copy into partial metadata
    void AllocateAndCopyMetadata(const camera_metadata_t& src);

    // Determine whether or not the key is available
    bool IsMetadataKeyAvailable(int32_t key) const;

    // Find and get key value from partial metadatas
    int32_t GetMetadataKeyValue(int32_t key) const;

    // Find and get key value in int64_t from partial metadatas
    int64_t GetMetadataKeyValue64(int32_t key) const;

    uint32_t num_output_buffers_;

    bool have_result_metadata_;

    std::vector<CameraMetadataUniquePtr> partial_metadata_;

   private:
    bool GetMetadataKeyEntry(int32_t key,
                             camera_metadata_ro_entry_t* entry) const;
  };

  // Store capture result information with frame number as the key
  std::unordered_map<uint32_t, CaptureResultInfo> capture_result_;

  // Store timestamps of shutter callback in the first-in-first-out order
  std::list<uint64_t> capture_timestamps_;

  sem_t shutter_sem_;

  sem_t capture_result_sem_;

  int32_t num_partial_results_;

  static const int32_t capture_result_keys[69];

  DISALLOW_COPY_AND_ASSIGN(Camera3FrameFixture);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_
