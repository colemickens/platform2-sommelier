/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/metadata_handler.h"

#include <cmath>
#include <limits>
#include <vector>

#include "arc/common.h"
#include "hal/usb/stream_format.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define UPDATE(tag, data, size)                      \
  {                                                  \
    if (metadata->Update((tag), (data), (size))) {   \
      LOGF(ERROR) << "Update " << #tag << " failed"; \
      return -EINVAL;                                \
    }                                                \
  }

namespace arc {

MetadataHandler::MetadataHandler(const camera_metadata_t& metadata)
    : af_trigger_(false) {
  // MetadataBase::operator= will make a copy of camera_metadata_t.
  metadata_ = &metadata;

  // camera3_request_template_t starts at 1.
  for (int i = 1; i < CAMERA3_TEMPLATE_COUNT; i++) {
    template_settings_[i] = CreateDefaultRequestSettings(i);
    if (template_settings_[i].get() == nullptr) {
      LOGF(ERROR) << "metadata for template type (" << i << ") is NULL";
    }
  }

  thread_checker_.DetachFromThread();
}

MetadataHandler::~MetadataHandler() {}

int MetadataHandler::FillDefaultMetadata(CameraMetadata* metadata) {
  const uint8_t hardware_level = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED;
  UPDATE(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL, &hardware_level, 1);

  // android.request
  const uint8_t available_capabilities[] = {
      ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE};
  UPDATE(ANDROID_REQUEST_AVAILABLE_CAPABILITIES, available_capabilities,
         ARRAY_SIZE(available_capabilities));

  const int32_t partial_result_count = 1;
  UPDATE(ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &partial_result_count, 1);

  // This means pipeline latency of X frame intervals. The maximum number is 4.
  const uint8_t request_pipeline_max_depth = 4;
  UPDATE(ANDROID_REQUEST_PIPELINE_MAX_DEPTH, &request_pipeline_max_depth, 1);

  // Three numbers represent the maximum numbers of different types of output
  // streams simultaneously. The types are raw sensor, processed (but not
  // stalling), and processed (but stalling). For usb limited mode, raw sensor
  // is not supported. Stalling stream is JPEG. Non-stalling streams are
  // YUV_420_888, NV21, or YV12.
  const int32_t request_max_num_output_streams[] = {0, 2, 1};
  UPDATE(ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS, request_max_num_output_streams,
         ARRAY_SIZE(request_max_num_output_streams));

  // Limited mode doesn't support reprocessing.
  const int32_t request_max_num_input_streams = 0;
  UPDATE(ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS, &request_max_num_input_streams,
         1);

  // android.jpeg
  const int32_t jpeg_available_thumbnail_sizes[] = {0, 0, 320, 240};
  UPDATE(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES, jpeg_available_thumbnail_sizes,
         ARRAY_SIZE(jpeg_available_thumbnail_sizes));

  const int32_t jpeg_max_size[] = {13 * 1024 * 1024};  // 13MB
  UPDATE(ANDROID_JPEG_MAX_SIZE, jpeg_max_size, ARRAY_SIZE(jpeg_max_size));

  const uint8_t jpeg_quality = 90;
  UPDATE(ANDROID_JPEG_QUALITY, &jpeg_quality, 1);
  UPDATE(ANDROID_JPEG_THUMBNAIL_QUALITY, &jpeg_quality, 1);

  // android.scaler
  const float scaler_available_max_digital_zoom[] = {1};
  UPDATE(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM,
         scaler_available_max_digital_zoom,
         ARRAY_SIZE(scaler_available_max_digital_zoom));

  const uint8_t cropping_type = ANDROID_SCALER_CROPPING_TYPE_CENTER_ONLY;
  UPDATE(ANDROID_SCALER_CROPPING_TYPE, &cropping_type, 1);

  // android.colorCorrection
  const uint8_t available_aberration_modes[] = {
      ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST,
      ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY};
  UPDATE(ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES,
         available_aberration_modes, ARRAY_SIZE(available_aberration_modes));

  // android.control
  // We don't support AE compensation.
  const int32_t control_ae_compensation_range[] = {0, 0};
  UPDATE(ANDROID_CONTROL_AE_COMPENSATION_RANGE, control_ae_compensation_range,
         ARRAY_SIZE(control_ae_compensation_range));

  const camera_metadata_rational_t control_ae_compensation_step[] = {{0, 0}};
  UPDATE(ANDROID_CONTROL_AE_COMPENSATION_STEP, control_ae_compensation_step,
         ARRAY_SIZE(control_ae_compensation_step));

  const int32_t control_max_regions[] = {/*AE*/ 0, /*AWB*/ 0, /*AF*/ 0};
  UPDATE(ANDROID_CONTROL_MAX_REGIONS, control_max_regions,
         ARRAY_SIZE(control_max_regions));

  const uint8_t video_stabilization_mode =
      ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
  UPDATE(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES,
         &video_stabilization_mode, 1);
  UPDATE(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &video_stabilization_mode,
         1);

  const uint8_t awb_available_mode = ANDROID_CONTROL_AWB_MODE_AUTO;
  UPDATE(ANDROID_CONTROL_AWB_AVAILABLE_MODES, &awb_available_mode, 1);
  UPDATE(ANDROID_CONTROL_AWB_MODE, &awb_available_mode, 1);

  const uint8_t ae_antibanding_mode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF;
  UPDATE(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES, &ae_antibanding_mode,
         1);
  UPDATE(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &ae_antibanding_mode, 1);

  const uint8_t ae_available_modes[] = {ANDROID_CONTROL_AE_MODE_ON,
                                        ANDROID_CONTROL_AE_MODE_OFF};
  UPDATE(ANDROID_CONTROL_AE_AVAILABLE_MODES, ae_available_modes,
         ARRAY_SIZE(ae_available_modes));
  // ON means auto-exposure is active with no flash control.
  UPDATE(ANDROID_CONTROL_AE_MODE, &ae_available_modes[0], 1);

  const uint8_t af_available_mode = ANDROID_CONTROL_AF_MODE_AUTO;
  UPDATE(ANDROID_CONTROL_AF_AVAILABLE_MODES, &af_available_mode, 1);
  UPDATE(ANDROID_CONTROL_AF_MODE, &af_available_mode, 1);

  const uint8_t available_scene_mode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
  UPDATE(ANDROID_CONTROL_AVAILABLE_SCENE_MODES, &available_scene_mode, 1);
  UPDATE(ANDROID_CONTROL_SCENE_MODE, &available_scene_mode, 1);

  const uint8_t available_effect = ANDROID_CONTROL_EFFECT_MODE_OFF;
  UPDATE(ANDROID_CONTROL_AVAILABLE_EFFECTS, &available_effect, 1);
  UPDATE(ANDROID_CONTROL_EFFECT_MODE, &available_effect, 1);

  const uint8_t ae_lock_available = ANDROID_CONTROL_AE_LOCK_AVAILABLE_FALSE;
  UPDATE(ANDROID_CONTROL_AE_LOCK_AVAILABLE, &ae_lock_available, 1);

  const uint8_t awb_lock_available = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_FALSE;
  UPDATE(ANDROID_CONTROL_AWB_LOCK_AVAILABLE, &awb_lock_available, 1);

  const uint8_t control_available_modes[] = {ANDROID_CONTROL_MODE_OFF,
                                             ANDROID_CONTROL_MODE_AUTO};
  UPDATE(ANDROID_CONTROL_AVAILABLE_MODES, control_available_modes,
         sizeof(control_available_modes));

  // android.edge
  const uint8_t available_edge_modes[] = {ANDROID_EDGE_MODE_OFF,
                                          ANDROID_EDGE_MODE_FAST};
  UPDATE(ANDROID_EDGE_AVAILABLE_EDGE_MODES, &available_edge_modes[0], 1);

  // android.flash
  const uint8_t flash_info = ANDROID_FLASH_INFO_AVAILABLE_FALSE;
  UPDATE(ANDROID_FLASH_INFO_AVAILABLE, &flash_info, 1);

  const uint8_t flash_state = ANDROID_FLASH_STATE_UNAVAILABLE;
  UPDATE(ANDROID_FLASH_STATE, &flash_state, 1);

  // This should not be needed.
  const float hyper_focal_distance = 0.0f;
  UPDATE(ANDROID_LENS_INFO_HYPERFOCAL_DISTANCE, &hyper_focal_distance, 1);

  // android.lens
  const uint8_t optical_stabilization_mode =
      ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;
  UPDATE(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION,
         &optical_stabilization_mode, 1);
  UPDATE(ANDROID_LENS_OPTICAL_STABILIZATION_MODE, &optical_stabilization_mode,
         1);

  // android.noiseReduction
  const uint8_t noise_reduction_mode = ANDROID_NOISE_REDUCTION_MODE_OFF;
  UPDATE(ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES,
         &noise_reduction_mode, 1);

  // android.statistics
  const uint8_t face_detect_mode = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
  UPDATE(ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES, &face_detect_mode,
         1);

  const int32_t max_face_count = 0;
  UPDATE(ANDROID_STATISTICS_INFO_MAX_FACE_COUNT, &max_face_count, 1);

  // android.sensor
  // UVC driver cannot set ISO sensitivity. Use the minimum range.
  // Android document says the minimum should be <= 100, and maximum should be
  // >= 800.
  const int32_t sensor_info_sensitivity_range[] = {100, 800};
  UPDATE(ANDROID_SENSOR_INFO_SENSITIVITY_RANGE, sensor_info_sensitivity_range,
         ARRAY_SIZE(sensor_info_sensitivity_range));

  const int32_t test_pattern_mode = ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;
  UPDATE(ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES, &test_pattern_mode, 1);

  const uint8_t timestamp_source = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_UNKNOWN;
  UPDATE(ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE, &timestamp_source, 1);

  // android.shading
  const uint8_t availabe_mode = ANDROID_SHADING_MODE_FAST;
  UPDATE(ANDROID_SHADING_AVAILABLE_MODES, &availabe_mode, 1);

  // android.sync
  const int32_t max_latency = ANDROID_SYNC_MAX_LATENCY_UNKNOWN;
  UPDATE(ANDROID_SYNC_MAX_LATENCY, &max_latency, 1);
  return 0;
}

int MetadataHandler::FillMetadataFromSupportedFormats(
    const SupportedFormats& supported_formats,
    CameraMetadata* metadata) {
  if (supported_formats.empty()) {
    return -EINVAL;
  }
  std::vector<int32_t> stream_configurations;
  std::vector<int64_t> min_frame_durations;
  std::vector<int64_t> stall_durations;
  int64_t max_frame_duration = std::numeric_limits<int64_t>::min();
  int32_t max_fps = std::numeric_limits<int32_t>::min();
  int32_t min_fps = std::numeric_limits<int32_t>::max();

  std::vector<int> hal_formats{HAL_PIXEL_FORMAT_BLOB,
                               HAL_PIXEL_FORMAT_YCbCr_420_888,
                               HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED};

  for (const auto& supported_format : supported_formats) {
    for (const auto& format : hal_formats) {
      stream_configurations.push_back(format);
      stream_configurations.push_back(supported_format.width);
      stream_configurations.push_back(supported_format.height);
      stream_configurations.push_back(
          ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT);
    }

    int64_t min_frame_duration = std::numeric_limits<int64_t>::max();
    for (const auto& frame_rate : supported_format.frameRates) {
      int64_t frame_duration = 1000000000LL / frame_rate;
      if (frame_duration < min_frame_duration) {
        min_frame_duration = frame_duration;
      }
      if (frame_duration > max_frame_duration) {
        max_frame_duration = frame_duration;
      }
      if (min_fps > static_cast<int32_t>(frame_rate)) {
        min_fps = static_cast<int32_t>(frame_rate);
      }
      if (max_fps < static_cast<int32_t>(frame_rate)) {
        max_fps = static_cast<int32_t>(frame_rate);
      }
    }

    for (const auto& format : hal_formats) {
      min_frame_durations.push_back(format);
      min_frame_durations.push_back(supported_format.width);
      min_frame_durations.push_back(supported_format.height);
      min_frame_durations.push_back(min_frame_duration);
    }

    // The stall duration is 0 for non-jpeg formats. For JPEG format, stall
    // duration can be 0 if JPEG is small. 5MP JPEG takes 700ms to decode
    // and encode. Here we choose 1 sec for JPEG.
    for (const auto& format : hal_formats) {
      // For non-jpeg formats, the camera orientation workaround crops,
      // rotates, and scales the frames. Theoretically the stall duration of
      // huge resolution may be bigger than 0. Set it to 0 for now.
      int64_t stall_duration =
          (format == HAL_PIXEL_FORMAT_BLOB) ? 1000000000 : 0;
      stall_durations.push_back(format);
      stall_durations.push_back(supported_format.width);
      stall_durations.push_back(supported_format.height);
      stall_durations.push_back(stall_duration);
    }
  }

  // The document in aeAvailableTargetFpsRanges section says the min_fps should
  // not be larger than 15.
  // We cannot support fixed 30fps but Android requires (min, max) and
  // (max, max) ranges.
  int32_t fps_ranges[] = {min_fps, max_fps, max_fps, max_fps};
  UPDATE(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, fps_ranges,
         ARRAY_SIZE(fps_ranges));

  UPDATE(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps_ranges, sizeof(int32_t) * 2);

  UPDATE(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
         stream_configurations.data(), stream_configurations.size());

  UPDATE(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
         min_frame_durations.data(), min_frame_durations.size());

  UPDATE(ANDROID_SCALER_AVAILABLE_STALL_DURATIONS, stall_durations.data(),
         stall_durations.size());

  UPDATE(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, &max_frame_duration, 1);

  SupportedFormat maximum_format = GetMaximumFormat(supported_formats);
  int32_t pixel_array_size[] = {static_cast<int32_t>(maximum_format.width),
                                static_cast<int32_t>(maximum_format.height)};
  UPDATE(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, pixel_array_size,
         ARRAY_SIZE(pixel_array_size));

  int32_t active_array_size[] = {0, 0,
                                 static_cast<int32_t>(maximum_format.width),
                                 static_cast<int32_t>(maximum_format.height)};
  UPDATE(ANDROID_SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE,
         active_array_size, ARRAY_SIZE(active_array_size));
  UPDATE(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, active_array_size,
         ARRAY_SIZE(active_array_size));

  return 0;
}

int MetadataHandler::FillMetadataFromDeviceInfo(const DeviceInfo& device_info,
                                                CameraMetadata* metadata) {
  UPDATE(ANDROID_SENSOR_ORIENTATION, &device_info.sensor_orientation, 1);

  uint8_t lens_facing = device_info.lens_facing;
  UPDATE(ANDROID_LENS_FACING, &lens_facing, 1);

  // TODO(henryhsu): Set physical size from device info.
  float physical_size[] = {3.2, 2.4};
  UPDATE(ANDROID_SENSOR_INFO_PHYSICAL_SIZE, physical_size,
         ARRAY_SIZE(physical_size));

  UPDATE(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
         device_info.lens_info_available_focal_lengths.data(),
         device_info.lens_info_available_focal_lengths.size());

  UPDATE(ANDROID_LENS_FOCAL_LENGTH,
         &device_info.lens_info_available_focal_lengths.data()[0], 1);

  UPDATE(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE,
         &device_info.lens_info_minimum_focus_distance, 1);
  UPDATE(ANDROID_LENS_FOCUS_DISTANCE,
         &device_info.lens_info_optimal_focus_distance, 1);

  uint8_t focus_distance_calibration =
      ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
  UPDATE(ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION,
         &focus_distance_calibration, 1);

  return 0;
}

const camera_metadata_t* MetadataHandler::GetDefaultRequestSettings(
    int template_type) {
  if (!IsValidTemplateType(template_type)) {
    LOGF(ERROR) << "Invalid template request type: " << template_type;
    return nullptr;
  }
  return template_settings_[template_type].get();
}

void MetadataHandler::PreHandleRequest(int frame_number,
                                       const CameraMetadata& metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (metadata.Exists(ANDROID_CONTROL_AF_TRIGGER)) {
    camera_metadata_ro_entry entry = metadata.Find(ANDROID_CONTROL_AF_TRIGGER);
    if (entry.data.u8[0] == ANDROID_CONTROL_AF_TRIGGER_START) {
      af_trigger_ = true;
    } else if (entry.data.u8[0] == ANDROID_CONTROL_AF_TRIGGER_CANCEL) {
      af_trigger_ = false;
    }
  }
  current_frame_number_ = frame_number;
}

void MetadataHandler::PostHandleRequest(int frame_number,
                                        int64_t timestamp,
                                        CameraMetadata* metadata) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (current_frame_number_ != frame_number) {
    LOGF(ERROR)
        << "Frame number mismatch in PreHandleRequest and PostHandleRequest";
    return;
  }

  metadata->Update(ANDROID_SENSOR_TIMESTAMP, &timestamp, 1);

  // For USB camera, we don't know the AE state. Set the state to converged to
  // indicate the frame should be good to use. Then apps don't have to wait the
  // AE state.
  uint8_t ae_state = ANDROID_CONTROL_AE_STATE_CONVERGED;
  metadata->Update(ANDROID_CONTROL_AE_STATE, &ae_state, 1);

  // For USB camera, the USB camera handles everything and we don't have control
  // over AF. We only simply fake the AF metadata based on the request
  // received here.
  uint8_t af_state;
  if (af_trigger_) {
    af_state = ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED;
  } else {
    af_state = ANDROID_CONTROL_AF_STATE_INACTIVE;
  }
  metadata->Update(ANDROID_CONTROL_AF_STATE, &af_state, 1);

  // Set AWB state to converged to indicate the frame should be good to use.
  uint8_t awb_state = ANDROID_CONTROL_AWB_STATE_CONVERGED;
  metadata->Update(ANDROID_CONTROL_AWB_STATE, &awb_state, 1);
}

bool MetadataHandler::IsValidTemplateType(int template_type) {
  return template_type > 0 && template_type < CAMERA3_TEMPLATE_COUNT;
}

CameraMetadataUniquePtr MetadataHandler::CreateDefaultRequestSettings(
    int template_type) {
  uint8_t capture_intent;
  switch (template_type) {
    case CAMERA3_TEMPLATE_PREVIEW:
      capture_intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
      break;
    case CAMERA3_TEMPLATE_STILL_CAPTURE:
      capture_intent = ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE;
      break;
    case CAMERA3_TEMPLATE_VIDEO_RECORD:
      capture_intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD;
      break;
    case CAMERA3_TEMPLATE_VIDEO_SNAPSHOT:
      capture_intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT;
      break;
    case CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG:
      capture_intent = ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG;
      break;
    case CAMERA3_TEMPLATE_MANUAL:
      capture_intent = ANDROID_CONTROL_CAPTURE_INTENT_MANUAL;
      break;
    default:
      LOGF(ERROR) << "Invalid template request type: " << template_type;
      return NULL;
  }

  CameraMetadata data(metadata_);
  uint8_t control_mode = ANDROID_CONTROL_MODE_AUTO;

  if (data.Update(ANDROID_CONTROL_MODE, &control_mode, 1) ||
      data.Update(ANDROID_CONTROL_CAPTURE_INTENT, &capture_intent, 1)) {
    return CameraMetadataUniquePtr();
  }
  return CameraMetadataUniquePtr(data.Release());
}

}  // namespace arc
