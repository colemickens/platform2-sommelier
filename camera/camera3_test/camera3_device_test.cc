// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "arc/camera_buffer_mapper.h"
#include "camera3_device_fixture.h"
#include "common/camera_buffer_handle.h"

namespace camera3_test {

int32_t Camera3TestGralloc::Initialize() {
  gbm_dev_ = CreateGbmDevice();
  if (!gbm_dev_) {
    LOG(ERROR) << "Can't create gbm device";
    return -ENODEV;
  }

  uint32_t formats[] = {GBM_FORMAT_YVU420, GBM_FORMAT_NV12, GBM_FORMAT_YUV422,
                        GBM_FORMAT_NV21};
  size_t i = 0;
  for (; i < arraysize(formats); i++) {
    if (gbm_device_is_format_supported(gbm_dev_, formats[i],
                                       GBM_BO_USE_RENDERING)) {
      flexible_yuv_420_format_ = formats[i];
      break;
    }
  }
  if (i == arraysize(formats)) {
    LOG(ERROR) << "Can't detect flexible YUV 420 format";
    return -EINVAL;
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
                                     buffer_handle_t* handle) {
  uint64_t gbm_usage;
  uint32_t gbm_format;
  struct gbm_bo* bo;

  gbm_format = GrallocConvertFormat(format);
  gbm_usage = GrallocConvertFlags(usage);

  if (!gbm_device_is_format_supported(gbm_dev_, gbm_format, gbm_usage)) {
    LOG(ERROR) << "Unsupported format " << gbm_format;
    return -EINVAL;
  }

  bo = gbm_bo_create(gbm_dev_, width, height, gbm_format, gbm_usage);
  if (!bo) {
    LOG(ERROR) << "Failed to create bo (" << width << "x" << height << ")";
    return -ENOBUFS;
  }

  camera_buffer_handle_t* hnd = new camera_buffer_handle_t();

  hnd->base.version = sizeof(hnd->base);
  hnd->base.numInts = kCameraBufferHandleNumInts;
  hnd->base.numFds = kCameraBufferHandleNumFds;

  hnd->magic = kCameraBufferMagic;
  hnd->buffer_id = reinterpret_cast<uint64_t>(bo);
  hnd->type = arc::GRALLOC;
  hnd->drm_format = gbm_bo_get_format(bo);
  hnd->hal_pixel_format = format;
  hnd->width = gbm_bo_get_width(bo);
  hnd->height = gbm_bo_get_height(bo);
  for (size_t i = 0; i < gbm_bo_get_num_planes(bo); ++i) {
    hnd->fds[i].reset(gbm_bo_get_plane_fd(bo, i));
    hnd->strides[i] = gbm_bo_get_plane_stride(bo, i);
    hnd->offsets[i] = gbm_bo_get_plane_offset(bo, i);
  }

  *handle = reinterpret_cast<buffer_handle_t>(hnd);

  return 0;
}

int32_t Camera3TestGralloc::Free(buffer_handle_t handle) {
  auto hnd = camera_buffer_handle_t::FromBufferHandle(handle);
  if (hnd) {
    if (hnd->buffer_id == 0) {
      ADD_FAILURE() << "Buffer handle mapping fails";
      delete hnd;
      return -EINVAL;
    }

    gbm_bo_destroy(reinterpret_cast<struct gbm_bo*>(hnd->buffer_id));
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
  uint64_t usage = GBM_BO_USE_RENDERING;

  // TODO: conversion from Gralloc flags to GBM flags

  return usage;
}

uint32_t Camera3TestGralloc::GrallocConvertFormat(int32_t format) {
  switch (format) {
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return GBM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      return GBM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_RGB_565:
      return GBM_FORMAT_RGB565;
    case HAL_PIXEL_FORMAT_RGB_888:
      return GBM_FORMAT_RGB888;
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return GBM_FORMAT_ABGR8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return GBM_FORMAT_XBGR8888;
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
      return flexible_yuv_420_format_;
    case HAL_PIXEL_FORMAT_YV12:
      return GBM_FORMAT_YVU420;
    case HAL_PIXEL_FORMAT_BLOB:
      return GBM_FORMAT_ARGB8888;
    default:
      LOG(ERROR) << "Unsupported format conversion " << format;
  }

  return GBM_FORMAT_ARGB8888;
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
            (uint16_t)HARDWARE_MODULE_API_VERSION(3, 3))
      << "The device must support at least HALv3.3";

  // Initialize camera device
  EXPECT_EQ(0, cam_device_->ops->initialize(cam_device_, callback_ops))
      << "Camera device initialization fails";

  EXPECT_EQ(0, gralloc_.Initialize()) << "Gralloc initialization fails";

  camera_info cam_info;
  EXPECT_EQ(0, cam_module->GetCameraInfo(cam_id_, &cam_info));
  static_info_.reset(new StaticInfo(cam_info));
  EXPECT_TRUE(static_info_->IsHardwareLevelAtLeastLimited())
      << "The device must support at least LIMITED hardware level";

  if (testing::Test::HasFailure()) {
    return -EINVAL;
  }

  initialized_ = true;
  return 0;
}

void Camera3Device::Destroy() {
  // Buffers are expected to be freed in ProcessCaptureResult callback in the
  // test.
  EXPECT_TRUE(stream_buffers_.empty()) << "Buffers are not freed correctly";
  ClearOutputStreamBuffers();

  gralloc_.Destroy();

  EXPECT_EQ(0, cam_device_->common.close(&cam_device_->common))
      << "Camera device close failed";
}

bool Camera3Device::IsTemplateSupported(int32_t type) const {
  return (type != CAMERA3_TEMPLATE_MANUAL ||
          static_info_->IsCapabilitySupported(
              ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR)) &&
         (type != CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG ||
          static_info_->IsCapabilitySupported(
              ANDROID_REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING));
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
  std::vector<camera3_stream_t> streams;
  return AllocateOutputStreamBuffersByStreams(streams, output_buffers);
}

int Camera3Device::AllocateOutputStreamBuffersByStreams(
    const std::vector<camera3_stream_t>& streams,
    std::vector<camera3_stream_buffer_t>* output_buffers) {
  base::AutoLock l1(stream_lock_);

  int32_t jpeg_max_size = static_info_->GetJpegMaxSize();
  if (!output_buffers ||
      (streams.empty() && cam_stream_[cam_stream_idx_].size() == 0) ||
      jpeg_max_size <= 0) {
    return -EINVAL;
  }

  const std::vector<camera3_stream_t>* streams_ptr =
      streams.empty() ? &cam_stream_[cam_stream_idx_] : &streams;
  for (const auto& it : *streams_ptr) {
    std::unique_ptr<buffer_handle_t> buffer(new buffer_handle_t);

    // Buffers of blob format must have a height of 1, and width equal to
    // their size in bytes.
    EXPECT_EQ(
        0, gralloc_.Allocate(
               (it.format == HAL_PIXEL_FORMAT_BLOB) ? jpeg_max_size : it.width,
               (it.format == HAL_PIXEL_FORMAT_BLOB) ? 1 : it.height, it.format,
               GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_CAMERA_WRITE,
               buffer.get()))
        << "Gralloc allocation fails";

    camera3_stream_buffer_t stream_buffer;
    {
      base::AutoLock l2(stream_buffers_lock_);
      camera3_stream_t* stream = const_cast<camera3_stream_t*>(&it);

      stream_buffer = {.stream = stream,
                       .buffer = buffer.get(),
                       .status = CAMERA3_BUFFER_STATUS_OK,
                       .acquire_fence = -1,
                       .release_fence = -1};
      stream_buffers_[stream].insert(std::move(buffer));
    }
    output_buffers->push_back(stream_buffer);
  }

  return 0;
}

int Camera3Device::FreeOutputStreamBuffers(
    const std::vector<camera3_stream_buffer_t>& output_buffers) {
  base::AutoLock l(stream_buffers_lock_);

  for (const auto& output_buffer : output_buffers) {
    if (stream_buffers_.find(output_buffer.stream) == stream_buffers_.end() ||
        !output_buffer.buffer) {
      LOG(ERROR) << "Failed to find configured stream or buffer is invalid";
      return -EINVAL;
    }

    bool found = false;
    for (const auto& it : stream_buffers_[output_buffer.stream]) {
      if (*it == *output_buffer.buffer) {
        int result = gralloc_.Free(*it);
        if (result != 0) {
          LOG(ERROR) << "Failed to free output buffer";
          return result;
        }
        stream_buffers_[output_buffer.stream].erase(it);
        if (stream_buffers_[output_buffer.stream].empty()) {
          stream_buffers_.erase(output_buffer.stream);
        }
        found = true;
        break;
      }
    }
    if (!found) {
      LOG(ERROR) << "Failed to find output buffer";
      return -EINVAL;
    }
  }
  return 0;
}

void Camera3Device::ClearOutputStreamBuffers() {
  base::AutoLock l(stream_buffers_lock_);

  // Free frame buffers
  for (auto const& it : stream_buffers_) {
    for (auto const& buffer : it.second) {
      EXPECT_EQ(0, gralloc_.Free(*buffer)) << "Buffer is not freed correctly";
    }
  }
  stream_buffers_.clear();
}

int Camera3Device::ProcessCaptureRequest(camera3_capture_request_t* request) {
  if (!initialized_) {
    return -ENODEV;
  }

  return cam_device_->ops->process_capture_request(cam_device_, request);
}

int Camera3Device::Flush() {
  if (!initialized_) {
    return -ENODEV;
  }

  return cam_device_->ops->flush(cam_device_);
}

const Camera3Device::StaticInfo* Camera3Device::GetStaticInfo() const {
  return static_info_.get();
}

Camera3Device::StaticInfo::StaticInfo(const camera_info& cam_info)
    : characteristics_(const_cast<camera_metadata_t*>(
          cam_info.static_camera_characteristics)) {}

bool Camera3Device::StaticInfo::IsKeyAvailable(uint32_t tag) const {
  return AreKeysAvailable(std::vector<uint32_t>(1, tag));
}

bool Camera3Device::StaticInfo::AreKeysAvailable(
    std::vector<uint32_t> tags) const {
  for (const auto& tag : tags) {
    camera_metadata_ro_entry_t entry;
    if (find_camera_metadata_ro_entry(characteristics_, tag, &entry)) {
      return false;
    }
  }
  return true;
}

int32_t Camera3Device::StaticInfo::GetHardwareLevel() const {
  camera_metadata_ro_entry_t entry;
  if (find_camera_metadata_ro_entry(characteristics_,
                                    ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL,
                                    &entry) != 0) {
    ADD_FAILURE()
        << "Cannot find the metadata ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL";
    return -EINVAL;
  }
  return entry.data.i32[0];
}

bool Camera3Device::StaticInfo::IsHardwareLevelAtLeast(int32_t level) const {
  int32_t dev_level = GetHardwareLevel();
  if (dev_level == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY) {
    return dev_level == level;
  }
  // Level is not LEGACY, can use numerical sort
  return dev_level >= level;
}

bool Camera3Device::StaticInfo::IsHardwareLevelAtLeastFull() const {
  return IsHardwareLevelAtLeast(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL);
}

bool Camera3Device::StaticInfo::IsHardwareLevelAtLeastLimited() const {
  return IsHardwareLevelAtLeast(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED);
}

bool Camera3Device::StaticInfo::IsCapabilitySupported(
    int32_t capability) const {
  EXPECT_GE(capability, 0) << "Capability must be non-negative";

  camera_metadata_ro_entry_t entry;
  if (find_camera_metadata_ro_entry(characteristics_,
                                    ANDROID_REQUEST_AVAILABLE_CAPABILITIES,
                                    &entry) == 0) {
    for (size_t i = 0; i < entry.count; i++) {
      if (entry.data.i32[i] == capability) {
        return true;
      }
    }
  }
  return false;
}

bool Camera3Device::StaticInfo::IsDepthOutputSupported() const {
  return IsCapabilitySupported(
      ANDROID_REQUEST_AVAILABLE_CAPABILITIES_DEPTH_OUTPUT);
}

bool Camera3Device::StaticInfo::IsColorOutputSupported() const {
  return IsCapabilitySupported(
      ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE);
}

std::set<uint8_t> Camera3Device::StaticInfo::GetAvailableModes(
    int32_t key,
    int32_t min_value,
    int32_t max_value) const {
  camera_metadata_ro_entry_t entry;
  if (find_camera_metadata_ro_entry(characteristics_, key, &entry) != 0) {
    ADD_FAILURE() << "Cannot find the metadata "
                  << get_camera_metadata_tag_name(key);
    return std::set<uint8_t>();
  }
  std::set<uint8_t> modes;
  for (size_t i = 0; i < entry.count; i++) {
    uint8_t mode = entry.data.u8[i];
    // Each element must be distinct
    EXPECT_TRUE(modes.find(mode) == modes.end())
        << "Duplicate modes " << mode << " for the metadata "
        << get_camera_metadata_tag_name(key);
    EXPECT_TRUE(mode >= min_value && mode <= max_value)
        << "Mode " << mode << " is outside of [" << min_value << ","
        << max_value << "] for the metadata "
        << get_camera_metadata_tag_name(key);
    modes.insert(mode);
  }
  return modes;
}

std::set<uint8_t> Camera3Device::StaticInfo::GetAvailableEdgeModes() const {
  std::set<uint8_t> modes = GetAvailableModes(
      ANDROID_EDGE_AVAILABLE_EDGE_MODES, ANDROID_EDGE_MODE_OFF,
      ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG);

  // Full device should always include OFF and FAST
  if (IsHardwareLevelAtLeastFull()) {
    EXPECT_TRUE((modes.find(ANDROID_EDGE_MODE_OFF) != modes.end()) &&
                (modes.find(ANDROID_EDGE_MODE_FAST) != modes.end()))
        << "Full device must contain OFF and FAST edge modes";
  }

  // FAST and HIGH_QUALITY mode must be both present or both not present
  EXPECT_TRUE((modes.find(ANDROID_EDGE_MODE_FAST) != modes.end()) ==
              (modes.find(ANDROID_EDGE_MODE_HIGH_QUALITY) != modes.end()))
      << "FAST and HIGH_QUALITY mode must both present or both not present";

  return modes;
}

std::set<uint8_t> Camera3Device::StaticInfo::GetAvailableNoiseReductionModes()
    const {
  std::set<uint8_t> modes =
      GetAvailableModes(ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES,
                        ANDROID_NOISE_REDUCTION_MODE_OFF,
                        ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG);

  // Full device should always include OFF and FAST
  if (IsHardwareLevelAtLeastFull()) {
    EXPECT_TRUE((modes.find(ANDROID_NOISE_REDUCTION_MODE_OFF) != modes.end()) &&
                (modes.find(ANDROID_NOISE_REDUCTION_MODE_FAST) != modes.end()))
        << "Full device must contain OFF and FAST noise reduction modes";
  }

  // FAST and HIGH_QUALITY mode must be both present or both not present
  EXPECT_TRUE(
      (modes.find(ANDROID_NOISE_REDUCTION_MODE_FAST) != modes.end()) ==
      (modes.find(ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY) != modes.end()))
      << "FAST and HIGH_QUALITY mode must both present or both not present";

  return modes;
}

std::set<uint8_t> Camera3Device::StaticInfo::GetAvailableColorAberrationModes()
    const {
  std::set<uint8_t> modes =
      GetAvailableModes(ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES,
                        ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF,
                        ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY);

  EXPECT_TRUE((modes.find(ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF) !=
               modes.end()) ||
              (modes.find(ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST) !=
               modes.end()))
      << "Camera devices must always support either OFF or FAST mode";

  // FAST and HIGH_QUALITY mode must be both present or both not present
  EXPECT_TRUE(
      (modes.find(ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST) !=
       modes.end()) ==
      (modes.find(ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY) !=
       modes.end()))
      << "FAST and HIGH_QUALITY mode must both present or both not present";

  return modes;
}

std::set<uint8_t> Camera3Device::StaticInfo::GetAvailableToneMapModes() const {
  std::set<uint8_t> modes = GetAvailableModes(
      ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES,
      ANDROID_TONEMAP_MODE_CONTRAST_CURVE, ANDROID_TONEMAP_MODE_PRESET_CURVE);

  EXPECT_TRUE(modes.find(ANDROID_TONEMAP_MODE_FAST) != modes.end())
      << "Camera devices must always support FAST mode";

  // FAST and HIGH_QUALITY mode must be both present
  EXPECT_TRUE(modes.find(ANDROID_TONEMAP_MODE_HIGH_QUALITY) != modes.end())
      << "FAST and HIGH_QUALITY mode must both present";

  return modes;
}

std::set<int32_t> Camera3Device::StaticInfo::GetAvailableFormats(
    int32_t direction) const {
  camera_metadata_ro_entry_t available_config;
  EXPECT_EQ(
      0, find_camera_metadata_ro_entry(
             characteristics_, ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
             &available_config));
  EXPECT_NE(0u, available_config.count)
      << "Camera stream configuration is empty";
  EXPECT_EQ(0u, available_config.count % kNumOfElementsInStreamConfigEntry)
      << "Camera stream configuration parsing error";

  if (testing::Test::HasFailure()) {
    return std::set<int32_t>();
  }
  std::set<int32_t> formats;
  for (size_t i = 0; i < available_config.count;
       i += kNumOfElementsInStreamConfigEntry) {
    int32_t format = available_config.data.i32[i];
    int32_t in_or_out = available_config.data.i32[i + 3];
    if (in_or_out == direction) {
      formats.insert(format);
    }
  }
  return formats;
}

bool Camera3Device::StaticInfo::IsAELockSupported() const {
  camera_metadata_ro_entry_t entry;
  if (find_camera_metadata_ro_entry(
          characteristics_, ANDROID_CONTROL_AE_LOCK_AVAILABLE, &entry) != 0) {
    ADD_FAILURE()
        << "Cannot find the metadata ANDROID_CONTROL_AE_LOCK_AVAILABLE";
    return false;
  }
  return entry.data.i32[0] == ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
}

bool Camera3Device::StaticInfo::IsAWBLockSupported() const {
  camera_metadata_ro_entry_t entry;
  if (find_camera_metadata_ro_entry(
          characteristics_, ANDROID_CONTROL_AWB_LOCK_AVAILABLE, &entry) != 0) {
    ADD_FAILURE()
        << "Cannot find the metadata ANDROID_CONTROL_AWB_LOCK_AVAILABLE";
    return false;
  }
  return entry.data.i32[0] == ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
}

int32_t Camera3Device::StaticInfo::GetPartialResultCount() const {
  camera_metadata_ro_entry_t entry;
  if (find_camera_metadata_ro_entry(characteristics_,
                                    ANDROID_REQUEST_PARTIAL_RESULT_COUNT,
                                    &entry) != 0) {
    // Optional key. Default value is 1 if key is missing.
    return 1;
  }
  return entry.data.i32[0];
}

int32_t Camera3Device::StaticInfo::GetJpegMaxSize() const {
  camera_metadata_ro_entry_t entry;
  if (find_camera_metadata_ro_entry(characteristics_, ANDROID_JPEG_MAX_SIZE,
                                    &entry) != 0) {
    ADD_FAILURE() << "Cannot find the metadata ANDROID_JPEG_MAX_SIZE";
    return -EINVAL;
  }
  return entry.data.i32[0];
}

int32_t Camera3Device::StaticInfo::GetSensorOrientation() const {
  camera_metadata_ro_entry_t entry;
  if (find_camera_metadata_ro_entry(characteristics_,
                                    ANDROID_SENSOR_ORIENTATION, &entry) != 0) {
    ADD_FAILURE() << "Cannot find the metadata ANDROID_SENSOR_ORIENTATION";
    return -EINVAL;
  }
  return entry.data.i32[0];
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
class Camera3DeviceSimpleTest : public Camera3DeviceFixture,
                                public ::testing::WithParamInterface<int> {
 public:
  Camera3DeviceSimpleTest() : Camera3DeviceFixture(GetParam()) {}
};

TEST_P(Camera3DeviceSimpleTest, SensorOrientationTest) {
  // Chromebook has a hardware requirement that the top of the camera should
  // match the top of the display in tablet mode.
  ASSERT_EQ(0, cam_device_.GetStaticInfo()->GetSensorOrientation())
      << "Invalid camera sensor orientation";
}

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

static bool IsMetadataKeyAvailable(const camera_metadata_t* settings,
                                   int32_t key) {
  camera_metadata_ro_entry_t entry;
  return find_camera_metadata_ro_entry(settings, key, &entry) == 0;
}

static void ExpectKeyValue(const camera_metadata_t* settings,
                           int32_t key,
                           const char* key_name,
                           int32_t value,
                           int32_t compare_type) {
  camera_metadata_ro_entry_t entry;
  ASSERT_EQ(0, find_camera_metadata_ro_entry(settings, key, &entry))
      << "Cannot find the metadata " << key_name;
  if (compare_type == 0) {
    ASSERT_EQ(value, entry.data.i32[0]) << "Wrong value of metadata "
                                        << key_name;
  } else {
    ASSERT_NE(value, entry.data.i32[0]) << "Wrong value of metadata "
                                        << key_name;
  }
}
#define EXPECT_KEY_VALUE_EQ(settings, key, value) \
  ExpectKeyValue(settings, key, #key, value, 0)
#define EXPECT_KEY_VALUE_NE(settings, key, value) \
  ExpectKeyValue(settings, key, #key, value, 1)

static void ExpectKeyValueNotEqualsI64(const camera_metadata_t* settings,
                                       int32_t key,
                                       const char* key_name,
                                       int64_t value) {
  camera_metadata_ro_entry_t entry;
  ASSERT_EQ(0, find_camera_metadata_ro_entry(settings, key, &entry))
      << "Cannot find the metadata " << key_name;
  ASSERT_NE(value, entry.data.i64[0]) << "Wrong value of metadata " << key_name;
}
#define EXPECT_KEY_VALUE_NE_I64(settings, key, value) \
  ExpectKeyValueNotEqualsI64(settings, key, #key, value)

TEST_P(Camera3DeviceDefaultSettings, ConstructDefaultSettings) {
  int type = std::get<1>(GetParam());

  const camera_metadata_t* default_settings;
  default_settings = cam_device_.ConstructDefaultRequestSettings(type);
  ASSERT_NE(nullptr, default_settings) << "Camera default settings are NULL";

  auto static_info = cam_device_.GetStaticInfo();

  // Reference: camera2/cts/CameraDeviceTest.java#captureTemplateTestByCamera
  if (!cam_device_.IsTemplateSupported(type)) {
    return;
  } else if (type != CAMERA3_TEMPLATE_PREVIEW &&
             static_info->IsDepthOutputSupported() &&
             !static_info->IsColorOutputSupported()) {
    // Depth-only devices need only support PREVIEW template
    return;
  }

  // Reference: camera2/cts/CameraDeviceTest.java#checkRequestForTemplate
  // 3A settings--control mode
  EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_CONTROL_AE_MODE,
                      ANDROID_CONTROL_AE_MODE_ON);

  EXPECT_KEY_VALUE_EQ(default_settings,
                      ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, 0);

  EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
                      ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE);

  // if AE lock is not supported, expect the control key to be non-existent or
  // false
  if (static_info->IsAELockSupported() ||
      IsMetadataKeyAvailable(default_settings, ANDROID_CONTROL_AE_LOCK)) {
    EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_CONTROL_AE_LOCK,
                        ANDROID_CONTROL_AE_LOCK_OFF);
  }

  EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_CONTROL_AF_TRIGGER,
                      ANDROID_CONTROL_AF_TRIGGER_IDLE);

  EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_CONTROL_AWB_MODE,
                      ANDROID_CONTROL_AWB_MODE_AUTO);

  // if AWB lock is not supported, expect the control key to be non-existent or
  // false
  if (static_info->IsAWBLockSupported() ||
      IsMetadataKeyAvailable(default_settings, ANDROID_CONTROL_AWB_LOCK)) {
    EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_CONTROL_AWB_LOCK,
                        ANDROID_CONTROL_AWB_LOCK_OFF);
  }

  // Check 3A regions
  // TODO: CONTROL_AE_REGIONS, CONTROL_AWB_REGIONS, CONTROL_AF_REGIONS?

  // Sensor settings
  // TODO: LENS_APERTURE, LENS_FILTER_DENSITY, LENS_FOCAL_LENGTH,
  //       LENS_OPTICAL_STABILIZATION_MODE?
  //       BLACK_LEVEL_LOCK?

  if (static_info->IsKeyAvailable(ANDROID_BLACK_LEVEL_LOCK)) {
    EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_BLACK_LEVEL_LOCK,
                        ANDROID_BLACK_LEVEL_LOCK_OFF);
  }

  if (static_info->IsKeyAvailable(ANDROID_SENSOR_FRAME_DURATION)) {
    EXPECT_KEY_VALUE_NE_I64(default_settings, ANDROID_SENSOR_FRAME_DURATION, 0);
  }

  if (static_info->IsKeyAvailable(ANDROID_SENSOR_EXPOSURE_TIME)) {
    EXPECT_KEY_VALUE_NE_I64(default_settings, ANDROID_SENSOR_EXPOSURE_TIME, 0);
  }

  if (static_info->IsKeyAvailable(ANDROID_SENSOR_SENSITIVITY)) {
    EXPECT_KEY_VALUE_NE(default_settings, ANDROID_SENSOR_SENSITIVITY, 0);
  }

  // ISP-processing settings
  EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_STATISTICS_FACE_DETECT_MODE,
                      ANDROID_STATISTICS_FACE_DETECT_MODE_OFF);

  EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_FLASH_MODE,
                      ANDROID_FLASH_MODE_OFF);

  if (static_info->IsKeyAvailable(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE)) {
    // If the device doesn't support RAW, all template should have OFF as
    // default
    if (!static_info->IsCapabilitySupported(
            ANDROID_REQUEST_AVAILABLE_CAPABILITIES_RAW)) {
      EXPECT_KEY_VALUE_EQ(default_settings,
                          ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
                          ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF);
    }
  }

  bool support_reprocessing =
      static_info->IsCapabilitySupported(
          ANDROID_REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING) ||
      static_info->IsCapabilitySupported(
          ANDROID_REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING);

  if (type == CAMERA3_TEMPLATE_STILL_CAPTURE) {
    // Not enforce high quality here, as some devices may not effectively have
    // high quality mode
    if (static_info->IsKeyAvailable(ANDROID_COLOR_CORRECTION_MODE)) {
      EXPECT_KEY_VALUE_NE(default_settings, ANDROID_COLOR_CORRECTION_MODE,
                          ANDROID_COLOR_CORRECTION_MODE_TRANSFORM_MATRIX);
    }

    // Edge enhancement, noise reduction and aberration correction modes.
    EXPECT_EQ(static_info->IsKeyAvailable(ANDROID_EDGE_MODE),
              static_info->IsKeyAvailable(ANDROID_EDGE_AVAILABLE_EDGE_MODES))
        << "Edge mode must be present in request if available edge modes are "
           "present in metadata, and vice-versa";
    if (static_info->IsKeyAvailable(ANDROID_EDGE_MODE)) {
      std::set<uint8_t> edge_modes = static_info->GetAvailableEdgeModes();
      // Don't need check fast as fast or high quality must be both present or
      // both not.
      if (edge_modes.find(ANDROID_EDGE_MODE_HIGH_QUALITY) != edge_modes.end()) {
        EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_EDGE_MODE,
                            ANDROID_EDGE_MODE_HIGH_QUALITY);
      } else {
        EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_EDGE_MODE,
                            ANDROID_EDGE_MODE_OFF);
      }
    }

    EXPECT_EQ(static_info->IsKeyAvailable(ANDROID_NOISE_REDUCTION_MODE),
              static_info->IsKeyAvailable(
                  ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES))
        << "Noise reduction mode must be present in request if available noise "
           "reductions are present in metadata, and vice-versa";
    if (static_info->IsKeyAvailable(
            ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES)) {
      std::set<uint8_t> nr_modes =
          static_info->GetAvailableNoiseReductionModes();
      // Don't need check fast as fast or high quality must be both present or
      // both not
      if (nr_modes.find(ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY) !=
          nr_modes.end()) {
        EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_NOISE_REDUCTION_MODE,
                            ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY);
      } else {
        EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_NOISE_REDUCTION_MODE,
                            ANDROID_NOISE_REDUCTION_MODE_OFF);
      }
    }

    EXPECT_EQ(
        static_info->IsKeyAvailable(ANDROID_COLOR_CORRECTION_ABERRATION_MODE),
        static_info->IsKeyAvailable(
            ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES))
        << "Aberration correction mode must be present in request if available "
           "aberration correction reductions are present in metadata, and "
           "vice-versa";
    if (static_info->IsKeyAvailable(ANDROID_COLOR_CORRECTION_ABERRATION_MODE)) {
      std::set<uint8_t> aberration_modes =
          static_info->GetAvailableColorAberrationModes();
      // Don't need check fast as fast or high quality must be both present or
      // both not
      if (aberration_modes.find(
              ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY) !=
          aberration_modes.end()) {
        EXPECT_KEY_VALUE_EQ(
            default_settings, ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
            ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY);
      } else {
        EXPECT_KEY_VALUE_EQ(default_settings,
                            ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
                            ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF);
      }
    }
  } else if (type == CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG &&
             support_reprocessing) {
    EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_EDGE_MODE,
                        ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG);
    EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_NOISE_REDUCTION_MODE,
                        ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG);
  } else if (type == CAMERA3_TEMPLATE_PREVIEW ||
             type == CAMERA3_TEMPLATE_VIDEO_RECORD) {
    if (static_info->IsKeyAvailable(ANDROID_EDGE_MODE)) {
      std::set<uint8_t> edge_modes = static_info->GetAvailableEdgeModes();
      if (edge_modes.find(ANDROID_EDGE_MODE_FAST) != edge_modes.end()) {
        EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_EDGE_MODE,
                            ANDROID_EDGE_MODE_FAST);
      } else {
        EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_EDGE_MODE,
                            ANDROID_EDGE_MODE_OFF);
      }
    }

    if (static_info->IsKeyAvailable(ANDROID_NOISE_REDUCTION_MODE)) {
      std::set<uint8_t> nr_modes =
          static_info->GetAvailableNoiseReductionModes();
      if (nr_modes.find(ANDROID_NOISE_REDUCTION_MODE_FAST) != nr_modes.end()) {
        EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_NOISE_REDUCTION_MODE,
                            ANDROID_NOISE_REDUCTION_MODE_FAST);
      } else {
        EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_NOISE_REDUCTION_MODE,
                            ANDROID_NOISE_REDUCTION_MODE_OFF);
      }
    }

    if (static_info->IsKeyAvailable(ANDROID_COLOR_CORRECTION_ABERRATION_MODE)) {
      std::set<uint8_t> aberration_modes =
          static_info->GetAvailableColorAberrationModes();
      if (aberration_modes.find(
              ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST) !=
          aberration_modes.end()) {
        EXPECT_KEY_VALUE_EQ(default_settings,
                            ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
                            ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST);
      } else {
        EXPECT_KEY_VALUE_EQ(default_settings,
                            ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
                            ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF);
      }
    }
  } else {
    if (static_info->IsKeyAvailable(ANDROID_EDGE_MODE)) {
      EXPECT_KEY_VALUE_NE(default_settings, ANDROID_EDGE_MODE, 0);
    }

    if (static_info->IsKeyAvailable(ANDROID_NOISE_REDUCTION_MODE)) {
      EXPECT_KEY_VALUE_NE(default_settings, ANDROID_NOISE_REDUCTION_MODE, 0);
    }

    if (static_info->IsKeyAvailable(ANDROID_COLOR_CORRECTION_ABERRATION_MODE)) {
      EXPECT_KEY_VALUE_NE(default_settings,
                          ANDROID_COLOR_CORRECTION_ABERRATION_MODE, 0);
    }
  }

  // Tone map and lens shading modes.
  if (type == CAMERA3_TEMPLATE_STILL_CAPTURE) {
    EXPECT_EQ(
        static_info->IsKeyAvailable(ANDROID_TONEMAP_MODE),
        static_info->IsKeyAvailable(ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES))
        << "Tonemap mode must be present in request if available tonemap modes "
           "are present in metadata, and vice-versa";
    if (static_info->IsKeyAvailable(ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES)) {
      std::set<uint8_t> tone_map_modes =
          static_info->GetAvailableToneMapModes();
      if (tone_map_modes.find(ANDROID_TONEMAP_MODE_HIGH_QUALITY) !=
          tone_map_modes.end()) {
        EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_TONEMAP_MODE,
                            ANDROID_TONEMAP_MODE_HIGH_QUALITY);
      } else {
        EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_TONEMAP_MODE,
                            ANDROID_TONEMAP_MODE_FAST);
      }
    }

    // Still capture template should have android.statistics.lensShadingMapMode
    // ON when RAW capability is supported.
    if (static_info->IsKeyAvailable(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE) &&
        static_info->IsCapabilitySupported(
            ANDROID_REQUEST_AVAILABLE_CAPABILITIES_RAW)) {
      EXPECT_KEY_VALUE_EQ(default_settings,
                          ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
                          ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_ON);
    }
  } else {
    if (static_info->IsKeyAvailable(ANDROID_TONEMAP_MODE)) {
      EXPECT_KEY_VALUE_NE(default_settings, ANDROID_TONEMAP_MODE,
                          ANDROID_TONEMAP_MODE_CONTRAST_CURVE);
      EXPECT_KEY_VALUE_NE(default_settings, ANDROID_TONEMAP_MODE,
                          ANDROID_TONEMAP_MODE_GAMMA_VALUE);
      EXPECT_KEY_VALUE_NE(default_settings, ANDROID_TONEMAP_MODE,
                          ANDROID_TONEMAP_MODE_PRESET_CURVE);
    }
    if (static_info->IsKeyAvailable(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE)) {
      EXPECT_KEY_VALUE_NE(default_settings,
                          ANDROID_STATISTICS_LENS_SHADING_MAP_MODE, 0);
    }
  }

  EXPECT_KEY_VALUE_EQ(default_settings, ANDROID_CONTROL_CAPTURE_INTENT, type);
}

// Test spec:
// - Camera ID
// - Capture type
class CreateInvalidTemplate
    : public Camera3DeviceFixture,
      public ::testing::WithParamInterface<std::tuple<int, int>> {
 public:
  CreateInvalidTemplate() : Camera3DeviceFixture(std::get<0>(GetParam())) {}
};

TEST_P(CreateInvalidTemplate, ConstructDefaultSettings) {
  // Reference:
  // camera2/cts/CameraDeviceTest.java#testCameraDeviceCreateCaptureBuilder
  int type = std::get<1>(GetParam());
  LOG(ERROR) << type;
  ASSERT_EQ(nullptr, cam_device_.ConstructDefaultRequestSettings(type))
      << "Should get error due to an invalid template ID";
}

INSTANTIATE_TEST_CASE_P(Camera3DeviceTest,
                        Camera3DeviceSimpleTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

INSTANTIATE_TEST_CASE_P(
    Camera3DeviceTest,
    Camera3DeviceDefaultSettings,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Values(CAMERA3_TEMPLATE_PREVIEW,
                                         CAMERA3_TEMPLATE_STILL_CAPTURE,
                                         CAMERA3_TEMPLATE_VIDEO_RECORD,
                                         CAMERA3_TEMPLATE_VIDEO_SNAPSHOT,
                                         CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG,
                                         CAMERA3_TEMPLATE_MANUAL)));

INSTANTIATE_TEST_CASE_P(
    Camera3DeviceTest,
    CreateInvalidTemplate,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Values(CAMERA3_TEMPLATE_PREVIEW - 1,
                                         CAMERA3_TEMPLATE_MANUAL + 1)));

}  // namespace camera3_test
