// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "camera3_device_fixture.h"

namespace camera3_test {

int32_t Camera3TestGralloc::Initialize() {
  gbm_dev_ = CreateGbmDevice();
  if (!gbm_dev_) {
    LOG(ERROR) << "Can't create gbm device";
    return -ENODEV;
  }

  return 0;
}

void Camera3TestGralloc::Destroy() {
  if (gbm_dev_) {
    close(gbm_device_get_fd(gbm_dev_));
    gbm_device_destroy(gbm_dev_);
  }
}

int32_t Camera3TestGralloc::Allocate(int32_t width,
                                     int32_t height,
                                     int32_t format,
                                     int32_t usage,
                                     buffer_handle_t* handle,
                                     int32_t* stride) {
  uint64_t gbm_usage;
  uint32_t gbm_format;
  struct gbm_bo* bo;

  gbm_format = GrallocConvertFormat(format);
  gbm_usage = GrallocConvertFlags(usage);

  if (!gbm_device_is_format_supported(gbm_dev_, gbm_format, gbm_usage)) {
    return -EINVAL;
  }

  bo = gbm_bo_create(gbm_dev_, width, height, gbm_format, gbm_usage);
  if (!bo) {
    return -ENOBUFS;
  }

  gralloc_drm_handle_t* hnd = new gralloc_drm_handle_t();

  hnd->base.version = sizeof(hnd->base);
  hnd->base.numInts = GRALLOC_DRM_HANDLE_NUM_INTS;
  hnd->base.numFds = GRALLOC_DRM_HANDLE_NUM_FDS;

  hnd->prime_fd = gbm_bo_get_fd(bo);
  hnd->width = gbm_bo_get_width(bo);
  hnd->height = gbm_bo_get_height(bo);
  hnd->stride = gbm_bo_get_stride(bo);
  hnd->format = gbm_bo_get_format(bo);
  hnd->usage = usage;
  hnd->data_owner = reinterpret_cast<int32_t>(gbm_bo_get_user_data(bo));
  hnd->data = reinterpret_cast<intptr_t>(bo);

  *handle = reinterpret_cast<buffer_handle_t>(hnd);
  *stride = hnd->stride;

  return 0;
}

int32_t Camera3TestGralloc::Free(buffer_handle_t handle) {
  gralloc_drm_handle_t* hnd = gralloc_drm_handle(handle);
  if (hnd) {
    EXPECT_TRUE(hnd->data != 0) << "Buffer handle mapping fails";
    if (testing::Test::HasFailure()) {
      return -EINVAL;
    }

    gbm_bo_destroy(reinterpret_cast<struct gbm_bo*>(hnd->data));
    delete hnd;
    return 0;
  }

  return -EINVAL;
}

struct gbm_device* Camera3TestGralloc::CreateGbmDevice() {
  int32_t fd;
  int32_t num_nodes = 63;
  int32_t min_node = 128;
  int32_t max_node = min_node + num_nodes;
  struct gbm_device* gbm = nullptr;

  // Try to get hardware-backed device first
  for (int32_t i = min_node; i < max_node; i++) {
    fd = drmOpenRender(i);
    if (fd < 0) {
      continue;
    }

    drmVersionPtr version = drmGetVersion(fd);
    if (!strcmp("vgem", version->name)) {
      drmFreeVersion(version);
      close(fd);
      continue;
    }

    gbm = gbm_create_device(fd);
    if (!gbm) {
      drmFreeVersion(version);
      close(fd);
      continue;
    }

    drmFreeVersion(version);
    return gbm;
  }

  // Fall back to vgem if not hardware-backed device is found
  for (int32_t i = min_node; i < max_node; i++) {
    fd = drmOpenRender(i);
    if (fd < 0) {
      continue;
    }

    drmVersionPtr version = drmGetVersion(fd);
    if (strcmp("vgem", drmGetVersion(fd)->name)) {
      drmFreeVersion(version);
      close(fd);
      continue;
    }

    gbm = gbm_create_device(fd);
    if (!gbm) {
      close(fd);
      return nullptr;
    }

    drmFreeVersion(version);
    return gbm;
  }

  return nullptr;
}

uint64_t Camera3TestGralloc::GrallocConvertFlags(int32_t flags) {
  uint64_t usage = 0;

  // TODO: conversion

  return usage;
}

uint32_t Camera3TestGralloc::GrallocConvertFormat(int32_t format) {
  switch (format) {
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return GBM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      return GBM_FORMAT_MOD_NONE;  // TODO
    case HAL_PIXEL_FORMAT_RGB_565:
      return GBM_FORMAT_RGB565;
    case HAL_PIXEL_FORMAT_RGB_888:
      return GBM_FORMAT_RGB888;
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return GBM_FORMAT_ABGR8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return GBM_FORMAT_XBGR8888;
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
      return GBM_FORMAT_YUV420;
    case HAL_PIXEL_FORMAT_YV12:
      return GBM_FORMAT_YVU420;
  }

  return GBM_FORMAT_MOD_NONE;  // TODO
}

int Camera3Device::Initialize(const Camera3Module* cam_module,
                              const camera3_callback_ops_t* callback_ops) {
  if (!cam_module) {
    return -EINVAL;
  }

  // Open camera device
  cam_device_ = cam_module->OpenDevice(cam_id_);
  if (!cam_device_) {
    return -ENODEV;
  }

  EXPECT_GE(((const hw_device_t*)cam_device_)->version,
            (uint16_t)HARDWARE_MODULE_API_VERSION(3, 0))
      << "The device does not support HAL3";

  // Initialize camera device
  EXPECT_EQ(0, cam_device_->ops->initialize(cam_device_, callback_ops))
      << "Camera device initialization fails";

  EXPECT_EQ(0, gralloc_.Initialize()) << "Gralloc initialization fails";

  if (testing::Test::HasFailure()) {
    return -EINVAL;
  }

  initialized_ = true;
  return 0;
}

void Camera3Device::Destroy() {
  // Buffers are expected to be freed in ProcessCaptureResult callback in the
  // test
  EXPECT_TRUE(stream_buffers_.empty()) << "Buffers are not freed correctly";
  // Free frame buffers
  for (auto const& it : stream_buffers_) {
    for (auto const& buffer : it.second) {
      EXPECT_EQ(0, gralloc_.Free(*buffer)) << "Buffer is not freed correctly";
      delete buffer;
    }
  }

  gralloc_.Destroy();

  EXPECT_EQ(0, cam_device_->common.close(&cam_device_->common))
      << "Camera device close failed";
}

const camera_metadata_t* Camera3Device::ConstructDefaultRequestSettings(
    int type) {
  if (!initialized_) {
    return NULL;
  }

  return cam_device_->ops->construct_default_request_settings(cam_device_,
                                                              type);
}

void Camera3Device::AddOutputStream(int format, int width, int height) {
  camera3_stream_t stream = {};
  stream.stream_type = CAMERA3_STREAM_OUTPUT;
  stream.width = width;
  stream.height = height;
  stream.format = format;

  // Push to the bin that is not used currently
  {
    base::AutoLock l(stream_lock_);
    cam_stream_[!cam_stream_idx_].push_back(stream);
  }
}

int Camera3Device::ConfigureStreams() {
  base::AutoLock l(stream_lock_);

  // Check whether there are streams
  if (cam_stream_[!cam_stream_idx_].size() == 0) {
    return -EINVAL;
  }

  // Prepare stream configuration
  std::vector<camera3_stream_t*> cam_streams;
  camera3_stream_configuration_t cam_stream_config;
  for (size_t i = 0; i < cam_stream_[!cam_stream_idx_].size(); i++) {
    cam_streams.push_back(&cam_stream_[!cam_stream_idx_][i]);
  }
  cam_stream_config.num_streams = cam_streams.size();
  cam_stream_config.streams = cam_streams.data();
  cam_stream_config.operation_mode = CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE;

  // Configure streams now
  int ret =
      cam_device_->ops->configure_streams(cam_device_, &cam_stream_config);

  // Swap to the other bin
  cam_stream_[cam_stream_idx_].clear();
  cam_stream_idx_ = !cam_stream_idx_;

  return ret;
}

int Camera3Device::AllocateOutputStreamBuffers(
    std::vector<camera3_stream_buffer_t>* output_buffers) {
  base::AutoLock l1(stream_lock_);

  if (cam_stream_[cam_stream_idx_].size() == 0 || !output_buffers) {
    return -EINVAL;
  }

  for (const auto& it : cam_stream_[cam_stream_idx_]) {
    int32_t format = it.format;
    uint32_t width = it.width;
    uint32_t height = it.height;

    // Allocate general-purpose buffer if it is implementation defined or blob
    // format
    if ((format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) ||
        (format == HAL_PIXEL_FORMAT_BLOB)) {
      format = HAL_PIXEL_FORMAT_RGBA_8888;
      width = width * height;
      height = 1;
    }

    buffer_handle_t* buffer = new buffer_handle_t();
    int32_t stride;

    EXPECT_EQ(0, gralloc_.Allocate(width, height, format,
                                   GRALLOC_USAGE_SW_WRITE_OFTEN |
                                       GRALLOC_USAGE_HW_CAMERA_WRITE,
                                   buffer, &stride))
        << "Gralloc allocation fails";

    camera3_stream_buffer_t stream_buffer;
    {
      base::AutoLock l2(stream_buffers_lock_);
      camera3_stream_t* stream = const_cast<camera3_stream_t*>(&it);
      stream_buffers_[stream].insert(buffer);

      stream_buffer = {.stream = stream,
                       .buffer = buffer,
                       .status = CAMERA3_BUFFER_STATUS_OK,
                       .acquire_fence = -1,
                       .release_fence = -1};
    }
    output_buffers->push_back(stream_buffer);
  }

  return 0;
}

int Camera3Device::FreeOutputStreamBuffers(
    const std::vector<camera3_stream_buffer_t>& output_buffers) {
  base::AutoLock l(stream_buffers_lock_);

  for (const auto& stream_buffer : output_buffers) {
    if ((stream_buffers_.find(stream_buffer.stream) == stream_buffers_.end()) ||
        (stream_buffers_[stream_buffer.stream].find(stream_buffer.buffer) ==
         stream_buffers_[stream_buffer.stream].end())) {
      return -EINVAL;
    }
    int result = gralloc_.Free(*stream_buffer.buffer);
    if (result != 0) {
      return result;
    }
    stream_buffers_[stream_buffer.stream].erase(stream_buffer.buffer);
    if (stream_buffers_[stream_buffer.stream].empty()) {
      stream_buffers_.erase(stream_buffer.stream);
    }
    delete stream_buffer.buffer;
  }
  return 0;
}

int Camera3Device::ProcessCaptureRequest(camera3_capture_request_t* request) {
  if (!initialized_) {
    return -ENODEV;
  }

  return cam_device_->ops->process_capture_request(cam_device_, request);
}

// Test fixture

void Camera3DeviceFixture::SetUp() {
  ASSERT_EQ(0, cam_module_.Initialize())
      << "Camera module initialization fails";

  // Prepare callback functions for camera device
  Camera3DeviceFixture::notify = Camera3DeviceFixture::NotifyCallback;
  Camera3DeviceFixture::process_capture_result =
      Camera3DeviceFixture::ProcessCaptureResultCallback;

  ASSERT_EQ(0, cam_device_.Initialize(&cam_module_, this))
      << "Camera device initialization fails";
}

void Camera3DeviceFixture::TearDown() {
  cam_device_.Destroy();
}

void Camera3DeviceFixture::ProcessCaptureResult(
    const camera3_capture_result* result) {
  // Do nothing in this callback
}

void Camera3DeviceFixture::Notify(const camera3_notify_msg* msg) {
  // Do nothing in this callback
}

void Camera3DeviceFixture::ProcessCaptureResultCallback(
    const camera3_callback_ops* cb,
    const camera3_capture_result* result) {
  // Forward to callback of instance
  Camera3DeviceFixture* d = const_cast<Camera3DeviceFixture*>(
      static_cast<const Camera3DeviceFixture*>(cb));
  d->ProcessCaptureResult(result);
}

void Camera3DeviceFixture::NotifyCallback(const camera3_callback_ops* cb,
                                          const camera3_notify_msg* msg) {
  // Forward to callback of instance
  Camera3DeviceFixture* d = const_cast<Camera3DeviceFixture*>(
      static_cast<const Camera3DeviceFixture*>(cb));
  d->Notify(msg);
}

// Test cases

// Test spec:
// - Camera ID
// - Capture type
class Camera3DeviceDefaultSettings
    : public Camera3DeviceFixture,
      public ::testing::WithParamInterface<std::tuple<int, int>> {
 public:
  Camera3DeviceDefaultSettings()
      : Camera3DeviceFixture(std::get<0>(GetParam())) {}
};

TEST_P(Camera3DeviceDefaultSettings, ConstructDefaultSettings) {
  int type = std::get<1>(GetParam());

  ASSERT_TRUE(NULL != cam_device_.ConstructDefaultRequestSettings(type))
      << "Camera default settings are NULL";
}

INSTANTIATE_TEST_CASE_P(
    ConstructDefaultSettings,
    Camera3DeviceDefaultSettings,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Values(CAMERA3_TEMPLATE_PREVIEW,
                                         CAMERA3_TEMPLATE_STILL_CAPTURE)));

}  // namespace camera3_test
