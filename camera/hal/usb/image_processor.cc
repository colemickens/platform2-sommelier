/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/image_processor.h"

#include <errno.h>
#include <libyuv.h>
#include <time.h>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>

#include <cros-camera/constants.h>
#include <cros-camera/exif_utils.h>
#include <cros-camera/jpeg_compressor.h>
#include <hardware/camera3.h>

#include "cros-camera/common.h"
#include "hal/usb/common_types.h"

namespace cros {

/*
 * Formats have different names in different header files. Here is the mapping
 * table:
 *
 * android_pixel_format_t           videodev2.h            FOURCC in libyuv
 * -----------------------------------------------------------------------------
 * HAL_PIXEL_FORMAT_RGBA_8888     = V4L2_PIX_FMT_RGBX32  = FOURCC_ABGR
 * HAL_PIXEL_FORMAT_YCbCr_422_I   = V4L2_PIX_FMT_YUYV    = FOURCC_YUYV
 *                                                       = FOURCC_YUY2
 *                                  V4L2_PIX_FMT_YUV420  = FOURCC_I420
 *                                                       = FOURCC_YU12
 *                                  V4L2_PIX_FMT_MJPEG   = FOURCC_MJPG
 *
 * HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED and HAL_PIXEL_FORMAT_YCbCr_420_888
 * may be backed by different types of buffers depending on the platform.
 *
 * HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED
 *                                = V4L2_PIX_FMT_NV12    = FOURCC_NV12
 *                                = V4L2_PIX_FMT_RGBX32  = FOURCC_ABGR
 *
 * HAL_PIXEL_FORMAT_YCbCr_420_888 = V4L2_PIX_FMT_NV12    = FOURCC_NV12
 *                                = V4L2_PIX_FMT_YVU420  = FOURCC_YV12
 *
 * Camera device generates FOURCC_YUYV and FOURCC_MJPG.
 * At the Android side:
 * - Camera preview uses HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED buffers.
 * - Video recording uses HAL_PIXEL_FORMAT_YCbCr_420_888 buffers.
 * - Still capture uses HAL_PIXEL_FORMAT_BLOB buffers.
 * - CTS requires FOURCC_YV12 and FOURCC_NV21 for applications.
 *
 * Android stride requirement:
 * YV12 horizontal stride should be a multiple of 16 pixels. See
 * android.graphics.ImageFormat.YV12.
 * The stride of ARGB, YU12, and NV21 are always equal to the width.
 *
 * Conversion Path:
 * MJPG/YUYV (from camera) -> YU12 -> ARGB / NM12 (preview)
 *                                 -> NV21 (apps)
 *                                 -> YV12 (apps)
 *                                 -> NM12 / YV12 (video encoder)
 */

static bool SetExifTags(const android::CameraMetadata& metadata,
                        const FrameBuffer& in_frame,
                        ExifUtils* utils);

// How precise the float-to-rational conversion for EXIF tags would be.
static const int kRationalPrecision = 10000;

ImageProcessor::ImageProcessor() :
    jpeg_encoder_(nullptr), jpeg_encoder_started_(false) {
  const base::FilePath kCrosCameraTestModePath(
      constants::kCrosCameraTestModePathString);
  test_enabled_ = base::PathExists(kCrosCameraTestModePath);
  LOGF(INFO) << "Test mode enabled: " << test_enabled_;

  jda_ = JpegDecodeAccelerator::CreateInstance();
  jda_available_ = jda_->Start();
  LOGF(INFO) << "JDA Available: " << jda_available_;
}

ImageProcessor::~ImageProcessor() {}

size_t ImageProcessor::GetConvertedSize(const FrameBuffer& frame) {
  if ((frame.GetWidth() % 2) || (frame.GetHeight() % 2)) {
    LOGF(ERROR) << "Width or height is not even (" << frame.GetWidth() << " x "
                << frame.GetHeight() << ")";
    return 0;
  }

  switch (frame.GetFourcc()) {
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_YVU420M:  // YM21, multiple planes YV12
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:  // YM12, multiple planes YU12
      if (frame.GetNumPlanes() != 3) {
        LOGF(ERROR) << "Stride is not set correctly";
        return 0;
      }
      return frame.GetStride(FrameBuffer::YPLANE) * frame.GetHeight() +
             frame.GetStride(FrameBuffer::UPLANE) * frame.GetHeight() / 2 +
             frame.GetStride(FrameBuffer::VPLANE) * frame.GetHeight() / 2;
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:  // NV12, multiple planes
      if (frame.GetNumPlanes() != 2) {
        LOGF(ERROR) << "Stride is not set correctly";
        return 0;
      }
      return frame.GetStride(FrameBuffer::YPLANE) * frame.GetHeight() +
             frame.GetStride(FrameBuffer::UPLANE) * frame.GetHeight() / 2;
    case V4L2_PIX_FMT_RGBX32:
      return frame.GetStride() * frame.GetHeight();
    default:
      LOGF(ERROR) << "Pixel format " << FormatToString(frame.GetFourcc())
                  << " is unsupported.";
      return 0;
  }
}

int ImageProcessor::ConvertFormat(const android::CameraMetadata& metadata,
                                  const FrameBuffer& in_frame,
                                  FrameBuffer* out_frame) {
  if ((in_frame.GetWidth() % 2) || (in_frame.GetHeight() % 2)) {
    LOGF(ERROR) << "Width or height is not even (" << in_frame.GetWidth()
                << " x " << in_frame.GetHeight() << ")";
    return -EINVAL;
  }

  if (out_frame->GetFourcc() != V4L2_PIX_FMT_JPEG) {
    size_t data_size = GetConvertedSize(*out_frame);

    if (out_frame->SetDataSize(data_size)) {
      LOGF(ERROR) << "Set data size failed";
      return -EINVAL;
    }
  }

  VLOGF(1) << "Convert format from " << FormatToString(in_frame.GetFourcc())
           << " to " << FormatToString(out_frame->GetFourcc());

  if (in_frame.GetFourcc() == V4L2_PIX_FMT_YUYV) {
    switch (out_frame->GetFourcc()) {
      case V4L2_PIX_FMT_YUV420:   // YU12
      case V4L2_PIX_FMT_YUV420M:  // YM12, multiple planes YU12
      {
        int res =
            libyuv::YUY2ToI420(in_frame.GetData(), in_frame.GetWidth() * 2,
                               out_frame->GetData(FrameBuffer::YPLANE),
                               out_frame->GetStride(FrameBuffer::YPLANE),
                               out_frame->GetData(FrameBuffer::UPLANE),
                               out_frame->GetStride(FrameBuffer::UPLANE),
                               out_frame->GetData(FrameBuffer::VPLANE),
                               out_frame->GetStride(FrameBuffer::VPLANE),
                               in_frame.GetWidth(), in_frame.GetHeight());
        LOGF_IF(ERROR, res) << "YUY2ToI420() for YU12 returns " << res;
        return res ? -EINVAL : 0;
      }
      default:
        LOGF(ERROR) << "Destination pixel format "
                    << FormatToString(out_frame->GetFourcc())
                    << " is unsupported for YUYV source format.";
        return -EINVAL;
    }
  } else if (in_frame.GetFourcc() == V4L2_PIX_FMT_YUV420 ||
             in_frame.GetFourcc() == V4L2_PIX_FMT_YUV420M) {
    // V4L2_PIX_FMT_YVU420 is YV12. I420 is usually referred to YU12
    // (V4L2_PIX_FMT_YUV420), and YV12 is similar to YU12 except that U/V
    // planes are swapped.
    switch (out_frame->GetFourcc()) {
      case V4L2_PIX_FMT_YVU420:     // YV12
      case V4L2_PIX_FMT_YVU420M: {  // YM21, multiple planes YV12
        int res =
            libyuv::I420Copy(in_frame.GetData(FrameBuffer::YPLANE),
                             in_frame.GetStride(FrameBuffer::YPLANE),
                             in_frame.GetData(FrameBuffer::UPLANE),
                             in_frame.GetStride(FrameBuffer::UPLANE),
                             in_frame.GetData(FrameBuffer::VPLANE),
                             in_frame.GetStride(FrameBuffer::VPLANE),
                             out_frame->GetData(FrameBuffer::YPLANE),
                             out_frame->GetStride(FrameBuffer::YPLANE),
                             out_frame->GetData(FrameBuffer::UPLANE),
                             out_frame->GetStride(FrameBuffer::UPLANE),
                             out_frame->GetData(FrameBuffer::VPLANE),
                             out_frame->GetStride(FrameBuffer::VPLANE),
                             out_frame->GetWidth(), out_frame->GetHeight());
        LOGF_IF(ERROR, res) << "I420Copy() returns " << res;
        return res ? -EINVAL : 0;
      }
      case V4L2_PIX_FMT_YUV420: {  // YU12
        memcpy(out_frame->GetData(), in_frame.GetData(),
               in_frame.GetDataSize());
        return 0;
      }
      case V4L2_PIX_FMT_NV12:     // NV12
      case V4L2_PIX_FMT_NV12M: {  // NM12
        int res = libyuv::I420ToNV12(in_frame.GetData(FrameBuffer::YPLANE),
                                     in_frame.GetStride(FrameBuffer::YPLANE),
                                     in_frame.GetData(FrameBuffer::UPLANE),
                                     in_frame.GetStride(FrameBuffer::UPLANE),
                                     in_frame.GetData(FrameBuffer::VPLANE),
                                     in_frame.GetStride(FrameBuffer::VPLANE),
                                     out_frame->GetData(FrameBuffer::YPLANE),
                                     out_frame->GetStride(FrameBuffer::YPLANE),
                                     out_frame->GetData(FrameBuffer::UPLANE),
                                     out_frame->GetStride(FrameBuffer::UPLANE),
                                     in_frame.GetWidth(), in_frame.GetHeight());
        LOGF_IF(ERROR, res) << "I420ToNV12() returns " << res;
        return res ? -EINVAL : 0;
      }
      case V4L2_PIX_FMT_RGBX32: {
        int res =
            libyuv::I420ToABGR(in_frame.GetData(FrameBuffer::YPLANE),
                               in_frame.GetStride(FrameBuffer::YPLANE),
                               in_frame.GetData(FrameBuffer::UPLANE),
                               in_frame.GetStride(FrameBuffer::UPLANE),
                               in_frame.GetData(FrameBuffer::VPLANE),
                               in_frame.GetStride(FrameBuffer::VPLANE),
                               out_frame->GetData(), out_frame->GetWidth() * 4,
                               in_frame.GetWidth(), in_frame.GetHeight());
        LOGF_IF(ERROR, res) << "I420ToABGR() returns " << res;
        return res ? -EINVAL : 0;
      }
      case V4L2_PIX_FMT_JPEG: {
        bool res = ConvertToJpeg(metadata, in_frame, out_frame);
        LOGF_IF(ERROR, !res) << "ConvertToJpeg() failed";
        return res ? 0 : -EINVAL;
      }
      default:
        LOGF(ERROR) << "Destination pixel format "
                    << FormatToString(out_frame->GetFourcc())
                    << " is unsupported for YU12 source format.";
        return -EINVAL;
    }
  } else if (in_frame.GetFourcc() == V4L2_PIX_FMT_MJPEG) {
    switch (out_frame->GetFourcc()) {
      case V4L2_PIX_FMT_YUV420:     // YU12
      case V4L2_PIX_FMT_YUV420M: {  // YM12, multiple planes YU12
        int res = MJPGToI420(in_frame, out_frame);
        return res ? -EINVAL : 0;
      }
      default:
        LOGF(ERROR) << "Destination pixel format "
                    << FormatToString(out_frame->GetFourcc())
                    << " is unsupported for MJPEG source format.";
        return -EINVAL;
    }
  } else {
    LOGF(ERROR) << "Convert format doesn't support source format "
                << FormatToString(in_frame.GetFourcc());
    return -EINVAL;
  }
}

int ImageProcessor::Scale(const FrameBuffer& in_frame, FrameBuffer* out_frame) {
  if (in_frame.GetFourcc() != V4L2_PIX_FMT_YUV420 &&
      in_frame.GetFourcc() != V4L2_PIX_FMT_YUV420M) {
    LOGF(ERROR) << "Pixel format " << FormatToString(in_frame.GetFourcc())
                << " is unsupported.";
    return -EINVAL;
  }

  out_frame->SetFourcc(in_frame.GetFourcc());
  size_t data_size = GetConvertedSize(*out_frame);

  if (out_frame->SetDataSize(data_size)) {
    LOGF(ERROR) << "Set data size failed";
    return -EINVAL;
  }

  VLOGF(1) << "Scale image from " << in_frame.GetWidth() << "x"
           << in_frame.GetHeight() << " to " << out_frame->GetWidth() << "x"
           << out_frame->GetHeight();

  int ret = libyuv::I420Scale(
      in_frame.GetData(), in_frame.GetWidth(),
      in_frame.GetData() + in_frame.GetWidth() * in_frame.GetHeight(),
      in_frame.GetWidth() / 2,
      in_frame.GetData() + in_frame.GetWidth() * in_frame.GetHeight() * 5 / 4,
      in_frame.GetWidth() / 2, in_frame.GetWidth(), in_frame.GetHeight(),
      out_frame->GetData(), out_frame->GetWidth(),
      out_frame->GetData() + out_frame->GetWidth() * out_frame->GetHeight(),
      out_frame->GetWidth() / 2,
      out_frame->GetData() +
          out_frame->GetWidth() * out_frame->GetHeight() * 5 / 4,
      out_frame->GetWidth() / 2, out_frame->GetWidth(), out_frame->GetHeight(),
      libyuv::FilterMode::kFilterNone);
  LOGF_IF(ERROR, ret) << "I420Scale failed: " << ret;
  return ret;
}

int ImageProcessor::CropAndRotate(const FrameBuffer& in_frame,
                                  FrameBuffer* out_frame,
                                  int rotate_degree) {
  if (in_frame.GetFourcc() != V4L2_PIX_FMT_YUV420 &&
      in_frame.GetFourcc() != V4L2_PIX_FMT_YUV420M) {
    LOGF(ERROR) << "Pixel format " << FormatToString(in_frame.GetFourcc())
                << " is unsupported.";
    return -EINVAL;
  }

  libyuv::RotationMode rotation_mode = libyuv::RotationMode::kRotate90;
  switch (rotate_degree) {
    case 90:
      rotation_mode = libyuv::RotationMode::kRotate90;
      break;
    case 270:
      rotation_mode = libyuv::RotationMode::kRotate270;
      break;
    default:
      LOGF(ERROR) << "Invalid rotation degree: " << rotate_degree;
      return -EINVAL;
  }

  out_frame->SetFourcc(in_frame.GetFourcc());
  size_t data_size = GetConvertedSize(*out_frame);

  if (out_frame->SetDataSize(data_size)) {
    LOGF(ERROR) << "Set data size failed";
    return -EINVAL;
  }

  VLOGF(1) << "Crop and rotate image, rotate degree: " << rotate_degree;

  // This libyuv method first crops the frame and then rotates it 90 degrees
  // clockwise or counterclockwise.
  int margin = (in_frame.GetWidth() - out_frame->GetHeight()) / 2;
  int ret = libyuv::ConvertToI420(
      in_frame.GetData(), in_frame.GetDataSize(), out_frame->GetData(),
      out_frame->GetWidth(),
      out_frame->GetData() + out_frame->GetWidth() * out_frame->GetHeight(),
      out_frame->GetWidth() / 2,
      out_frame->GetData() +
          out_frame->GetWidth() * out_frame->GetHeight() * 5 / 4,
      out_frame->GetWidth() / 2, margin, 0, in_frame.GetWidth(),
      in_frame.GetHeight(), out_frame->GetHeight(), out_frame->GetWidth(),
      rotation_mode, libyuv::FourCC::FOURCC_I420);

  LOGF_IF(ERROR, ret) << "ConvertToI420 failed: " << ret;
  return ret;
}

int ImageProcessor::MJPGToI420(const FrameBuffer& in_frame,
                               FrameBuffer* out_frame) {
  if (jda_available_) {
    int input_fd = in_frame.GetFd();
    int output_fd = out_frame->GetFd();
    if (input_fd > 0 && output_fd > 0) {
      JpegDecodeAccelerator::Error error = jda_->DecodeSync(
          input_fd, in_frame.GetDataSize(), in_frame.GetWidth(),
          in_frame.GetHeight(), output_fd, out_frame->GetBufferSize());
      if (error == JpegDecodeAccelerator::Error::NO_ERRORS)
        return 0;
      if (error == JpegDecodeAccelerator::Error::TRY_START_AGAIN) {
        LOGF(WARNING)
            << "Restart JDA, possibly due to Mojo communication error";
        // If we can't Start JDA successfully, we just consider that we have no
        // JDA.
        jda_available_ = jda_->Start();
      }
      LOGF(WARNING) << "JDA Fail: " << error;
      // Don't fallback in test mode. So we can know the JDA is not working.
      if (test_enabled_)
        return -EINVAL;
    }
  }

  int res = libyuv::MJPGToI420(in_frame.GetData(), in_frame.GetDataSize(),
                               out_frame->GetData(FrameBuffer::YPLANE),
                               out_frame->GetStride(FrameBuffer::YPLANE),
                               out_frame->GetData(FrameBuffer::UPLANE),
                               out_frame->GetStride(FrameBuffer::UPLANE),
                               out_frame->GetData(FrameBuffer::VPLANE),
                               out_frame->GetStride(FrameBuffer::VPLANE),
                               in_frame.GetWidth(), in_frame.GetHeight(),
                               out_frame->GetWidth(), out_frame->GetHeight());
  LOGF_IF(ERROR, res) << "libyuv::MJPEGToI420() returns " << res;

  return res;
}

bool ImageProcessor::ConvertToJpeg(const android::CameraMetadata& metadata,
                                   const FrameBuffer& in_frame,
                                   FrameBuffer* out_frame) {
  ExifUtils utils;
  if (!utils.Initialize()) {
    LOGF(ERROR) << "ExifUtils initialization failed.";
    return false;
  }

  if (!SetExifTags(metadata, in_frame, &utils)) {
    LOGF(ERROR) << "Setting Exif tags failed.";
    return false;
  }

  int jpeg_quality, thumbnail_jpeg_quality;
  camera_metadata_ro_entry entry = metadata.find(ANDROID_JPEG_QUALITY);
  if (entry.count) {
    jpeg_quality = entry.data.u8[0];
  } else {
    LOGF(ERROR) << "Cannot find jpeg quality in metadata.";
    return false;
  }
  if (metadata.exists(ANDROID_JPEG_THUMBNAIL_QUALITY)) {
    entry = metadata.find(ANDROID_JPEG_THUMBNAIL_QUALITY);
    thumbnail_jpeg_quality = entry.data.u8[0];
  } else {
    thumbnail_jpeg_quality = jpeg_quality;
  }

  JpegCompressor compressor;

  // Generate thumbnail
  std::vector<uint8_t> thumbnail;
  if (metadata.exists(ANDROID_JPEG_THUMBNAIL_SIZE)) {
    entry = metadata.find(ANDROID_JPEG_THUMBNAIL_SIZE);
    if (entry.count < 2) {
      LOGF(ERROR) << "Thumbnail size in metadata is not complete.";
      return false;
    }
    int thumbnail_width = entry.data.i32[0];
    int thumbnail_height = entry.data.i32[1];
    if (thumbnail_width == 0 && thumbnail_height == 0) {
      LOGF(INFO) << "Thumbnail size = (0, 0), nothing will be generated";
    } else {
      uint32_t thumbnail_data_size = 0;
      thumbnail.resize(thumbnail_width * thumbnail_height * 1.5);
      if (compressor.GenerateThumbnail(
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
    return false;
  }

  if (!jpeg_encoder_) {
    jpeg_encoder_ = JpegEncodeAccelerator::CreateInstance();
    jpeg_encoder_started_ = jpeg_encoder_->Start();
  }

  if (jpeg_encoder_ && jpeg_encoder_started_) {
    // Create SharedMemory for output buffer.
    std::unique_ptr<base::SharedMemory> output_shm =
        base::WrapUnique(new base::SharedMemory);
    if (!output_shm->CreateAndMapAnonymous(out_frame->GetBufferSize())) {
      LOGF(WARNING) << "CreateAndMapAnonymous for output buffer failed, size="
          << out_frame->GetBufferSize();
      return false;
    }

    // Utilize HW Jpeg encode through IPC.
    uint32_t encoded_data_size = 0;
    int status = jpeg_encoder_->EncodeSync(
        in_frame.GetFd(), in_frame.GetDataSize(),
        in_frame.GetWidth(), in_frame.GetHeight(),
        utils.GetApp1Buffer(), utils.GetApp1Length(),
        output_shm->handle().fd, out_frame->GetBufferSize(),
        &encoded_data_size);
    if (status == JpegEncodeAccelerator::TRY_START_AGAIN) {
      // There might be some mojo errors. We will give a second try.
      // But if it fails again, we will fall back to SW encode.
      LOG(WARNING) << "EncodeSync() returns TRY_START_AGAIN.";
      jpeg_encoder_started_ = jpeg_encoder_->Start();
      if (jpeg_encoder_started_) {
        status = jpeg_encoder_->EncodeSync(
            in_frame.GetFd(), in_frame.GetDataSize(),
            in_frame.GetWidth(), in_frame.GetHeight(),
            utils.GetApp1Buffer(), utils.GetApp1Length(),
            output_shm->handle().fd, out_frame->GetBufferSize(),
            &encoded_data_size);
      } else {
        LOG(ERROR) << "JPEG encode accelerator can't be started.";
      }
    }
    if (status == JpegEncodeAccelerator::ENCODE_OK) {
      memcpy(out_frame->GetData(), output_shm->memory(), encoded_data_size);
      InsertJpegBlob(out_frame, encoded_data_size);
      return true;
    }

    LOG(ERROR) << "JEA returns " << status << ". Fall back to SW encode.";
  }

  if (test_enabled_) {
    // In test mode, don't fall back to SW encode.
    LOG(ERROR) << "Test is enabled and JEA failed.";
    return false;
  }

  uint32_t jpeg_data_size = 0;
  if (!compressor.CompressImage(
          in_frame.GetData(), in_frame.GetWidth(), in_frame.GetHeight(),
          jpeg_quality, utils.GetApp1Buffer(), utils.GetApp1Length(),
          out_frame->GetBufferSize(), out_frame->GetData(), &jpeg_data_size)) {
    LOGF(ERROR) << "JPEG image compression failed";
    return false;
  }
  InsertJpegBlob(out_frame, jpeg_data_size);
  return true;
}

void ImageProcessor::InsertJpegBlob(FrameBuffer* out_frame, uint32_t jpeg_data_size) {
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
  localtime_r(&tp.tv_sec, &time_info);
  if (!utils->SetDateTime(time_info)) {
    LOGF(ERROR) << "Setting data time failed.";
    return false;
  }

  float focal_length;
  camera_metadata_ro_entry entry = metadata.find(ANDROID_LENS_FOCAL_LENGTH);
  if (entry.count) {
    focal_length = entry.data.f[0];
  } else {
    LOGF(ERROR) << "Cannot find focal length in metadata.";
    return false;
  }
  if (!utils->SetFocalLength(
          static_cast<uint32_t>(focal_length * kRationalPrecision),
          kRationalPrecision)) {
    LOGF(ERROR) << "Setting focal length failed.";
    return false;
  }

  if (metadata.exists(ANDROID_JPEG_GPS_COORDINATES)) {
    entry = metadata.find(ANDROID_JPEG_GPS_COORDINATES);
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
    entry = metadata.find(ANDROID_JPEG_GPS_PROCESSING_METHOD);
    std::string method_str(reinterpret_cast<const char*>(entry.data.u8));
    if (!utils->SetGpsProcessingMethod(method_str)) {
      LOGF(ERROR) << "Setting gps processing method failed.";
      return false;
    }
  }

  if (time_available && metadata.exists(ANDROID_JPEG_GPS_TIMESTAMP)) {
    entry = metadata.find(ANDROID_JPEG_GPS_TIMESTAMP);
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
    entry = metadata.find(ANDROID_JPEG_ORIENTATION);
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
    entry = metadata.find(ANDROID_LENS_APERTURE);
    if (!utils->SetFNumber(entry.data.f[0] * kAperturePrecision,
                           kAperturePrecision)) {
      LOGF(ERROR) << "Setting F number failed.";
      return false;
    }
  }

  if (metadata.exists(ANDROID_FLASH_INFO_AVAILABLE)) {
    entry = metadata.find(ANDROID_FLASH_INFO_AVAILABLE);
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
    entry = metadata.find(ANDROID_CONTROL_AWB_MODE);
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
    if (snprintf(str, sizeof(str), "%03d", tp.tv_nsec / 1000000) < 0) {
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
