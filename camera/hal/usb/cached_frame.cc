/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/cached_frame.h"

#include <errno.h>

#include <string>
#include <vector>

#include <hardware/camera3.h>

#include <base/timer/elapsed_timer.h>
#include "cros-camera/common.h"
#include "cros-camera/exif_utils.h"
#include "cros-camera/utils/camera_config.h"
#include "hal/usb/common_types.h"

namespace cros {

// How precise the float-to-rational conversion for EXIF tags would be.
static const int kRationalPrecision = 10000;

static bool SetExifTags(const android::CameraMetadata& metadata,
                        const FrameBuffer& in_frame,
                        ExifUtils* utils);

static void InsertJpegBlob(FrameBuffer* out_frame, uint32_t jpeg_data_size);

CachedFrame::CachedFrame()
    : temp_frame_(new SharedFrameBuffer(0)),
      temp_frame2_(new SharedFrameBuffer(0)),
      yu12_frame_(new SharedFrameBuffer(0)),
      image_processor_(new ImageProcessor()),
      camera_metrics_(CameraMetrics::New()),
      jda_available_(false),
      force_jpeg_hw_encode_(false),
      force_jpeg_hw_decode_(false) {
  jda_ = JpegDecodeAccelerator::CreateInstance();
  jda_available_ = jda_->Start();
  LOGF(INFO) << "JDA available: " << jda_available_;

  jpeg_compressor_ = JpegCompressor::GetInstance();

  // Read force_jpeg_hw_(enc|dec) configs
  std::unique_ptr<CameraConfig> camera_config =
      CameraConfig::Create(constants::kCrosCameraTestConfigPathString);
  force_jpeg_hw_encode_ = camera_config->GetBoolean(
      constants::kCrosForceJpegHardwareEncodeOption, false);
  force_jpeg_hw_decode_ = camera_config->GetBoolean(
      constants::kCrosForceJpegHardwareDecodeOption, false);
  LOGF(INFO) << "Force JPEG hardware encode: " << force_jpeg_hw_encode_;
  LOGF(INFO) << "Force JPEG hardware decode: " << force_jpeg_hw_decode_;
}

int CachedFrame::SetSource(const FrameBuffer& in_frame, int rotate_degree) {
  if (in_frame.GetHeight() % 2 != 0 || in_frame.GetWidth() % 2 != 0) {
    LOGF(ERROR) << "Source image has odd dimension: " << in_frame.GetWidth()
                << "x" << in_frame.GetHeight();
    return -EINVAL;
  }

  VLOGF(2) << "Source image: " << in_frame.GetWidth() << "x"
           << in_frame.GetHeight() << " "
           << FormatToString(in_frame.GetFourcc()) << ", rotate "
           << rotate_degree;

  int ret = ConvertToYU12(in_frame);
  if (ret != 0) {
    return ret;
  }
  if (rotate_degree > 0) {
    ret = CropRotateScale(rotate_degree);
    if (ret != 0) {
      return ret;
    }
  }
  return 0;
}

int CachedFrame::Convert(const android::CameraMetadata& metadata,
                         int crop_width,
                         int crop_height,
                         FrameBuffer* out_frame) {
  if (!yu12_frame_) {
    LOGF(ERROR) << "No source image available";
    return -EINVAL;
  }

  VLOGF(2) << "Convert " << yu12_frame_->GetWidth() << "x"
           << yu12_frame_->GetHeight() << " YU12 to output "
           << out_frame->GetWidth() << "x" << out_frame->GetHeight() << " "
           << FormatToString(out_frame->GetFourcc()) << ", crop " << crop_width
           << "x" << crop_height;

  int ret = 0;
  FrameBuffer* final_yu12_frame = yu12_frame_.get();

  // Crop if needed
  if (crop_width != yu12_frame_->GetWidth() ||
      crop_height != yu12_frame_->GetHeight()) {
    temp_frame_->SetWidth(crop_width);
    temp_frame_->SetHeight(crop_height);
    ret = image_processor_->Crop(*yu12_frame_.get(), temp_frame_.get());
    if (ret) {
      LOGF(ERROR) << "Crop failed: " << ret;
      return ret;
    }
    final_yu12_frame = temp_frame_.get();
  }

  // Scale if needed
  if (out_frame->GetWidth() != crop_width ||
      out_frame->GetHeight() != crop_height) {
    temp_frame2_->SetWidth(out_frame->GetWidth());
    temp_frame2_->SetHeight(out_frame->GetHeight());
    ret = image_processor_->Scale(*final_yu12_frame, temp_frame2_.get());
    if (ret) {
      LOGF(ERROR) << "Scale failed: " << ret;
      return ret;
    }
    final_yu12_frame = temp_frame2_.get();
  }

  // Use JPEG compressor to output JPEG
  if (out_frame->GetFourcc() == V4L2_PIX_FMT_JPEG) {
    return ConvertYU12ToJpeg(metadata, *final_yu12_frame, out_frame);
  }

  // Output other formats
  return image_processor_->ConvertFormat(metadata, *final_yu12_frame,
                                         out_frame);
}

int CachedFrame::ConvertToYU12(const FrameBuffer& in_frame) {
  yu12_frame_->SetFourcc(V4L2_PIX_FMT_YUV420);
  yu12_frame_->SetWidth(in_frame.GetWidth());
  yu12_frame_->SetHeight(in_frame.GetHeight());

  size_t data_size = ImageProcessor::GetConvertedSize(*yu12_frame_);
  if (data_size == 0 || yu12_frame_->SetDataSize(data_size)) {
    LOGF(ERROR) << "Failed to calculate or set size of YU12 frame";
    return -EINVAL;
  }

  if (in_frame.GetFourcc() == V4L2_PIX_FMT_MJPEG) {
    // Try HW decoding.
    int ret = DecodeToYU12ByJDA(in_frame);
    if (ret == 0 || ret == -EAGAIN) {
      return ret;
    }
    // JDA error, fallback to SW decoding if not forcing HW decoding.
    if (force_jpeg_hw_decode_) {
      return -EINVAL;
    }
  }

  base::ElapsedTimer timer;
  // SW conversion. If this fails, the input frame is likely invalid. Return
  // -EAGAIN so HAL can skip this frame.
  int ret = image_processor_->ConvertFormat(android::CameraMetadata(), in_frame,
                                            yu12_frame_.get());
  if (ret) {
    LOGF(ERROR) << "Convert from " << FormatToString(in_frame.GetFourcc())
                << " to YU12 failed: " << ret;
    return -EAGAIN;
  }
  camera_metrics_->SendJpegProcessLatency(
      JpegProcessType::kDecode, JpegProcessMethod::kSoftware, timer.Elapsed());
  camera_metrics_->SendJpegResolution(
      JpegProcessType::kDecode, JpegProcessMethod::kSoftware,
      in_frame.GetWidth(), in_frame.GetHeight());
  return 0;
}

int CachedFrame::DecodeToYU12ByJDA(const FrameBuffer& in_frame) {
  if (!jda_available_) {
    return -EINVAL;
  }

  DCHECK(in_frame.GetFourcc() == V4L2_PIX_FMT_MJPEG);
  DCHECK(yu12_frame_->GetFourcc() == V4L2_PIX_FMT_YUV420);
  DCHECK(yu12_frame_->GetWidth() == in_frame.GetWidth() &&
         yu12_frame_->GetHeight() == in_frame.GetHeight());

  JpegDecodeAccelerator::Error error = jda_->DecodeSync(
      in_frame.GetFd(), in_frame.GetDataSize(), in_frame.GetWidth(),
      in_frame.GetHeight(), yu12_frame_->GetFd(), yu12_frame_->GetBufferSize());
  switch (error) {
    case JpegDecodeAccelerator::Error::NO_ERRORS:
      return 0;
    case JpegDecodeAccelerator::Error::PARSE_JPEG_FAILED:
    case JpegDecodeAccelerator::Error::UNSUPPORTED_JPEG:
      // Camera device may temporarily output invalid MJPEG stream. HAL should
      // skip this frame and get the next frame.
      return -EAGAIN;
    case JpegDecodeAccelerator::Error::TRY_START_AGAIN:
      // Possibly Mojo communication error. Try restart JDA and return error
      // this time.
      jda_available_ = jda_->Start();
      LOGF(WARNING) << "Restart JDA: " << jda_available_;
      return -EINVAL;
    default:
      // Other JDA errors.
      return -EINVAL;
  }
}

int CachedFrame::CropRotateScale(int rotate_degree) {
  if (yu12_frame_->GetHeight() > yu12_frame_->GetWidth()) {
    LOGF(ERROR) << "yu12_frame_ is tall frame already: "
                << yu12_frame_->GetWidth() << "x" << yu12_frame_->GetHeight();
    return -EINVAL;
  }

  // Step 1: Crop and rotate
  //
  //   Original frame                  Cropped frame              Rotated frame
  // --------------------               --------
  // |     |      |     |               |      |                 ---------------
  // |     |      |     |               |      |                 |             |
  // |     |      |     |   =======>>   |      |     =======>>   |             |
  // |     |      |     |               |      |                 ---------------
  // |     |      |     |               |      |
  // --------------------               --------
  //
  int cropped_width = yu12_frame_->GetHeight() * yu12_frame_->GetHeight() /
                      yu12_frame_->GetWidth();
  if (cropped_width % 2 == 1) {
    // Make cropped_width to the closest even number.
    cropped_width++;
  }
  int cropped_height = yu12_frame_->GetHeight();
  // SetWidth and SetHeight are for final image after crop and rotation.
  temp_frame_->SetWidth(cropped_height);
  temp_frame_->SetHeight(cropped_width);

  int ret = image_processor_->ProcessForInsetPortraitMode(
      *yu12_frame_.get(), temp_frame_.get(), rotate_degree);
  if (ret) {
    LOGF(ERROR) << "Crop and Rotate " << rotate_degree << " degree fails.";
    return ret;
  }

  // Step 2: Scale
  //
  //                               Final frame
  //  Rotated frame            ---------------------
  // --------------            |                   |
  // |            |  =====>>   |                   |
  // |            |            |                   |
  // --------------            |                   |
  //                           |                   |
  //                           ---------------------
  //

  ret = image_processor_->Scale(*temp_frame_.get(), yu12_frame_.get());
  LOGF_IF(ERROR, ret) << "Scale failed: " << ret;
  return ret;
}

int CachedFrame::ConvertYU12ToJpeg(const android::CameraMetadata& metadata,
                                   const FrameBuffer& in_frame,
                                   FrameBuffer* out_frame) {
  ExifUtils utils;
  if (!utils.Initialize()) {
    LOGF(ERROR) << "ExifUtils initialization failed.";
    return -ENODEV;
  }

  if (!SetExifTags(metadata, in_frame, &utils)) {
    LOGF(ERROR) << "Setting Exif tags failed.";
    return -EINVAL;
  }

  int jpeg_quality, thumbnail_jpeg_quality;
  camera_metadata_ro_entry entry = metadata.find(ANDROID_JPEG_QUALITY);
  if (entry.count) {
    jpeg_quality = entry.data.u8[0];
  } else {
    LOGF(ERROR) << "Cannot find jpeg quality in metadata.";
    return -EINVAL;
  }
  if (metadata.exists(ANDROID_JPEG_THUMBNAIL_QUALITY)) {
    entry = metadata.find(ANDROID_JPEG_THUMBNAIL_QUALITY);
    thumbnail_jpeg_quality = entry.data.u8[0];
  } else {
    thumbnail_jpeg_quality = jpeg_quality;
  }

  // Generate thumbnail
  std::vector<uint8_t> thumbnail;
  if (metadata.exists(ANDROID_JPEG_THUMBNAIL_SIZE)) {
    entry = metadata.find(ANDROID_JPEG_THUMBNAIL_SIZE);
    if (entry.count < 2) {
      LOGF(ERROR) << "Thumbnail size in metadata is not complete.";
      return -EINVAL;
    }
    int thumbnail_width = entry.data.i32[0];
    int thumbnail_height = entry.data.i32[1];
    if (thumbnail_width == 0 && thumbnail_height == 0) {
      LOGF(INFO) << "Thumbnail size = (0, 0), nothing will be generated";
    } else {
      uint32_t thumbnail_data_size = 0;
      thumbnail.resize(thumbnail_width * thumbnail_height * 1.5);
      if (jpeg_compressor_->GenerateThumbnail(
              in_frame.GetData(), in_frame.GetWidth(), in_frame.GetHeight(),
              thumbnail_width, thumbnail_height, thumbnail_jpeg_quality,
              thumbnail.size(), thumbnail.data(), &thumbnail_data_size)) {
        thumbnail.resize(thumbnail_data_size);
      } else {
        LOGF(WARNING) << "Generate JPEG thumbnail failed";
        thumbnail.clear();
      }
    }
  }

  // TODO(shik): Regenerate if thumbnail is too large.
  if (!utils.GenerateApp1(thumbnail.data(), thumbnail.size())) {
    LOGF(ERROR) << "Generating APP1 segment failed.";
    return -EINVAL;
  }

  uint32_t jpeg_data_size = 0;
  if (!jpeg_compressor_->CompressImage(
          in_frame.GetData(), in_frame.GetWidth(), in_frame.GetHeight(),
          jpeg_quality, utils.GetApp1Buffer(), utils.GetApp1Length(),
          out_frame->GetBufferSize(), out_frame->GetData(), &jpeg_data_size,
          force_jpeg_hw_encode_ ? JpegCompressor::Mode::kHwOnly
                                : JpegCompressor::Mode::kDefault)) {
    LOGF(ERROR) << "JPEG image compression failed";
    return -EINVAL;
  }
  InsertJpegBlob(out_frame, jpeg_data_size);
  return 0;
}

static void InsertJpegBlob(FrameBuffer* out_frame, uint32_t jpeg_data_size) {
  camera3_jpeg_blob_t blob;
  blob.jpeg_blob_id = CAMERA3_JPEG_BLOB_ID;
  blob.jpeg_size = jpeg_data_size;
  memcpy(out_frame->GetData() + out_frame->GetBufferSize() - sizeof(blob),
         &blob, sizeof(blob));
}

static bool SetExifTags(const android::CameraMetadata& metadata,
                        const FrameBuffer& in_frame,
                        ExifUtils* utils) {
  if (!utils->SetImageWidth(in_frame.GetWidth()) ||
      !utils->SetImageLength(in_frame.GetHeight())) {
    LOGF(ERROR) << "Setting image resolution failed.";
    return false;
  }

  struct timespec tp;
  struct tm time_info;
  bool time_available = clock_gettime(CLOCK_REALTIME, &tp) != -1;
  tzset();
  localtime_r(&tp.tv_sec, &time_info);
  if (!utils->SetDateTime(time_info)) {
    LOGF(ERROR) << "Setting data time failed.";
    return false;
  }

  if (metadata.exists(ANDROID_LENS_FOCAL_LENGTH)) {
    camera_metadata_ro_entry entry = metadata.find(ANDROID_LENS_FOCAL_LENGTH);
    float focal_length = entry.data.f[0];
    if (!utils->SetFocalLength(
            static_cast<uint32_t>(focal_length * kRationalPrecision),
            kRationalPrecision)) {
      LOGF(ERROR) << "Setting focal length failed.";
      return false;
    }
  } else {
    if (metadata.exists(ANDROID_LENS_FACING)) {
      camera_metadata_ro_entry entry = metadata.find(ANDROID_LENS_FACING);
      if (entry.data.u8[0] != ANDROID_LENS_FACING_EXTERNAL) {
        LOGF(ERROR)
            << "Cannot find focal length in metadata from a built-in camera.";
        return false;
      }
    } else {
      LOGF(WARNING) << "Cannot find focal length in metadata.";
    }
  }

  if (metadata.exists(ANDROID_JPEG_GPS_COORDINATES)) {
    camera_metadata_ro_entry entry =
        metadata.find(ANDROID_JPEG_GPS_COORDINATES);
    if (entry.count < 3) {
      LOGF(ERROR) << "Gps coordinates in metadata is not complete.";
      return false;
    }
    if (!utils->SetGpsLatitude(entry.data.d[0])) {
      LOGF(ERROR) << "Setting gps latitude failed.";
      return false;
    }
    if (!utils->SetGpsLongitude(entry.data.d[1])) {
      LOGF(ERROR) << "Setting gps longitude failed.";
      return false;
    }
    if (!utils->SetGpsAltitude(entry.data.d[2])) {
      LOGF(ERROR) << "Setting gps altitude failed.";
      return false;
    }
  }

  if (metadata.exists(ANDROID_JPEG_GPS_PROCESSING_METHOD)) {
    camera_metadata_ro_entry entry =
        metadata.find(ANDROID_JPEG_GPS_PROCESSING_METHOD);
    std::string method_str(reinterpret_cast<const char*>(entry.data.u8));
    if (!utils->SetGpsProcessingMethod(method_str)) {
      LOGF(ERROR) << "Setting gps processing method failed.";
      return false;
    }
  }

  if (time_available && metadata.exists(ANDROID_JPEG_GPS_TIMESTAMP)) {
    camera_metadata_ro_entry entry = metadata.find(ANDROID_JPEG_GPS_TIMESTAMP);
    time_t timestamp = static_cast<time_t>(entry.data.i64[0]);
    if (gmtime_r(&timestamp, &time_info)) {
      if (!utils->SetGpsTimestamp(time_info)) {
        LOGF(ERROR) << "Setting gps timestamp failed.";
        return false;
      }
    } else {
      LOGF(ERROR) << "Time tranformation failed.";
      return false;
    }
  }

  if (metadata.exists(ANDROID_JPEG_ORIENTATION)) {
    camera_metadata_ro_entry entry = metadata.find(ANDROID_JPEG_ORIENTATION);
    if (!utils->SetOrientation(entry.data.i32[0])) {
      LOGF(ERROR) << "Setting orientation failed.";
      return false;
    }
  }

  // TODO(henryhsu): Query device to know exposure time.
  // Currently set frame duration as default.
  if (!utils->SetExposureTime(1, 30)) {
    LOGF(ERROR) << "Setting exposure time failed.";
    return false;
  }

  if (metadata.exists(ANDROID_LENS_APERTURE)) {
    const int kAperturePrecision = 10000;
    camera_metadata_ro_entry entry = metadata.find(ANDROID_LENS_APERTURE);
    if (!utils->SetFNumber(entry.data.f[0] * kAperturePrecision,
                           kAperturePrecision)) {
      LOGF(ERROR) << "Setting F number failed.";
      return false;
    }
  }

  if (metadata.exists(ANDROID_FLASH_INFO_AVAILABLE)) {
    camera_metadata_ro_entry entry =
        metadata.find(ANDROID_FLASH_INFO_AVAILABLE);
    if (entry.data.u8[0] == ANDROID_FLASH_INFO_AVAILABLE_FALSE) {
      const uint32_t kNoFlashFunction = 0x20;
      if (!utils->SetFlash(kNoFlashFunction)) {
        LOGF(ERROR) << "Setting flash failed.";
        return false;
      }
    } else {
      LOGF(ERROR) << "Unsupported flash info: " << entry.data.u8[0];
      return false;
    }
  }

  if (metadata.exists(ANDROID_CONTROL_AWB_MODE)) {
    camera_metadata_ro_entry entry = metadata.find(ANDROID_CONTROL_AWB_MODE);
    if (entry.data.u8[0] == ANDROID_CONTROL_AWB_MODE_AUTO) {
      const uint16_t kAutoWhiteBalance = 0;
      if (!utils->SetWhiteBalance(kAutoWhiteBalance)) {
        LOGF(ERROR) << "Setting white balance failed.";
        return false;
      }
    } else {
      LOGF(ERROR) << "Unsupported awb mode: " << entry.data.u8[0];
      return false;
    }
  }

  if (time_available) {
    char str[4];
    if (snprintf(str, sizeof(str), "%03ld", tp.tv_nsec / 1000000) < 0) {
      LOGF(ERROR) << "Subsec is invalid: " << tp.tv_nsec;
      return false;
    }
    if (!utils->SetSubsecTime(std::string(str))) {
      LOGF(ERROR) << "Setting subsec time failed.";
      return false;
    }
  }

  return true;
}

}  // namespace cros
