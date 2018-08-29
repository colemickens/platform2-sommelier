/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/reprocess_effect/portrait_mode_effect.h"

#include <linux/videodev2.h>
#include <sys/wait.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/command_line.h>
#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/shared_memory.h>
#include <base/process/launch.h>
#include <base/values.h>
#include <libyuv.h>
#include <libyuv/convert_argb.h>
#include <system/camera_metadata.h>

#include "cros-camera/common.h"
#include "hal_adapter/reprocess_effect/scoped_buffer_handle.h"

namespace cros {

const char kPortraitProcessorBinary[] = "/usr/bin/portrait_processor_shm";

// 1: enable portrait processing
// 0: disable portrait processing; apps should not set this value
const std::pair<std::string, uint8_t> kRequestVendorTag[] = {
    {"vendor.google.effect.portraitMode", TYPE_BYTE}};

// SegmentationResult::kSuccess: portrait mode segmentation succeeds
// SegmentationResult::kFailure: portrait mode segmentation fails
const std::pair<std::string, uint8_t> kResultVendorTag[] = {
    {"vendor.google.effect.portraitModeSegmentationResult", TYPE_BYTE}};

PortraitModeEffect::PortraitModeEffect()
    : enable_vendor_tag_(0),
      result_vendor_tag_(0),
      buffer_manager_(CameraBufferManager::GetInstance()) {}

int32_t PortraitModeEffect::InitializeAndGetVendorTags(
    std::vector<std::pair<std::string, uint8_t>>* request_vendor_tags,
    std::vector<std::pair<std::string, uint8_t>>* result_vendor_tags) {
  VLOGF_ENTER();
  if (!request_vendor_tags || !result_vendor_tags) {
    return -EINVAL;
  }
  if (access(kPortraitProcessorBinary, X_OK) != 0) {
    LOGF(WARNING)
        << "Portrait processor binary is not found. Disable portrait mode";
    return 0;
  }
  *request_vendor_tags = {std::begin(kRequestVendorTag),
                          std::end(kRequestVendorTag)};
  *result_vendor_tags = {std::begin(kResultVendorTag),
                         std::end(kResultVendorTag)};
  return 0;
}

int32_t PortraitModeEffect::SetVendorTags(uint32_t request_vendor_tag_start,
                                          uint32_t request_vendor_tag_count,
                                          uint32_t result_vendor_tag_start,
                                          uint32_t result_vendor_tag_count) {
  if (request_vendor_tag_count != arraysize(kRequestVendorTag) ||
      result_vendor_tag_count != arraysize(kResultVendorTag)) {
    return -EINVAL;
  }
  enable_vendor_tag_ = request_vendor_tag_start;
  result_vendor_tag_ = result_vendor_tag_start;
  LOGF(INFO) << "Allocated vendor tag " << std::hex << enable_vendor_tag_;
  return 0;
}

int32_t PortraitModeEffect::ReprocessRequest(
    const camera_metadata_t& settings,
    buffer_handle_t input_buffer,
    uint32_t width,
    uint32_t height,
    uint32_t orientation,
    uint32_t v4l2_format,
    android::CameraMetadata* result_metadata,
    buffer_handle_t output_buffer) {
  VLOGF_ENTER();
  camera_metadata_ro_entry_t entry = {};
  if (find_camera_metadata_ro_entry(&settings, enable_vendor_tag_, &entry) !=
      0) {
    LOGF(ERROR) << "Failed to find portrait mode vendor tag";
    return -EINVAL;
  }
  ScopedYUVBufferHandle scoped_input_handle =
      ScopedYUVBufferHandle::CreateScopedHandle(
          input_buffer, GRALLOC_USAGE_SW_READ_OFTEN, width, height);
  if (!scoped_input_handle) {
    LOGF(ERROR) << "Failed to lock input buffer handle";
    return -EINVAL;
  }
  auto* input_ycbcr = scoped_input_handle.GetYCbCr();
  ScopedYUVBufferHandle scoped_output_handle =
      ScopedYUVBufferHandle::CreateScopedHandle(
          output_buffer,
          GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN, width,
          height);
  if (!scoped_input_handle) {
    LOGF(ERROR) << "Failed to lock output buffer handle";
    return -EINVAL;
  }
  auto* output_ycbcr = scoped_output_handle.GetYCbCr();

  if (entry.data.u8[0] != 0) {
    const uint32_t kRGBNumOfChannels = 3;
    size_t rgb_buf_size = width * height * kRGBNumOfChannels;
    base::SharedMemory input_rgb_shm;
    if (!input_rgb_shm.CreateAndMapAnonymous(rgb_buf_size)) {
      LOGF(ERROR) << "Failed to create shared memory for input RGB buffer";
      return -ENOMEM;
    }
    base::SharedMemory output_rgb_shm;
    if (!output_rgb_shm.CreateAndMapAnonymous(rgb_buf_size)) {
      LOGF(ERROR) << "Failed to create shared memory for output RGB buffer";
      return -ENOMEM;
    }
    base::SharedMemory result_report_shm;
    // The size of result report is determined by portrait processor. Allocate
    // a minimum size here.
    if (!result_report_shm.CreateAnonymous(1)) {
      LOGF(ERROR) << "Failed to create shared memory for result report";
      return -ENOMEM;
    }
    uint32_t rgb_buf_stride = width * kRGBNumOfChannels;
    int result =
        ConvertYUVToRGB(v4l2_format, *input_ycbcr, input_rgb_shm.memory(),
                        rgb_buf_stride, width, height);
    if (result != 0) {
      LOGF(ERROR) << "Failed to convert from YUV to RGB";
      return result;
    }

    base::Process process = LaunchPortraitProcessor(
        input_rgb_shm.handle().fd, output_rgb_shm.handle().fd,
        result_report_shm.handle().fd, width, height, orientation);
    if (!process.IsValid()) {
      LOGF(ERROR) << "Failed to launch portrait processor";
      return -EINVAL;
    }
    int exit_code;
    if (!process.WaitForExitWithTimeout(base::TimeDelta::FromSeconds(3),
                                        &exit_code) ||
        exit_code != 0) {
      PLOGF(ERROR) << "Wait for child process error";
      return -EINVAL;
    }
    LOGF(INFO) << "Portrait processing finished";

    SegmentationResult segmentation_result = SegmentationResult::kFailure;
    size_t size = 0;
    if (!base::SharedMemory::GetSizeFromSharedMemoryHandle(
            result_report_shm.handle(), &size) ||
        size == 0) {
      LOGF(ERROR) << "Failed to get report or the report is empty";
      return -EINVAL;
    }
    if (!result_report_shm.Map(size)) {
      LOGF(ERROR) << "Failed to map shared memory";
      return -EINVAL;
    }
    std::string report(static_cast<char*>(result_report_shm.memory()), size);
    VLOGF(1) << "Result report json: " << report;
    std::unique_ptr<base::DictionaryValue> report_dict =
        base::DictionaryValue::From(base::JSONReader::Read(report));
    std::string result_value;
    if (!report_dict) {
      LOGF(ERROR) << "There is no value in report";
    } else if (!report_dict->GetString("result", &result_value)) {
      LOGF(ERROR) << "Failed to find result in report";
    } else if (result_value == "success") {
      segmentation_result = SegmentationResult::kSuccess;
    }

    result = ConvertRGBToYUV(output_rgb_shm.memory(), rgb_buf_stride,
                             v4l2_format, *output_ycbcr, width, height);
    if (result != 0) {
      LOGF(ERROR) << "Failed to convert from RGB to YUV";
      segmentation_result = SegmentationResult::kFailure;
    }
    result_metadata->update(result_vendor_tag_,
                            reinterpret_cast<uint8_t*>(&segmentation_result),
                            1);
    return result;
  } else {
    // TODO(hywu): add an API to query if an effect want to reprocess this
    // request or not
    LOGF(WARNING) << "Portrait mode is turned off. Just copy the image.";
    switch (v4l2_format) {
      case V4L2_PIX_FMT_NV12:
      case V4L2_PIX_FMT_NV12M:
        libyuv::CopyPlane(static_cast<const uint8_t*>(input_ycbcr->y),
                          input_ycbcr->ystride,
                          static_cast<uint8_t*>(output_ycbcr->y),
                          output_ycbcr->ystride, width, height);
        libyuv::CopyPlane_16(static_cast<const uint16_t*>(input_ycbcr->cb),
                             input_ycbcr->cstride,
                             static_cast<uint16_t*>(output_ycbcr->cb),
                             output_ycbcr->cstride, width, height);
        break;
      case V4L2_PIX_FMT_NV21:
      case V4L2_PIX_FMT_NV21M:
        libyuv::CopyPlane(static_cast<const uint8_t*>(input_ycbcr->y),
                          input_ycbcr->ystride,
                          static_cast<uint8_t*>(output_ycbcr->y),
                          output_ycbcr->ystride, width, height);
        libyuv::CopyPlane_16(static_cast<const uint16_t*>(input_ycbcr->cr),
                             input_ycbcr->cstride,
                             static_cast<uint16_t*>(output_ycbcr->cr),
                             output_ycbcr->cstride, width, height);
        break;
      case V4L2_PIX_FMT_YUV420:
      case V4L2_PIX_FMT_YUV420M:
      case V4L2_PIX_FMT_YVU420:
      case V4L2_PIX_FMT_YVU420M:
        if (libyuv::I420Copy(
                static_cast<const uint8_t*>(input_ycbcr->y),
                input_ycbcr->ystride,
                static_cast<const uint8_t*>(input_ycbcr->cb),
                input_ycbcr->cstride,
                static_cast<const uint8_t*>(input_ycbcr->cr),
                input_ycbcr->cstride, static_cast<uint8_t*>(output_ycbcr->y),
                output_ycbcr->ystride, static_cast<uint8_t*>(output_ycbcr->cb),
                output_ycbcr->cstride, static_cast<uint8_t*>(output_ycbcr->cr),
                output_ycbcr->cstride, width, height) != 0) {
          LOGF(ERROR) << "Failed to copy I420";
          return -ENOMEM;
        }
        break;
      default:
        LOGF(ERROR) << "Unsupported format " << FormatToString(v4l2_format);
        return -EINVAL;
    }
  }
  return 0;
}

base::Process PortraitModeEffect::LaunchPortraitProcessor(
    int input_rgb_buf_fd,
    int output_rgb_buf_fd,
    int result_report_fd,
    uint32_t width,
    uint32_t height,
    uint32_t orientation) {
  int dup_input_rgb_buf_fd = dup(input_rgb_buf_fd);
  int dup_output_rgb_buf_fd = dup(output_rgb_buf_fd);
  int dup_result_report_fd = dup(result_report_fd);
  LOGF(INFO) << "Prepare arguments for portrait processor";
  // Added a pair of parentheses to declare a variable so as to avoid the most
  // vexing parse ambiguity
  base::CommandLine cmdline((base::FilePath(kPortraitProcessorBinary)));
  cmdline.AppendSwitchASCII("debug_images_verbosity", "1");
  cmdline.AppendSwitchASCII("input_shmbuf_fd",
                            std::to_string(dup_input_rgb_buf_fd));
  cmdline.AppendSwitchASCII("output_shmbuf_fd",
                            std::to_string(dup_output_rgb_buf_fd));
  cmdline.AppendSwitchASCII("width", std::to_string(width));
  cmdline.AppendSwitchASCII("height", std::to_string(height));
  cmdline.AppendSwitchASCII("orientation", std::to_string(orientation));
  cmdline.AppendSwitchASCII("result_report_fd",
                            std::to_string(dup_result_report_fd));
  VLOGF(1) << cmdline.GetCommandLineString();
  LOGF(INFO) << "Start portrait processing ...";
  base::FileHandleMappingVector fds_to_remap;
  fds_to_remap.emplace_back(dup_input_rgb_buf_fd, dup_input_rgb_buf_fd);
  fds_to_remap.emplace_back(dup_output_rgb_buf_fd, dup_output_rgb_buf_fd);
  fds_to_remap.emplace_back(dup_result_report_fd, dup_result_report_fd);
  base::LaunchOptions options;
  options.fds_to_remap = &fds_to_remap;
  return base::LaunchProcess(cmdline, options);
}

int PortraitModeEffect::ConvertYUVToRGB(uint32_t v4l2_format,
                                        const android_ycbcr& ycbcr,
                                        void* rgb_buf_addr,
                                        uint32_t rgb_buf_stride,
                                        uint32_t width,
                                        uint32_t height) {
  switch (v4l2_format) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      if (libyuv::NV12ToRGB24(
              static_cast<const uint8_t*>(ycbcr.y), ycbcr.ystride,
              static_cast<const uint8_t*>(ycbcr.cb), ycbcr.cstride,
              static_cast<uint8_t*>(rgb_buf_addr), rgb_buf_stride, width,
              height) != 0) {
        LOGF(ERROR) << "Failed to convert from NV12 to RGB";
        return -EINVAL;
      }
      break;
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV21M:
      if (libyuv::NV21ToRGB24(
              static_cast<const uint8_t*>(ycbcr.y), ycbcr.ystride,
              static_cast<const uint8_t*>(ycbcr.cr), ycbcr.cstride,
              static_cast<uint8_t*>(rgb_buf_addr), rgb_buf_stride, width,
              height) != 0) {
        LOGF(ERROR) << "Failed to convert from NV21 to RGB";
        return -EINVAL;
      }
      break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_YVU420M:
      if (libyuv::I420ToRGB24(
              static_cast<const uint8_t*>(ycbcr.y), ycbcr.ystride,
              static_cast<const uint8_t*>(ycbcr.cb), ycbcr.cstride,
              static_cast<const uint8_t*>(ycbcr.cr), ycbcr.cstride,
              static_cast<uint8_t*>(rgb_buf_addr), rgb_buf_stride, width,
              height) != 0) {
        LOGF(ERROR) << "Failed to convert from I420 to RGB";
        return -EINVAL;
      }
      break;
    default:
      LOGF(ERROR) << "Unsupported format " << FormatToString(v4l2_format);
      return -EINVAL;
  }
  return 0;
}

int PortraitModeEffect::ConvertRGBToYUV(void* rgb_buf_addr,
                                        uint32_t rgb_buf_stride,
                                        uint32_t v4l2_format,
                                        const android_ycbcr& ycbcr,
                                        uint32_t width,
                                        uint32_t height) {
  auto convert_rgb_to_nv = [](const uint8_t* rgb_addr,
                              const android_ycbcr& ycbcr, uint32_t width,
                              uint32_t height, uint32_t v4l2_format) {
    // TODO(hywu): convert RGB to NV12/NV21 directly
    auto div_round_up = [](uint32_t n, uint32_t d) {
      return ((n + d - 1) / d);
    };
    const uint32_t kRGBNumOfChannels = 3;
    uint32_t ystride = width;
    uint32_t cstride = div_round_up(width, 2);
    uint32_t total_size =
        width * height + cstride * div_round_up(height, 2) * 2;
    uint32_t uv_plane_size = cstride * div_round_up(height, 2);
    auto i420_y = std::make_unique<uint8_t[]>(total_size);
    uint8_t* i420_cb = i420_y.get() + width * height;
    uint8_t* i420_cr = i420_cb + uv_plane_size;
    if (libyuv::RGB24ToI420(static_cast<const uint8_t*>(rgb_addr),
                            width * kRGBNumOfChannels, i420_y.get(), ystride,
                            i420_cb, cstride, i420_cr, cstride, width,
                            height) != 0) {
      LOGF(ERROR) << "Failed to convert from RGB to I420";
      return -ENOMEM;
    }
    if (v4l2_format == V4L2_PIX_FMT_NV12) {
      if (libyuv::I420ToNV12(i420_y.get(), ystride, i420_cb, cstride, i420_cr,
                             cstride, static_cast<uint8_t*>(ycbcr.y),
                             ycbcr.ystride, static_cast<uint8_t*>(ycbcr.cb),
                             ycbcr.cstride, width, height) != 0) {
        LOGF(ERROR) << "Failed to convert from I420 to NV12";
        return -ENOMEM;
      }
    } else if (v4l2_format == V4L2_PIX_FMT_NV21) {
      if (libyuv::I420ToNV21(i420_y.get(), ystride, i420_cb, cstride, i420_cr,
                             cstride, static_cast<uint8_t*>(ycbcr.y),
                             ycbcr.ystride, static_cast<uint8_t*>(ycbcr.cr),
                             ycbcr.cstride, width, height) != 0) {
        LOGF(ERROR) << "Failed to convert from I420 to NV21";
        return -ENOMEM;
      }
    } else {
      return -EINVAL;
    }
    return 0;
  };
  switch (v4l2_format) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      if (convert_rgb_to_nv(static_cast<const uint8_t*>(rgb_buf_addr), ycbcr,
                            width, height, V4L2_PIX_FMT_NV12) != 0) {
        return -EINVAL;
      }
      break;
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV21M:
      if (convert_rgb_to_nv(static_cast<const uint8_t*>(rgb_buf_addr), ycbcr,
                            width, height, V4L2_PIX_FMT_NV21) != 0) {
        return -EINVAL;
      }
      break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_YVU420M:
      if (libyuv::RGB24ToI420(static_cast<const uint8_t*>(rgb_buf_addr),
                              rgb_buf_stride, static_cast<uint8_t*>(ycbcr.y),
                              ycbcr.ystride, static_cast<uint8_t*>(ycbcr.cb),
                              ycbcr.cstride, static_cast<uint8_t*>(ycbcr.cr),
                              ycbcr.cstride, width, height) != 0) {
        LOGF(ERROR) << "Failed to convert from RGB";
        return -EINVAL;
      }
      break;
    default:
      LOGF(ERROR) << "Unsupported format " << FormatToString(v4l2_format);
      return -EINVAL;
  }
  return 0;
}

}  // namespace cros
