/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_CAMERA_CLIENT_H_
#define HAL_USB_CAMERA_CLIENT_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <arc/camera_buffer_mapper.h>
#include <base/bind.h>
#include <base/macros.h>
#include <base/threading/thread.h>
#include <base/threading/thread_checker.h>
#include <hardware/camera3.h>
#include <hardware/hardware.h>

#include "arc/camera_metadata.h"
#include "arc/future.h"
#include "hal/usb/cached_frame.h"
#include "hal/usb/capture_request.h"
#include "hal/usb/common_types.h"
#include "hal/usb/frame_buffer.h"
#include "hal/usb/metadata_handler.h"
#include "hal/usb/v4l2_camera_device.h"

namespace arc {

// CameraClient class is not thread-safe. There are three threads in this
// class.
// 1. Hal thread: Called from hal adapter. Constructor and OpenDevice are called
//    on hal thread.
// 2. Device ops thread: Called from hal adapter. Camera v3 Device Operations
//    (except dump) run on this thread. CloseDevice also runs on this thread.
// 3. Request thread: Owned by this class. Used to handle all requests. The
//    functions in RequestHandler run on request thread.
//
// Android framework synchronizes Constructor, OpenDevice, CloseDevice, and
// device ops. The following items are guaranteed by Android frameworks. Note
// that HAL adapter has more restrictions that all functions of device ops
// (except dump) run on the same thread.
// 1. Open, Initialize, and Close are not concurrent with any of the method in
//    device ops.
// 2. Dump can be called at any time.
// 3. ConfigureStreams is not concurrent with either ProcessCaptureRequest or
//    Flush.
// 4. Flush can be called concurrently with ProcessCaptureRequest.
// 5. ConstructDefaultRequestSettings may be called concurrently to any of the
//    device ops.
class CameraClient {
 public:
  // id is used to distinguish cameras. 0 <= id < number of cameras.
  CameraClient(int id,
               const std::string& device_path,
               const camera_metadata_t& static_info,
               const hw_module_t* module,
               hw_device_t** hw_device);
  ~CameraClient();

  // Camera Device Operations from CameraHal.
  int OpenDevice();
  int CloseDevice();

  int GetId() const { return id_; }

  // Camera v3 Device Operations (see <hardware/camera3.h>)
  int Initialize(const camera3_callback_ops_t* callback_ops);
  int ConfigureStreams(camera3_stream_configuration_t* stream_list);
  // |type| is camera3_request_template_t in camera3.h.
  const camera_metadata_t* ConstructDefaultRequestSettings(int type);
  int ProcessCaptureRequest(camera3_capture_request_t* request);
  void Dump(int fd);
  int Flush(const camera3_device_t* dev);

 private:
  // Verify a set of streams in aggregate.
  bool IsValidStreamSet(const std::vector<camera3_stream_t*>& streams);

  // Calculate usage and maximum number of buffers of each stream.
  void SetUpStreams(int num_buffers, std::vector<camera3_stream_t*>* streams);

  // Start |request_thread_| and streaming.
  int StreamOn(SupportedFormat stream_on_resolution, int* num_buffers);

  // Stop streaming and |request_thread_|.
  void StreamOff();

  // Callback function for RequestHandler::StreamOn.
  void StreamOnCallback(scoped_refptr<internal::Future<int>> future,
                        int* out_num_buffers,
                        int num_buffers,
                        int result);

  // Callback function for RequestHandler::StreamOff.
  void StreamOffCallback(scoped_refptr<internal::Future<int>> future,
                         int result);

  // Camera device id.
  const int id_;

  // Camera device path.
  const std::string device_path_;

  // Delegate to communicate with camera device.
  std::unique_ptr<V4L2CameraDevice> device_;

  // Camera device handle returned to framework for use.
  camera3_device_t camera3_device_;

  // Use to check the constructor, OpenDevice, and CloseDevice are called on the
  // same thread.
  base::ThreadChecker thread_checker_;

  // Use to check camera v3 device operations are called on the same thread.
  base::ThreadChecker ops_thread_checker_;

  // Methods used to call back into the framework.
  const camera3_callback_ops_t* callback_ops_;

  // Handle metadata events and store states.
  std::unique_ptr<MetadataHandler> metadata_handler_;

  // Metadata for latest request.
  CameraMetadata latest_request_metadata_;

  // RequestHandler is used to handle in-flight requests. All functions in the
  // class run on |request_thread_|. The class will be created in StreamOn and
  // destroyed in StreamOff.
  class RequestHandler {
   public:
    RequestHandler(
        const int device_id,
        const std::string& device_path,
        V4L2CameraDevice* device,
        const camera3_callback_ops_t* callback_ops,
        const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
        MetadataHandler* metadata_handler);
    ~RequestHandler();

    // Synchronous call to start streaming.
    void StreamOn(SupportedFormat stream_on_resolution,
                  const base::Callback<void(int, int)>& callback);

    // Synchronous call to stop streaming.
    void StreamOff(const base::Callback<void(int)>& callback);

    // Handle one request.
    void HandleRequest(std::unique_ptr<CaptureRequest> request);

   private:
    // Convert |cache_frame_| to the |buffer| with corresponding format.
    int WriteStreamBuffer(const CameraMetadata& metadata,
                           camera3_stream_buffer_t* buffer);

    // Wait output buffer synced.
    int WaitGrallocBufferSync(camera3_capture_result_t* result);

    // Notify shutter event.
    void NotifyShutter(uint32_t frame_number, int64_t* timestamp);

    // Variables from CameraClient:

    const int device_id_;

    const std::string device_path_;

    // Delegate to communicate with camera device. Caller owns the ownership.
    V4L2CameraDevice* device_;

    // Methods used to call back into the framework.
    const camera3_callback_ops_t* callback_ops_;

    // Task runner for request thread.
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    // Variables only for RequestHandler:

    // The formats used to report to apps.
    SupportedFormats qualified_formats_;

    // Memory mapped buffers which are shared from |device_|.
    std::vector<std::unique_ptr<V4L2FrameBuffer>> input_buffers_;

    // Used to convert to different output formats.
    CachedFrame input_frame_;

    // Handle metadata events and store states. CameraClient takes the
    // ownership.
    MetadataHandler* metadata_handler_;
  };

  std::unique_ptr<RequestHandler> request_handler_;

  // Used to handle requests.
  base::Thread request_thread_;

  // Task runner for request thread.
  scoped_refptr<base::SingleThreadTaskRunner> request_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CameraClient);
};

}  // namespace arc

#endif  // HAL_USB_CAMERA_CLIENT_H_
