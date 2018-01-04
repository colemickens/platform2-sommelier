// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera3_test/camera3_frame_fixture.h"

#include <linux/videodev2.h>
#include <semaphore.h>
#include <stdio.h>

#include <limits>
#include <list>

#include <base/command_line.h>
#include <base/files/file_util.h>
#include <base/macros.h>
#include <base/strings/string_split.h>
#include <jpeglib.h>
#include <libyuv.h>

namespace camera3_test {

int32_t Camera3FrameFixture::CreateCaptureRequest(
    const camera_metadata_t& metadata,
    uint32_t* frame_number) {
  // Allocate output buffers
  std::vector<camera3_stream_buffer_t> output_buffers;
  if (cam_device_.AllocateOutputStreamBuffers(&output_buffers)) {
    ADD_FAILURE() << "Failed to allocate buffers for capture request";
    return -EINVAL;
  }

  camera3_capture_request_t capture_request = {
      .frame_number = UINT32_MAX,
      .settings = &metadata,
      .input_buffer = NULL,
      .num_output_buffers = static_cast<uint32_t>(output_buffers.size()),
      .output_buffers = output_buffers.data()};

  // Process capture request
  int32_t ret = cam_device_.ProcessCaptureRequest(&capture_request);
  if (ret == 0 && frame_number) {
    *frame_number = capture_request.frame_number;
  }
  return ret;
}

int32_t Camera3FrameFixture::CreateCaptureRequestByMetadata(
    const CameraMetadataUniquePtr& metadata,
    uint32_t* frame_number) {
  return CreateCaptureRequest(*metadata, frame_number);
}

int32_t Camera3FrameFixture::CreateCaptureRequestByTemplate(
    int32_t type,
    uint32_t* frame_number) {
  const camera_metadata_t* default_settings;
  default_settings = cam_device_.ConstructDefaultRequestSettings(type);
  if (!default_settings) {
    ADD_FAILURE() << "Camera default settings are NULL";
    return -EINVAL;
  }

  return CreateCaptureRequest(*default_settings, frame_number);
}

void Camera3FrameFixture::WaitShutterAndCaptureResult(
    const struct timespec& timeout) {
  ASSERT_EQ(0, cam_device_.WaitShutter(timeout))
      << "Timeout waiting for shutter callback";
  ASSERT_EQ(0, cam_device_.WaitCaptureResult(timeout))
      << "Timeout waiting for capture result callback";
}

std::vector<int32_t>
Camera3FrameFixture::GetAvailableColorBarsTestPatternModes() {
  std::vector<int32_t> test_pattern_modes;
  if (cam_device_.GetStaticInfo()->GetAvailableTestPatternModes(
          &test_pattern_modes) != 0) {
    ADD_FAILURE() << "Failed to get sensor available test pattern modes";
    return std::vector<int32_t>();
  }
  std::vector<int32_t> result;
  for (const auto& it : supported_color_bars_test_pattern_modes_) {
    if (std::find(test_pattern_modes.begin(), test_pattern_modes.end(), it) !=
        test_pattern_modes.end()) {
      result.push_back(it);
    }
  }
  return result;
}

Camera3FrameFixture::ImagePlane::ImagePlane(uint32_t stride,
                                            uint32_t size,
                                            uint8_t* addr)
    : stride(stride), size(size), addr(addr) {}

#define DIV_ROUND_UP(n, d) (((n) + (d)-1) / (d))
Camera3FrameFixture::Image::Image(uint32_t w, uint32_t h, ImageFormat f)
    : width(w), height(h), format(f) {
  if (format == ImageFormat::IMAGE_FORMAT_ARGB) {
    size = w * h * kARGBPixelWidth;
    data.resize(size);
    planes.emplace_back(w * kARGBPixelWidth, size, data.data());
  } else if (format == ImageFormat::IMAGE_FORMAT_I420) {
    uint32_t cstride = DIV_ROUND_UP(w, 2);
    size = w * h + cstride * DIV_ROUND_UP(h, 2) * 2;
    uint32_t uv_plane_size = cstride * DIV_ROUND_UP(h, 2);
    data.resize(size);
    planes.emplace_back(w, w * h, data.data());  // y
    planes.emplace_back(cstride, uv_plane_size,
                        planes.back().addr + planes.back().size);  // u
    planes.emplace_back(cstride, uv_plane_size,
                        planes.back().addr + planes.back().size);  // v
  }
}

int Camera3FrameFixture::Image::SaveToFile(const std::string filename) const {
  const char* suffix =
      (format == ImageFormat::IMAGE_FORMAT_ARGB) ? ".argb" : ".i420";
  base::FilePath file_path(filename + suffix);
  if (base::WriteFile(file_path, reinterpret_cast<const char*>(data.data()),
                      size) != size) {
    LOGF(ERROR) << "Failed to write file " << filename << suffix;
    return -EINVAL;
  }
  return 0;
}

Camera3FrameFixture::ImageUniquePtr Camera3FrameFixture::ConvertToImage(
    BufferHandleUniquePtr buffer,
    uint32_t width,
    uint32_t height,
    ImageFormat format) {
  if (!buffer || format >= ImageFormat::IMAGE_FORMAT_END) {
    LOGF(ERROR) << "Invalid input buffer or format";
    return ImageUniquePtr(nullptr);
  }
  buffer_handle_t handle = *buffer;
  auto hnd = camera_buffer_handle_t::FromBufferHandle(handle);
  if (!hnd || hnd->buffer_id == 0) {
    LOGF(ERROR) << "Invalid input buffer handle";
    return ImageUniquePtr(nullptr);
  }
  ImageUniquePtr out_buffer(new Image(width, height, format));
  auto gralloc = Camera3TestGralloc::GetInstance();
  if (gralloc->GetFormat(handle) == HAL_PIXEL_FORMAT_BLOB) {
    size_t jpeg_max_size = cam_device_.GetStaticInfo()->GetJpegMaxSize();
    void* buf_addr = nullptr;
    if (gralloc->Lock(handle, 0, 0, 0, jpeg_max_size, 1, &buf_addr) != 0 ||
        !buf_addr) {
      LOG(ERROR) << "Failed to lock input buffer";
      return ImageUniquePtr(nullptr);
    }
    auto jpeg_blob = reinterpret_cast<camera3_jpeg_blob_t*>(
        static_cast<uint8_t*>(buf_addr) + jpeg_max_size -
        sizeof(camera3_jpeg_blob_t));
    if (static_cast<void*>(jpeg_blob) < buf_addr ||
        jpeg_blob->jpeg_blob_id != CAMERA3_JPEG_BLOB_ID) {
      gralloc->Unlock(handle);
      LOG(ERROR) << "Invalid JPEG BLOB ID";
      return ImageUniquePtr(nullptr);
    }
    if ((format == ImageFormat::IMAGE_FORMAT_I420 &&
         libyuv::MJPGToI420(
             static_cast<uint8_t*>(buf_addr), jpeg_blob->jpeg_size,
             out_buffer->planes[0].addr, out_buffer->planes[0].stride,
             out_buffer->planes[1].addr, out_buffer->planes[1].stride,
             out_buffer->planes[2].addr, out_buffer->planes[2].stride, width,
             height, width, height) != 0) ||
        (format == ImageFormat::IMAGE_FORMAT_ARGB &&
         libyuv::MJPGToARGB(static_cast<uint8_t*>(buf_addr),
                            jpeg_blob->jpeg_size, out_buffer->planes[0].addr,
                            out_buffer->planes[0].stride, width, height, width,
                            height) != 0)) {
      LOG(ERROR) << "Failed to convert image from JPEG";
      out_buffer.reset();
    }
    gralloc->Unlock(handle);
  } else {
    struct android_ycbcr in_ycbcr_info;
    if (gralloc->LockYCbCr(handle, 0, 0, 0, width, height, &in_ycbcr_info) !=
        0) {
      LOG(ERROR) << "Failed to lock input buffer";
      return ImageUniquePtr(nullptr);
    }
    uint32_t v4l2_format = arc::CameraBufferManager::GetV4L2PixelFormat(handle);
    switch (v4l2_format) {
      case V4L2_PIX_FMT_NV12:
      case V4L2_PIX_FMT_NV12M:
        if ((format == ImageFormat::IMAGE_FORMAT_I420 &&
             libyuv::NV12ToI420(
                 static_cast<uint8_t*>(in_ycbcr_info.y), in_ycbcr_info.ystride,
                 static_cast<uint8_t*>(in_ycbcr_info.cb), in_ycbcr_info.cstride,
                 out_buffer->planes[0].addr, out_buffer->planes[0].stride,
                 out_buffer->planes[1].addr, out_buffer->planes[1].stride,
                 out_buffer->planes[2].addr, out_buffer->planes[2].stride,
                 width, height) != 0) ||
            (format == ImageFormat::IMAGE_FORMAT_ARGB &&
             libyuv::NV12ToARGB(
                 static_cast<uint8_t*>(in_ycbcr_info.y), in_ycbcr_info.ystride,
                 static_cast<uint8_t*>(in_ycbcr_info.cb), in_ycbcr_info.cstride,
                 out_buffer->planes[0].addr, out_buffer->planes[0].stride,
                 width, height) != 0)) {
          LOG(ERROR) << "Failed to convert image from NV12";
          out_buffer.reset();
        }
        break;
      case V4L2_PIX_FMT_NV21:
      case V4L2_PIX_FMT_NV21M:
        if ((format == ImageFormat::IMAGE_FORMAT_I420 &&
             libyuv::NV21ToI420(
                 static_cast<uint8_t*>(in_ycbcr_info.y), in_ycbcr_info.ystride,
                 static_cast<uint8_t*>(in_ycbcr_info.cr), in_ycbcr_info.cstride,
                 out_buffer->planes[0].addr, out_buffer->planes[0].stride,
                 out_buffer->planes[1].addr, out_buffer->planes[1].stride,
                 out_buffer->planes[2].addr, out_buffer->planes[2].stride,
                 width, height) != 0) ||
            (format == ImageFormat::IMAGE_FORMAT_ARGB &&
             libyuv::NV21ToARGB(
                 static_cast<uint8_t*>(in_ycbcr_info.y), in_ycbcr_info.ystride,
                 static_cast<uint8_t*>(in_ycbcr_info.cr), in_ycbcr_info.cstride,
                 out_buffer->planes[0].addr, out_buffer->planes[0].stride,
                 width, height) != 0)) {
          LOG(ERROR) << "Failed to convert image from NV21";
          out_buffer.reset();
        }
        break;
      case V4L2_PIX_FMT_YUV420:
      case V4L2_PIX_FMT_YUV420M:
      case V4L2_PIX_FMT_YVU420:
      case V4L2_PIX_FMT_YVU420M:
        if ((format == ImageFormat::IMAGE_FORMAT_I420 &&
             libyuv::I420Copy(
                 static_cast<uint8_t*>(in_ycbcr_info.y), in_ycbcr_info.ystride,
                 static_cast<uint8_t*>(in_ycbcr_info.cb), in_ycbcr_info.cstride,
                 static_cast<uint8_t*>(in_ycbcr_info.cr), in_ycbcr_info.cstride,
                 out_buffer->planes[0].addr, out_buffer->planes[0].stride,
                 out_buffer->planes[1].addr, out_buffer->planes[1].stride,
                 out_buffer->planes[2].addr, out_buffer->planes[2].stride,
                 width, height) != 0) ||
            (format == ImageFormat::IMAGE_FORMAT_ARGB &&
             libyuv::I420ToARGB(
                 static_cast<uint8_t*>(in_ycbcr_info.y), in_ycbcr_info.ystride,
                 static_cast<uint8_t*>(in_ycbcr_info.cb), in_ycbcr_info.cstride,
                 static_cast<uint8_t*>(in_ycbcr_info.cr), in_ycbcr_info.cstride,
                 out_buffer->planes[0].addr, out_buffer->planes[0].stride,
                 width, height) != 0)) {
          LOG(ERROR) << "Failed to convert image from YUV420 or YVU420";
          out_buffer.reset();
        }
        break;
      default:
        LOGF(ERROR) << "Unsupported format " << FormatToString(v4l2_format);
        out_buffer.reset();
    }
    gralloc->Unlock(handle);
  }
  return out_buffer;
}

Camera3FrameFixture::ImageUniquePtr
Camera3FrameFixture::GenerateColorBarsPattern(
    uint32_t width,
    uint32_t height,
    ImageFormat format,
    const std::vector<std::tuple<uint8_t, uint8_t, uint8_t>>&
        color_bars_pattern,
    int32_t color_bars_pattern_mode) {
  if (format >= ImageFormat::IMAGE_FORMAT_END ||
      std::find(supported_color_bars_test_pattern_modes_.begin(),
                supported_color_bars_test_pattern_modes_.end(),
                color_bars_pattern_mode) ==
          supported_color_bars_test_pattern_modes_.end()) {
    return nullptr;
  }
  ImageUniquePtr argb_image(
      new Image(width, height, ImageFormat::IMAGE_FORMAT_ARGB));
  uint8_t* pdata = argb_image->planes[0].addr;
  int color_bar_width = width / color_bars_pattern.size();
  int color_bar_height = height / 128 * 128;
  if (color_bar_height == 0) {
    color_bar_height = height;
  }
  for (size_t h = 0; h < height; h++) {
    float gray_factor =
        static_cast<float>(color_bar_height - (h % color_bar_height)) /
        color_bar_height;
    for (size_t w = 0; w < width; w++) {
      int index = (w / color_bar_width) % color_bars_pattern.size();
      auto get_fade_color = [&](uint8_t base_color) {
        if (color_bars_pattern_mode ==
            ANDROID_SENSOR_TEST_PATTERN_MODE_COLOR_BARS) {
          return base_color;
        }
        uint8_t color = base_color * gray_factor;
        if ((w / (color_bar_width / 2)) % 2) {
          color = (color & 0xF0) | (color >> 4);
        }
        return color;
      };
      *pdata++ = get_fade_color(std::get<2>(color_bars_pattern[index]));  // B
      *pdata++ = get_fade_color(std::get<1>(color_bars_pattern[index]));  // G
      *pdata++ = get_fade_color(std::get<0>(color_bars_pattern[index]));  // R
      *pdata++ = 0x00;
    }
  }

  if (format == ImageFormat::IMAGE_FORMAT_I420) {
    ImageUniquePtr i420_image(new Image(width, height, format));
    libyuv::ARGBToI420(argb_image->planes[0].addr, argb_image->planes[0].stride,
                       i420_image->planes[0].addr, i420_image->planes[0].stride,
                       i420_image->planes[1].addr, i420_image->planes[1].stride,
                       i420_image->planes[2].addr, i420_image->planes[2].stride,
                       width, height);
    return i420_image;
  }
  return argb_image;
}

double Camera3FrameFixture::ComputeSsim(const Image& buffer_a,
                                        const Image& buffer_b) {
  if (buffer_a.format != ImageFormat::IMAGE_FORMAT_I420 ||
      buffer_b.format != ImageFormat::IMAGE_FORMAT_I420 ||
      buffer_a.width != buffer_b.width || buffer_a.height != buffer_b.height) {
    LOGF(ERROR) << "Images are not of I420 format or resolutions do not match";
    return 0.0;
  }
  return libyuv::I420Ssim(buffer_a.planes[0].addr, buffer_a.planes[0].stride,
                          buffer_a.planes[1].addr, buffer_a.planes[1].stride,
                          buffer_a.planes[2].addr, buffer_a.planes[2].stride,
                          buffer_b.planes[0].addr, buffer_b.planes[0].stride,
                          buffer_b.planes[1].addr, buffer_b.planes[1].stride,
                          buffer_b.planes[2].addr, buffer_b.planes[2].stride,
                          buffer_a.width, buffer_a.height);
}

// Get real-time clock time after waiting for given timeout
static void GetTimeOfTimeout(int32_t ms, struct timespec* ts) {
  memset(ts, 0, sizeof(*ts));
  if (clock_gettime(CLOCK_REALTIME, ts)) {
    LOG(ERROR) << "Failed to get clock time";
  }
  ts->tv_sec += ms / 1000;
  ts->tv_nsec += (ms % 1000) * 1000;
}

// Test parameters:
// - Camera ID
// - Template ID
// - Frame format
// - If true, capture with the maximum resolution supported for this format;
// otherwise, capture the minimum one.
class Camera3SingleFrameTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<
          std::tuple<int32_t, int32_t, int32_t, bool>> {
 public:
  Camera3SingleFrameTest() : Camera3FrameFixture(std::get<0>(GetParam())) {}
};

TEST_P(Camera3SingleFrameTest, GetFrame) {
  int32_t format = std::get<1>(GetParam());
  int32_t type = std::get<2>(GetParam());
  if (!cam_device_.IsTemplateSupported(type)) {
    return;
  }

  if (cam_device_.GetStaticInfo()->IsFormatAvailable(format)) {
    ResolutionInfo resolution(0, 0);
    if (std::get<3>(GetParam())) {
      ASSERT_EQ(0, GetMaxResolution(format, &resolution))
          << "Failed to get max resolution for format " << format;
    } else {
      ASSERT_EQ(0, GetMinResolution(format, &resolution))
          << "Failed to get min resolution for format " << format;
    }
    VLOGF(1) << "Device " << cam_id_;
    VLOGF(1) << "Format 0x" << std::hex << format;
    VLOGF(1) << "Resolution " << resolution.Width() << "x"
             << resolution.Height();

    cam_device_.AddOutputStream(format, resolution.Width(), resolution.Height(),
                                CAMERA3_STREAM_ROTATION_0);
    ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
        << "Configuring stream fails";

    ASSERT_EQ(0, CreateCaptureRequestByTemplate(type, nullptr))
        << "Creating capture request fails";

    struct timespec timeout;
    GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
    WaitShutterAndCaptureResult(timeout);
  }
}

// Test parameters:
// - Camera ID
// - Template ID
// - Number of frames to capture
class Camera3MultiFrameTest : public Camera3FrameFixture,
                              public ::testing::WithParamInterface<
                                  std::tuple<int32_t, int32_t, int32_t>> {
 public:
  Camera3MultiFrameTest() : Camera3FrameFixture(std::get<0>(GetParam())) {}
};

TEST_P(Camera3MultiFrameTest, GetFrame) {
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_,
                              CAMERA3_STREAM_ROTATION_0);
  std::vector<const camera3_stream_t*> streams;
  ASSERT_EQ(0, cam_device_.ConfigureStreams(&streams))
      << "Configuring stream fails";
  ASSERT_EQ(1, streams.size());
  int32_t stream_queue_depth = static_cast<int32_t>(streams[0]->max_buffers);

  int32_t type = std::get<1>(GetParam());
  if (!cam_device_.IsTemplateSupported(type)) {
    return;
  }

  int32_t num_frames = std::get<2>(GetParam());
  struct timespec timeout;
  for (int32_t i = 0; i < num_frames; i++) {
    GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
    if (i >= stream_queue_depth) {
      WaitShutterAndCaptureResult(timeout);
    }
    EXPECT_EQ(0, CreateCaptureRequestByTemplate(type, nullptr))
        << "Creating capture request fails";
  }

  for (int32_t i = 0; i < std::min(num_frames, stream_queue_depth); i++) {
    WaitShutterAndCaptureResult(timeout);
  }
}

// Test parameters:
// - Camera ID
class Camera3MixedTemplateMultiFrameTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<int32_t> {
 public:
  Camera3MixedTemplateMultiFrameTest() : Camera3FrameFixture(GetParam()) {}
};

TEST_P(Camera3MixedTemplateMultiFrameTest, GetFrame) {
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_,
                              CAMERA3_STREAM_ROTATION_0);
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";

  int32_t types[] = {CAMERA3_TEMPLATE_PREVIEW, CAMERA3_TEMPLATE_STILL_CAPTURE,
                     CAMERA3_TEMPLATE_VIDEO_RECORD,
                     CAMERA3_TEMPLATE_VIDEO_SNAPSHOT};
  for (size_t i = 0; i < arraysize(types); ++i) {
    EXPECT_EQ(0, CreateCaptureRequestByTemplate(types[i], nullptr))
        << "Creating capture request fails";
  }

  struct timespec timeout;
  for (size_t i = 0; i < arraysize(types); ++i) {
    GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
    WaitShutterAndCaptureResult(timeout);
  }
}

// Test parameters:
// - Camera ID
// - Template ID
// - Number of frames to capture
class Camera3FlushRequestsTest : public Camera3FrameFixture,
                                 public ::testing::WithParamInterface<
                                     std::tuple<int32_t, int32_t, int32_t>> {
 public:
  Camera3FlushRequestsTest()
      : Camera3FrameFixture(std::get<0>(GetParam())), num_capture_results_(0) {}

  void SetUp() override;

 protected:
  // Callback functions from HAL device
  virtual void ProcessCaptureResult(const camera3_capture_result* result);

  // Callback functions from HAL device
  // Do nothing.
  virtual void Notify(const camera3_notify_msg* msg) {}

  // Number of received capture results with all output buffers returned
  int32_t num_capture_results_;

  sem_t flush_result_sem_;

 private:
  // Number of configured streams
  static const int32_t kNumberOfConfiguredStreams;

  // Store number of output buffers returned in capture results with frame
  // number as the key
  std::unordered_map<uint32_t, int32_t> num_capture_result_buffers_;

  // Store number of partial metadatas returned in capture results with frame
  // number as the key
  std::unordered_map<uint32_t, int32_t> num_capture_result_partial_metadata_;
};

const int32_t Camera3FlushRequestsTest::kNumberOfConfiguredStreams = 1;

void Camera3FlushRequestsTest::SetUp() {
  Camera3FrameFixture::SetUp();
  cam_device_.RegisterProcessCaptureResultCallback(base::Bind(
      &Camera3FlushRequestsTest::ProcessCaptureResult, base::Unretained(this)));
  cam_device_.RegisterNotifyCallback(
      base::Bind(&Camera3FlushRequestsTest::Notify, base::Unretained(this)));
  sem_init(&flush_result_sem_, 0, 0);
}

void Camera3FlushRequestsTest::ProcessCaptureResult(
    const camera3_capture_result* result) {
  VLOGF_ENTER();
  ASSERT_NE(nullptr, result) << "Capture result is null";

  if (result->result) {
    num_capture_result_partial_metadata_[result->frame_number] ++;
  }

  num_capture_result_buffers_[result->frame_number] +=
      result->num_output_buffers;

  if (num_capture_result_buffers_[result->frame_number] ==
      kNumberOfConfiguredStreams &&
      num_capture_result_partial_metadata_[result->frame_number] ==
      cam_device_.GetStaticInfo()->GetPartialResultCount()) {
    num_capture_results_++;
    sem_post(&flush_result_sem_);
  }
}

TEST_P(Camera3FlushRequestsTest, GetFrame) {
  // TODO(hywu): spawn a thread to test simultaneous process_capture_request
  // and flush

  // The number of configured streams must match the value of
  // |kNumberOfConfiguredStreams|.
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_,
                              CAMERA3_STREAM_ROTATION_0);
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";

  int32_t type = std::get<1>(GetParam());
  if (!cam_device_.IsTemplateSupported(type)) {
    return;
  }

  const int32_t num_frames = std::get<2>(GetParam());
  for (int32_t i = 0; i < num_frames; i++) {
    EXPECT_EQ(0, CreateCaptureRequestByTemplate(type, nullptr))
        << "Creating capture request fails";
  }

  ASSERT_EQ(0, cam_device_.Flush()) << "Flushing capture requests fails";

  // flush() should only return when there are no more outstanding buffers or
  // requests left in the HAL
  EXPECT_EQ(num_frames, num_capture_results_)
      << "There are requests left in the HAL after flushing";

  struct timespec timeout;
  for (int32_t i = 0; i < num_frames; i++) {
    GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
    ASSERT_EQ(0, sem_timedwait(&flush_result_sem_, &timeout));
  }
}

// Test parameters:
// - Camera ID
class Camera3MultiStreamFrameTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<int32_t> {
 public:
  Camera3MultiStreamFrameTest() : Camera3FrameFixture(GetParam()) {}
};

TEST_P(Camera3MultiStreamFrameTest, GetFrame) {
  // Preview stream with large size no bigger than 1080p
  ResolutionInfo limit_resolution(1920, 1080);
  ResolutionInfo preview_resolution(0, 0);
  ASSERT_EQ(
      0, GetMaxResolution(HAL_PIXEL_FORMAT_YCbCr_420_888, &preview_resolution))
      << "Failed to get max resolution for implementation defined format";
  preview_resolution = CapResolution(preview_resolution, limit_resolution);
  cam_device_.AddOutputStream(
      HAL_PIXEL_FORMAT_YCbCr_420_888, preview_resolution.Width(),
      preview_resolution.Height(), CAMERA3_STREAM_ROTATION_0);

  // Second preview stream
  cam_device_.AddOutputStream(
      HAL_PIXEL_FORMAT_YCbCr_420_888, preview_resolution.Width(),
      preview_resolution.Height(), CAMERA3_STREAM_ROTATION_0);

  // Capture stream with largest size
  ResolutionInfo capture_resolution(0, 0);
  ASSERT_EQ(0, GetMaxResolution(HAL_PIXEL_FORMAT_BLOB, &capture_resolution))
      << "Failed to get max resolution for YCbCr 420 format";
  cam_device_.AddOutputStream(HAL_PIXEL_FORMAT_BLOB, capture_resolution.Width(),
                              capture_resolution.Height(),
                              CAMERA3_STREAM_ROTATION_0);

  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";

  ASSERT_EQ(0,
            CreateCaptureRequestByTemplate(CAMERA3_TEMPLATE_PREVIEW, nullptr))
      << "Creating capture request fails";

  struct timespec timeout;
  GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
  WaitShutterAndCaptureResult(timeout);
}

// Test parameters:
// - Camera ID
class Camera3InvalidRequestTest : public Camera3FrameFixture,
                                  public ::testing::WithParamInterface<int> {
 public:
  Camera3InvalidRequestTest() : Camera3FrameFixture(GetParam()) {}
};

TEST_P(Camera3InvalidRequestTest, NullOrUnconfiguredRequest) {
  // Reference:
  // camera2/cts/CameraDeviceTest.java#testInvalidCapture
  EXPECT_NE(0, cam_device_.ProcessCaptureRequest(nullptr))
      << "Capturing with null request should fail";

  const camera_metadata_t* default_settings;
  default_settings =
      cam_device_.ConstructDefaultRequestSettings(CAMERA3_TEMPLATE_PREVIEW);
  std::vector<camera3_stream_buffer_t> output_buffers;
  std::vector<camera3_stream_t> streams(1);
  streams[0].stream_type = CAMERA3_STREAM_OUTPUT;
  streams[0].width = static_cast<uint32_t>(default_width_);
  streams[0].height = static_cast<uint32_t>(default_height_);
  streams[0].format = default_format_;
  std::vector<const camera3_stream_t*> stream_ptrs(1, &streams[0]);
  ASSERT_EQ(0, cam_device_.AllocateOutputBuffersByStreams(stream_ptrs,
                                                          &output_buffers))
      << "Failed to allocate buffers for capture request";
  camera3_capture_request_t capture_request = {
      .frame_number = 0,
      .settings = default_settings,
      .input_buffer = NULL,
      .num_output_buffers = static_cast<uint32_t>(output_buffers.size()),
      .output_buffers = output_buffers.data()};
  EXPECT_NE(0, cam_device_.ProcessCaptureRequest(&capture_request))
      << "Capturing with stream unconfigured should fail";
}

// Test parameters:
// - Camera ID
// - Number of frames to capture
class Camera3SimpleCaptureFrames
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<std::tuple<int32_t, int32_t>> {
 public:
  Camera3SimpleCaptureFrames()
      : Camera3FrameFixture(std::get<0>(GetParam())),
        num_frames_(std::get<1>(GetParam())) {}

 protected:
  // Process result metadata and/or output buffers
  void ProcessResultMetadataOutputBuffers(
      uint32_t frame_number,
      CameraMetadataUniquePtr metadata,
      std::vector<BufferHandleUniquePtr> buffers) override;

  // Validate capture result keys
  void ValidateCaptureResultKeys(
      const CameraMetadataUniquePtr& request_metadata);

  // Get waiver keys per camera device hardware level and capability
  void GetWaiverKeys(std::set<int32_t>* waiver_keys) const;

  // Process partial metadata
  void ProcessPartialMetadata(
      std::vector<CameraMetadataUniquePtr>* partial_metadata) override;

  // Validate partial results
  void ValidatePartialMetadata();

  const int32_t num_frames_;

  // Store result metadata in the first-in-first-out order
  std::list<CameraMetadataUniquePtr> result_metadata_;

  // Store partial metadata in the first-in-first-out order
  std::list<std::vector<CameraMetadataUniquePtr>> partial_metadata_list_;

  static const int32_t kCaptureResultKeys[69];
};

void Camera3SimpleCaptureFrames::ProcessResultMetadataOutputBuffers(
    uint32_t frame_number,
    CameraMetadataUniquePtr metadata,
    std::vector<BufferHandleUniquePtr> buffers) {
  result_metadata_.push_back(std::move(metadata));
}

void Camera3SimpleCaptureFrames::ValidateCaptureResultKeys(
    const CameraMetadataUniquePtr& request_metadata) {
  std::set<int32_t> waiver_keys;
  GetWaiverKeys(&waiver_keys);
  while (!result_metadata_.empty()) {
    camera_metadata_t* metadata = result_metadata_.front().get();
    for (auto key : kCaptureResultKeys) {
      if (waiver_keys.find(key) != waiver_keys.end()) {
        continue;
      }
      // Check the critical tags here.
      switch (key) {
        case ANDROID_CONTROL_AE_MODE:
        case ANDROID_CONTROL_AF_MODE:
        case ANDROID_CONTROL_AWB_MODE:
        case ANDROID_CONTROL_MODE:
        case ANDROID_STATISTICS_FACE_DETECT_MODE:
        case ANDROID_NOISE_REDUCTION_MODE:
          camera_metadata_ro_entry_t request_entry;
          if (find_camera_metadata_ro_entry(request_metadata.get(), key,
                                            &request_entry)) {
            ADD_FAILURE() << "Metadata " << get_camera_metadata_tag_name(key)
                          << " is unavailable in capture request";
            continue;
          }
          camera_metadata_ro_entry_t result_entry;
          if (find_camera_metadata_ro_entry(metadata, key, &result_entry)) {
            ADD_FAILURE() << "Metadata " << get_camera_metadata_tag_name(key)
                          << " is not present in capture result";
            continue;
          }
          EXPECT_EQ(request_entry.data.i32[0], result_entry.data.i32[0])
              << "Wrong value of metadata " << get_camera_metadata_tag_name(key)
              << " in capture result";
          break;
        case ANDROID_REQUEST_PIPELINE_DEPTH:
          break;
        default:
          // Only do non-null check for the rest of keys.
          camera_metadata_ro_entry_t entry;
          EXPECT_EQ(0, find_camera_metadata_ro_entry(metadata, key, &entry))
              << "Metadata " << get_camera_metadata_tag_name(key)
              << " is unavailable in capture result";
          break;
      }
    }
    result_metadata_.pop_front();
  }
}

const int32_t Camera3SimpleCaptureFrames::kCaptureResultKeys[] = {
    ANDROID_COLOR_CORRECTION_MODE,
    ANDROID_COLOR_CORRECTION_TRANSFORM,
    ANDROID_COLOR_CORRECTION_GAINS,
    ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE,
    ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
    ANDROID_CONTROL_AE_LOCK,
    ANDROID_CONTROL_AE_MODE,
    ANDROID_CONTROL_AE_REGIONS,
    ANDROID_CONTROL_AF_REGIONS,
    ANDROID_CONTROL_AWB_REGIONS,
    ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
    ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
    ANDROID_CONTROL_AF_MODE,
    ANDROID_CONTROL_AF_TRIGGER,
    ANDROID_CONTROL_AWB_LOCK,
    ANDROID_CONTROL_AWB_MODE,
    ANDROID_CONTROL_CAPTURE_INTENT,
    ANDROID_CONTROL_EFFECT_MODE,
    ANDROID_CONTROL_MODE,
    ANDROID_CONTROL_SCENE_MODE,
    ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
    ANDROID_CONTROL_AE_STATE,
    ANDROID_CONTROL_AF_STATE,
    ANDROID_CONTROL_AWB_STATE,
    ANDROID_EDGE_MODE,
    ANDROID_FLASH_MODE,
    ANDROID_FLASH_STATE,
    ANDROID_HOT_PIXEL_MODE,
    ANDROID_JPEG_ORIENTATION,
    ANDROID_JPEG_QUALITY,
    ANDROID_JPEG_THUMBNAIL_QUALITY,
    ANDROID_JPEG_THUMBNAIL_SIZE,
    ANDROID_LENS_APERTURE,
    ANDROID_LENS_FILTER_DENSITY,
    ANDROID_LENS_FOCAL_LENGTH,
    ANDROID_LENS_FOCUS_DISTANCE,
    ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
    ANDROID_LENS_POSE_ROTATION,
    ANDROID_LENS_POSE_TRANSLATION,
    ANDROID_LENS_FOCUS_RANGE,
    ANDROID_LENS_STATE,
    ANDROID_LENS_INTRINSIC_CALIBRATION,
    ANDROID_LENS_RADIAL_DISTORTION,
    ANDROID_NOISE_REDUCTION_MODE,
    ANDROID_REQUEST_PIPELINE_DEPTH,
    ANDROID_SCALER_CROP_REGION,
    ANDROID_SENSOR_EXPOSURE_TIME,
    ANDROID_SENSOR_FRAME_DURATION,
    ANDROID_SENSOR_SENSITIVITY,
    ANDROID_SENSOR_TIMESTAMP,
    ANDROID_SENSOR_NEUTRAL_COLOR_POINT,
    ANDROID_SENSOR_NOISE_PROFILE,
    ANDROID_SENSOR_GREEN_SPLIT,
    ANDROID_SENSOR_TEST_PATTERN_DATA,
    ANDROID_SENSOR_TEST_PATTERN_MODE,
    ANDROID_SENSOR_ROLLING_SHUTTER_SKEW,
    ANDROID_SHADING_MODE,
    ANDROID_STATISTICS_FACE_DETECT_MODE,
    ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE,
    ANDROID_STATISTICS_LENS_SHADING_CORRECTION_MAP,
    ANDROID_STATISTICS_SCENE_FLICKER,
    ANDROID_STATISTICS_HOT_PIXEL_MAP,
    ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
    ANDROID_TONEMAP_MODE,
    ANDROID_TONEMAP_GAMMA,
    ANDROID_TONEMAP_PRESET_CURVE,
    ANDROID_BLACK_LEVEL_LOCK,
    ANDROID_REPROCESS_EFFECTIVE_EXPOSURE_FACTOR};

void Camera3SimpleCaptureFrames::GetWaiverKeys(
    std::set<int32_t>* waiver_keys) const {
  // Global waiver keys
  waiver_keys->insert(ANDROID_JPEG_ORIENTATION);
  waiver_keys->insert(ANDROID_JPEG_QUALITY);
  waiver_keys->insert(ANDROID_JPEG_THUMBNAIL_QUALITY);
  waiver_keys->insert(ANDROID_JPEG_THUMBNAIL_SIZE);

  // Keys only present when corresponding control is on are being verified in
  // its own functional test
  // Only present in certain tonemap mode
  waiver_keys->insert(ANDROID_TONEMAP_CURVE_BLUE);
  waiver_keys->insert(ANDROID_TONEMAP_CURVE_GREEN);
  waiver_keys->insert(ANDROID_TONEMAP_CURVE_RED);
  waiver_keys->insert(ANDROID_TONEMAP_GAMMA);
  waiver_keys->insert(ANDROID_TONEMAP_PRESET_CURVE);
  // Only present when test pattern mode is SOLID_COLOR.
  waiver_keys->insert(ANDROID_SENSOR_TEST_PATTERN_DATA);
  // Only present when STATISTICS_LENS_SHADING_MAP_MODE is ON
  waiver_keys->insert(ANDROID_STATISTICS_LENS_SHADING_CORRECTION_MAP);
  // Only present when STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES is ON
  waiver_keys->insert(ANDROID_STATISTICS_HOT_PIXEL_MAP);
  // Only present when face detection is on
  waiver_keys->insert(ANDROID_STATISTICS_FACE_IDS);
  waiver_keys->insert(ANDROID_STATISTICS_FACE_LANDMARKS);
  waiver_keys->insert(ANDROID_STATISTICS_FACE_RECTANGLES);
  waiver_keys->insert(ANDROID_STATISTICS_FACE_SCORES);
  // Only present in reprocessing capture result.
  waiver_keys->insert(ANDROID_REPROCESS_EFFECTIVE_EXPOSURE_FACTOR);

  // Keys not required if RAW is not supported
  if (!cam_device_.GetStaticInfo()->IsCapabilitySupported(
          ANDROID_REQUEST_AVAILABLE_CAPABILITIES_RAW)) {
    waiver_keys->insert(ANDROID_SENSOR_NEUTRAL_COLOR_POINT);
    waiver_keys->insert(ANDROID_SENSOR_GREEN_SPLIT);
    waiver_keys->insert(ANDROID_SENSOR_NOISE_PROFILE);
  }

  // Keys for depth output capability
  if (!cam_device_.GetStaticInfo()->IsCapabilitySupported(
          ANDROID_REQUEST_AVAILABLE_CAPABILITIES_DEPTH_OUTPUT)) {
    waiver_keys->insert(ANDROID_LENS_POSE_ROTATION);
    waiver_keys->insert(ANDROID_LENS_POSE_TRANSLATION);
    waiver_keys->insert(ANDROID_LENS_INTRINSIC_CALIBRATION);
    waiver_keys->insert(ANDROID_LENS_RADIAL_DISTORTION);
  }

  if (cam_device_.GetStaticInfo()->GetAeMaxRegions() == 0) {
    waiver_keys->insert(ANDROID_CONTROL_AE_REGIONS);
  }
  if (cam_device_.GetStaticInfo()->GetAwbMaxRegions() == 0) {
    waiver_keys->insert(ANDROID_CONTROL_AWB_REGIONS);
  }
  if (cam_device_.GetStaticInfo()->GetAfMaxRegions() == 0) {
    waiver_keys->insert(ANDROID_CONTROL_AF_REGIONS);
  }

  if (cam_device_.GetStaticInfo()->IsHardwareLevelAtLeastFull()) {
    return;
  }

  // Keys to waive for limited devices
  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(
          ANDROID_COLOR_CORRECTION_MODE)) {
    waiver_keys->insert(ANDROID_COLOR_CORRECTION_GAINS);
    waiver_keys->insert(ANDROID_COLOR_CORRECTION_MODE);
    waiver_keys->insert(ANDROID_COLOR_CORRECTION_TRANSFORM);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(
          ANDROID_COLOR_CORRECTION_ABERRATION_MODE)) {
    waiver_keys->insert(ANDROID_COLOR_CORRECTION_ABERRATION_MODE);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(ANDROID_TONEMAP_MODE)) {
    waiver_keys->insert(ANDROID_TONEMAP_MODE);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(ANDROID_EDGE_MODE)) {
    waiver_keys->insert(ANDROID_EDGE_MODE);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(ANDROID_HOT_PIXEL_MODE)) {
    waiver_keys->insert(ANDROID_HOT_PIXEL_MODE);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(
          ANDROID_NOISE_REDUCTION_MODE)) {
    waiver_keys->insert(ANDROID_NOISE_REDUCTION_MODE);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(ANDROID_SHADING_MODE)) {
    waiver_keys->insert(ANDROID_SHADING_MODE);
  }

  // Keys not required if neither MANUAL_SENSOR nor READ_SENSOR_SETTINGS is
  // supported
  if (!cam_device_.GetStaticInfo()->IsCapabilitySupported(
          ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR)) {
    waiver_keys->insert(ANDROID_SENSOR_EXPOSURE_TIME);
    waiver_keys->insert(ANDROID_SENSOR_FRAME_DURATION);
    waiver_keys->insert(ANDROID_SENSOR_SENSITIVITY);
    waiver_keys->insert(ANDROID_BLACK_LEVEL_LOCK);
    waiver_keys->insert(ANDROID_LENS_FOCUS_RANGE);
    waiver_keys->insert(ANDROID_LENS_FOCUS_DISTANCE);
    waiver_keys->insert(ANDROID_LENS_STATE);
    waiver_keys->insert(ANDROID_LENS_APERTURE);
    waiver_keys->insert(ANDROID_LENS_FILTER_DENSITY);
  }
}

void Camera3SimpleCaptureFrames::ProcessPartialMetadata(
    std::vector<CameraMetadataUniquePtr>* partial_metadata) {
  partial_metadata_list_.resize(partial_metadata_list_.size() + 1);
  for (auto& it : *partial_metadata) {
    partial_metadata_list_.back().push_back(std::move(it));
  }
}

void Camera3SimpleCaptureFrames::ValidatePartialMetadata() {
  for (const auto& it : partial_metadata_list_) {
    // Number of partial results is less than or equal to
    // REQUEST_PARTIAL_RESULT_COUNT
    EXPECT_GE(cam_device_.GetStaticInfo()->GetPartialResultCount(), it.size())
        << "Number of received partial results is wrong";

    // Each key appeared in partial results must be unique across all partial
    // results
    for (size_t i = 0; i < it.size(); i++) {
      size_t entry_count = get_camera_metadata_entry_count(it[i].get());
      for (size_t entry_index = 0; entry_index < entry_count; entry_index++) {
        camera_metadata_ro_entry_t entry;
        ASSERT_EQ(
            0, get_camera_metadata_ro_entry(it[i].get(), entry_index, &entry));
        int32_t key = entry.tag;
        for (size_t j = i + 1; j < it.size(); j++) {
          EXPECT_NE(0, find_camera_metadata_ro_entry(it[j].get(), key, &entry))
              << "Key " << get_camera_metadata_tag_name(key)
              << " appears in multiple partial results";
        }
      }
    }
  }
}

TEST_P(Camera3SimpleCaptureFrames, Camera3ResultAllKeysTest) {
  // Reference:
  // camera2/cts/CaptureResultTest.java#testCameraCaptureResultAllKeys
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_,
                              CAMERA3_STREAM_ROTATION_0);
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";
  CameraMetadataUniquePtr metadata(clone_camera_metadata(
      cam_device_.ConstructDefaultRequestSettings(CAMERA3_TEMPLATE_PREVIEW)));

  for (int32_t i = 0; i < num_frames_; i++) {
    ASSERT_EQ(0, CreateCaptureRequestByMetadata(metadata, nullptr))
        << "Creating capture request fails";
  }

  struct timespec timeout;
  GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
  for (int32_t i = 0; i < num_frames_; i++) {
    WaitShutterAndCaptureResult(timeout);
  }

  ValidateCaptureResultKeys(metadata);
}

TEST_P(Camera3SimpleCaptureFrames, Camera3PartialResultTest) {
  // Reference:
  // camera2/cts/CaptureResultTest.java#testPartialResult
  // Skip the test if partial result is not supported
  if (cam_device_.GetStaticInfo()->GetPartialResultCount() == 1) {
    return;
  }

  cam_device_.AddOutputStream(default_format_, default_width_, default_height_,
                              CAMERA3_STREAM_ROTATION_0);
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";

  for (int32_t i = 0; i < num_frames_; i++) {
    ASSERT_EQ(0,
              CreateCaptureRequestByTemplate(CAMERA3_TEMPLATE_PREVIEW, nullptr))
        << "Creating capture request fails";
  }

  struct timespec timeout;
  GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
  for (int32_t i = 0; i < num_frames_; i++) {
    WaitShutterAndCaptureResult(timeout);
  }

  ValidatePartialMetadata();
}

// Test parameters:
// - Camera ID
class Camera3ResultTimestampsTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<int32_t> {
 public:
  Camera3ResultTimestampsTest() : Camera3FrameFixture(GetParam()) {}

  void SetUp() override;

 protected:
  virtual void Notify(const camera3_notify_msg* msg);

  // Process result metadata and/or output buffers
  void ProcessResultMetadataOutputBuffers(
      uint32_t frame_number,
      CameraMetadataUniquePtr metadata,
      std::vector<BufferHandleUniquePtr> buffers) override;

  // Validate and get one timestamp
  void ValidateAndGetTimestamp(int64_t* timestamp);

 private:
  // Store timestamps of shutter callback in the first-in-first-out order
  std::list<uint64_t> capture_timestamps_;

  // Store result metadata in the first-in-first-out order
  std::list<CameraMetadataUniquePtr> result_metadata_;
};

void Camera3ResultTimestampsTest::SetUp() {
  Camera3FrameFixture::SetUp();
  cam_device_.RegisterNotifyCallback(
      base::Bind(&Camera3ResultTimestampsTest::Notify, base::Unretained(this)));
}

void Camera3ResultTimestampsTest::Notify(const camera3_notify_msg* msg) {
  VLOGF_ENTER();
  EXPECT_EQ(CAMERA3_MSG_SHUTTER, msg->type)
      << "Shutter error = " << msg->message.error.error_code;

  if (msg->type == CAMERA3_MSG_SHUTTER) {
    capture_timestamps_.push_back(msg->message.shutter.timestamp);
  }
}

void Camera3ResultTimestampsTest::ProcessResultMetadataOutputBuffers(
    uint32_t frame_number,
    CameraMetadataUniquePtr metadata,
    std::vector<BufferHandleUniquePtr> buffers) {
  VLOGF_ENTER();
  result_metadata_.push_back(std::move(metadata));
}

void Camera3ResultTimestampsTest::ValidateAndGetTimestamp(int64_t* timestamp) {
  ASSERT_FALSE(capture_timestamps_.empty())
      << "Capture timestamp is unavailable";
  ASSERT_FALSE(result_metadata_.empty()) << "Result metadata is unavailable";
  camera_metadata_ro_entry_t entry;
  ASSERT_EQ(0, find_camera_metadata_ro_entry(result_metadata_.front().get(),
                                             ANDROID_SENSOR_TIMESTAMP, &entry))
      << "Metadata key ANDROID_SENSOR_TIMESTAMP not found";
  *timestamp = entry.data.i64[0];
  EXPECT_EQ(capture_timestamps_.front(), *timestamp)
      << "Shutter notification timestamp must be same as capture result"
         " timestamp";
  capture_timestamps_.pop_front();
  result_metadata_.pop_front();
}

TEST_P(Camera3ResultTimestampsTest, GetFrame) {
  // Reference:
  // camera2/cts/CaptureResultTest.java#testResultTimestamps
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_,
                              CAMERA3_STREAM_ROTATION_0);
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";

  ASSERT_EQ(0,
            CreateCaptureRequestByTemplate(CAMERA3_TEMPLATE_PREVIEW, nullptr))
      << "Creating capture request fails";
  struct timespec timeout;
  GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
  ASSERT_EQ(0, cam_device_.WaitCaptureResult(timeout));
  int64_t timestamp1 = 0;
  ValidateAndGetTimestamp(&timestamp1);

  ASSERT_EQ(0,
            CreateCaptureRequestByTemplate(CAMERA3_TEMPLATE_PREVIEW, nullptr))
      << "Creating capture request fails";
  GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
  ASSERT_EQ(0, cam_device_.WaitCaptureResult(timeout));
  int64_t timestamp2 = 0;
  ValidateAndGetTimestamp(&timestamp2);

  ASSERT_LT(timestamp1, timestamp2) << "Timestamps must be increasing";
}

// Test parameters:
// - Camera ID
class Camera3InvalidBufferTest : public Camera3FrameFixture,
                                 public ::testing::WithParamInterface<int32_t> {
 public:
  Camera3InvalidBufferTest() : Camera3FrameFixture(GetParam()) {}

  void SetUp() override;

 protected:
  // Callback functions from HAL device
  virtual void ProcessCaptureResult(const camera3_capture_result* result);

  void RunInvalidBufferTest(buffer_handle_t* handle);

  // Callback functions from HAL device
  // Do nothing.
  virtual void Notify(const camera3_notify_msg* msg) {}

  sem_t capture_result_sem_;

 private:
  // Number of configured streams
  static const int32_t kNumberOfConfiguredStreams;
};

const int32_t Camera3InvalidBufferTest::kNumberOfConfiguredStreams = 1;

void Camera3InvalidBufferTest::SetUp() {
  Camera3FrameFixture::SetUp();
  cam_device_.RegisterProcessCaptureResultCallback(base::Bind(
      &Camera3InvalidBufferTest::ProcessCaptureResult, base::Unretained(this)));
  cam_device_.RegisterNotifyCallback(
      base::Bind(&Camera3InvalidBufferTest::Notify, base::Unretained(this)));
  sem_init(&capture_result_sem_, 0, 0);
}

void Camera3InvalidBufferTest::ProcessCaptureResult(
    const camera3_capture_result* result) {
  VLOGF_ENTER();
  ASSERT_NE(nullptr, result) << "Capture result is null";
  for (uint32_t i = 0; i < result->num_output_buffers; i++) {
    EXPECT_EQ(CAMERA3_BUFFER_STATUS_ERROR, result->output_buffers[i].status)
        << "Capture result should return error on invalid buffer";
  }
  if (result->num_output_buffers > 0) {
    sem_post(&capture_result_sem_);
  }
}

void Camera3InvalidBufferTest::RunInvalidBufferTest(buffer_handle_t* handle) {
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_,
                              CAMERA3_STREAM_ROTATION_0);
  std::vector<const camera3_stream_t*> streams;
  ASSERT_EQ(0, cam_device_.ConfigureStreams(&streams))
      << "Configuring stream fails";
  const camera_metadata_t* default_settings;
  default_settings =
      cam_device_.ConstructDefaultRequestSettings(CAMERA3_TEMPLATE_PREVIEW);
  ASSERT_NE(nullptr, default_settings) << "Camera default settings are NULL";
  camera3_stream_buffer_t stream_buffer = {
      .stream = const_cast<camera3_stream_t*>(streams.front()),
      .buffer = handle,
      .status = CAMERA3_BUFFER_STATUS_OK,
      .acquire_fence = -1,
      .release_fence = -1};
  std::vector<camera3_stream_buffer_t> stream_buffers(1, stream_buffer);
  camera3_capture_request_t capture_request = {
      .frame_number = UINT32_MAX,
      .settings = default_settings,
      .input_buffer = NULL,
      .num_output_buffers = static_cast<uint32_t>(stream_buffers.size()),
      .output_buffers = stream_buffers.data()};
  int ret = cam_device_.ProcessCaptureRequest(&capture_request);
  if (ret == -EINVAL) {
    return;
  }
  ASSERT_EQ(0, ret);
  struct timespec timeout;
  GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
  ASSERT_EQ(0, sem_timedwait(&capture_result_sem_, &timeout));
}

TEST_P(Camera3InvalidBufferTest, NullBufferHandle) {
  buffer_handle_t handle = nullptr;
  RunInvalidBufferTest(&handle);
}

TEST_P(Camera3InvalidBufferTest, FreedBufferHandle) {
  BufferHandleUniquePtr buffer = Camera3TestGralloc::GetInstance()->Allocate(
      default_width_, default_height_, HAL_PIXEL_FORMAT_YCbCr_420_888,
      GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_CAMERA_WRITE);
  buffer_handle_t* handle = buffer.get();
  buffer.reset();
  ASSERT_NE(nullptr, handle);
  RunInvalidBufferTest(handle);
}

// Test parameters:
// - Camera ID, frame format, resolution width, resolution height
class Camera3FrameContentTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<
          std::tuple<int32_t, int32_t, int32_t, int32_t>> {
 public:
  const double kContentTestSsimThreshold = 0.75;

  Camera3FrameContentTest()
      : Camera3FrameFixture(std::get<0>(GetParam())),
        format_(std::get<1>(GetParam())),
        width_(std::get<2>(GetParam())),
        height_(std::get<3>(GetParam())) {}

 protected:
  void ProcessResultMetadataOutputBuffers(
      uint32_t frame_number,
      CameraMetadataUniquePtr metadata,
      std::vector<BufferHandleUniquePtr> buffers) override;

  int32_t format_;

  int32_t width_;

  int32_t height_;

  BufferHandleUniquePtr buffer_handle_;
};

void Camera3FrameContentTest::ProcessResultMetadataOutputBuffers(
    uint32_t frame_number,
    CameraMetadataUniquePtr metadata,
    std::vector<BufferHandleUniquePtr> buffers) {
  ASSERT_EQ(nullptr, buffer_handle_);
  buffer_handle_ = std::move(buffers.front());
}

TEST_P(Camera3FrameContentTest, CorruptionDetection) {
  auto test_pattern_modes = GetAvailableColorBarsTestPatternModes();
  ASSERT_FALSE(test_pattern_modes.empty())
      << "Failed to get sensor available test pattern modes";

  cam_device_.AddOutputStream(format_, width_, height_,
                              CAMERA3_STREAM_ROTATION_0);
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";
  CameraMetadataUniquePtr metadata(clone_camera_metadata(
      cam_device_.ConstructDefaultRequestSettings(CAMERA3_TEMPLATE_PREVIEW)));
  UpdateMetadata(ANDROID_SENSOR_TEST_PATTERN_MODE, test_pattern_modes.data(), 1,
                 &metadata);
  ASSERT_EQ(0, CreateCaptureRequestByMetadata(metadata, nullptr))
      << "Creating capture request fails";

  struct timespec timeout;
  GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
  WaitShutterAndCaptureResult(timeout);
  ASSERT_NE(nullptr, buffer_handle_) << "Failed to get frame buffer";
  auto capture_image = ConvertToImage(std::move(buffer_handle_), width_,
                                      height_, ImageFormat::IMAGE_FORMAT_I420);
  ASSERT_NE(nullptr, capture_image);

  for (const auto& it : color_bars_test_patterns_) {
    auto pattern_image = GenerateColorBarsPattern(
        width_, height_, ImageFormat::IMAGE_FORMAT_I420, it,
        test_pattern_modes.front());
    ASSERT_NE(nullptr, pattern_image);

    if (ComputeSsim(*capture_image, *pattern_image) >
        kContentTestSsimThreshold) {
      return;
    }
  }
  std::stringstream ss;
  ss << "/tmp/corruption_test_0x" << std::hex << format_ << "_" << std::dec
     << width_ << "x" << height_;
  capture_image->SaveToFile(ss.str());
  ADD_FAILURE() << "The frame content is corrupted";
}

TEST_P(Camera3FrameContentTest, DetectGreenLine) {
  auto test_pattern_modes = GetAvailableColorBarsTestPatternModes();
  ASSERT_FALSE(test_pattern_modes.empty())
      << "Failed to get sensor available test pattern modes";

  cam_device_.AddOutputStream(format_, width_, height_,
                              CAMERA3_STREAM_ROTATION_0);
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";
  CameraMetadataUniquePtr metadata(clone_camera_metadata(
      cam_device_.ConstructDefaultRequestSettings(CAMERA3_TEMPLATE_PREVIEW)));
  UpdateMetadata(ANDROID_SENSOR_TEST_PATTERN_MODE, test_pattern_modes.data(), 1,
                 &metadata);
  ASSERT_EQ(0, CreateCaptureRequestByMetadata(metadata, nullptr))
      << "Creating capture request fails";

  struct timespec timeout;
  GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
  WaitShutterAndCaptureResult(timeout);
  ASSERT_NE(nullptr, buffer_handle_) << "Failed to get frame buffer";
  auto argb_image = ConvertToImage(std::move(buffer_handle_), width_, height_,
                                   ImageFormat::IMAGE_FORMAT_ARGB);
  ASSERT_NE(nullptr, argb_image);

  auto IsGreenPixel = [](const uint8_t* pixel) {
    const uint8_t kRedOrBlueUpperLimit = 50;
    const uint8_t kGreenLowerLimit = 100;
    const uint32_t kRedOffset = 0;
    const uint32_t kGreenOffset = 1;
    const uint32_t kBlueOffset = 2;
    return *(pixel + kRedOffset)<kRedOrBlueUpperLimit&&*(pixel + kGreenOffset)>
               kGreenLowerLimit &&
           *(pixel + kBlueOffset) < kRedOrBlueUpperLimit;
  };
  auto IsBottomLineGreen = [&](const ImageUniquePtr& argb_image) {
    uint8_t* pixel_of_last_line =
        argb_image->planes[0].addr +
        argb_image->planes[0].stride * (argb_image->height - 1);
    for (size_t i = 0; i < argb_image->planes[0].stride;
         i += kARGBPixelWidth, pixel_of_last_line += kARGBPixelWidth) {
      if (!IsGreenPixel(pixel_of_last_line)) {
        return false;
      }
    }
    return true;
  };
  EXPECT_FALSE(IsBottomLineGreen(argb_image))
      << "Green line at the bottom of captured frame";
  auto IsRightMostLineGreen = [&](const ImageUniquePtr& argb_image) {
    uint8_t* last_pixel_of_line = argb_image->planes[0].addr +
                                  argb_image->planes[0].stride -
                                  kARGBPixelWidth;
    for (size_t i = 0; i < argb_image->height;
         i++, last_pixel_of_line += argb_image->planes[0].stride) {
      if (!IsGreenPixel(last_pixel_of_line)) {
        return false;
      }
    }
    return true;
  };
  EXPECT_FALSE(IsRightMostLineGreen(argb_image))
      << "Green line at the rightmost of captured frame";
}

// Test parameters:
// - Camera ID, frame format, resolution width, resolution height
// - Rotation degrees
class Camera3PortraitRotationTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<
          std::tuple<std::tuple<int32_t, int32_t, int32_t, int32_t>, int32_t>> {
 public:
  const double kPortraitTestSsimThreshold = 0.75;

  Camera3PortraitRotationTest()
      : Camera3FrameFixture(std::get<0>(std::get<0>(GetParam()))),
        format_(std::get<1>(std::get<0>(GetParam()))),
        width_(std::get<2>(std::get<0>(GetParam()))),
        height_(std::get<3>(std::get<0>(GetParam()))),
        rotation_degrees_(std::get<1>(GetParam())),
        save_images_(base::CommandLine::ForCurrentProcess()->HasSwitch(
            "save_portrait_test_images")) {}

 protected:
  void ProcessResultMetadataOutputBuffers(
      uint32_t frame_number,
      CameraMetadataUniquePtr metadata,
      std::vector<BufferHandleUniquePtr> buffers) override;

  // Rotate |in_buffer| 180 degrees to |out_buffer|.
  int Rotate180(const Image& in_buffer, Image* out_buffer);

  // Crop-rotate-scale |in_buffer| to |out_buffer|.
  int CropRotateScale(const Image& in_buffer, Image* out_buffer);

  int32_t format_;

  int32_t width_;

  int32_t height_;

  int32_t rotation_degrees_;

  bool save_images_;

  BufferHandleUniquePtr buffer_handle_;
};

void Camera3PortraitRotationTest::ProcessResultMetadataOutputBuffers(
    uint32_t frame_number,
    CameraMetadataUniquePtr metadata,
    std::vector<BufferHandleUniquePtr> buffers) {
  ASSERT_EQ(nullptr, buffer_handle_);
  buffer_handle_ = std::move(buffers.front());
}

int Camera3PortraitRotationTest::Rotate180(const Image& in_buffer,
                                           Image* out_buffer) {
  if (in_buffer.format != ImageFormat::IMAGE_FORMAT_I420 ||
      out_buffer->format != ImageFormat::IMAGE_FORMAT_I420 ||
      in_buffer.width != out_buffer->width ||
      in_buffer.height != out_buffer->height) {
    return -EINVAL;
  }
  return libyuv::I420Rotate(
      in_buffer.planes[0].addr, in_buffer.planes[0].stride,
      in_buffer.planes[1].addr, in_buffer.planes[1].stride,
      in_buffer.planes[2].addr, in_buffer.planes[2].stride,
      out_buffer->planes[0].addr, out_buffer->planes[0].stride,
      out_buffer->planes[1].addr, out_buffer->planes[1].stride,
      out_buffer->planes[2].addr, out_buffer->planes[2].stride, in_buffer.width,
      in_buffer.height, libyuv::RotationMode::kRotate180);
}

int Camera3PortraitRotationTest::CropRotateScale(const Image& in_buffer,
                                                 Image* out_buffer) {
  if (in_buffer.format != ImageFormat::IMAGE_FORMAT_I420 || !out_buffer ||
      out_buffer->format != ImageFormat::IMAGE_FORMAT_I420 ||
      in_buffer.width != out_buffer->width ||
      in_buffer.height != out_buffer->height) {
    return -EINVAL;
  }
  int width = in_buffer.width;
  int height = in_buffer.height;
  int cropped_width = height * height / width;
  if (cropped_width % 2 == 1) {
    // Make cropped_width to the closest even number.
    cropped_width++;
  }
  int cropped_height = height;
  int margin = (width - cropped_width) / 2;

  int rotated_height = cropped_width;
  int rotated_width = cropped_height;
  libyuv::RotationMode rotation_mode = libyuv::RotationMode::kRotate90;
  switch (rotation_degrees_) {
    case 90:
      rotation_mode = libyuv::RotationMode::kRotate90;
      break;
    case 270:
      rotation_mode = libyuv::RotationMode::kRotate270;
      break;
    default:
      LOG(ERROR) << "Invalid rotation degree: " << rotation_degrees_;
      return -EINVAL;
  }

  ImageUniquePtr rotated_buffer(
      new Image(rotated_width, rotated_height, ImageFormat::IMAGE_FORMAT_I420));
  // This libyuv method first crops the frame and then rotates it 90 degrees
  // clockwise or counterclockwise.
  int res = libyuv::ConvertToI420(
      in_buffer.planes[0].addr, in_buffer.planes[0].stride,
      rotated_buffer->planes[0].addr, rotated_buffer->planes[0].stride,
      rotated_buffer->planes[1].addr, rotated_buffer->planes[1].stride,
      rotated_buffer->planes[2].addr, rotated_buffer->planes[2].stride, margin,
      0, width, height, cropped_width, cropped_height, rotation_mode,
      libyuv::FourCC::FOURCC_I420);
  if (res) {
    LOG(ERROR) << "ConvertToI420 failed: " << res;
    return res;
  }

  res = libyuv::I420Scale(
      rotated_buffer->planes[0].addr, rotated_buffer->planes[0].stride,
      rotated_buffer->planes[1].addr, rotated_buffer->planes[1].stride,
      rotated_buffer->planes[2].addr, rotated_buffer->planes[2].stride,
      rotated_width, rotated_height, out_buffer->planes[0].addr,
      out_buffer->planes[0].stride, out_buffer->planes[1].addr,
      out_buffer->planes[1].stride, out_buffer->planes[2].addr,
      out_buffer->planes[2].stride, width, height,
      libyuv::FilterMode::kFilterNone);
  if (res) {
    LOG(ERROR) << "I420Scale failed: " << res;
  }
  return res;
}

TEST_P(Camera3PortraitRotationTest, GetFrame) {
  auto test_pattern_modes = GetAvailableColorBarsTestPatternModes();
  ASSERT_FALSE(test_pattern_modes.empty())
      << "Failed to get sensor available test pattern modes";

  if (cam_device_.GetStaticInfo()->IsFormatAvailable(format_)) {
    VLOGF(1) << "Device " << cam_id_;
    VLOGF(1) << "Format 0x" << std::hex << format_;
    VLOGF(1) << "Resolution " << width_ << "x" << height_;
    VLOGF(1) << "Rotation " << rotation_degrees_;

    cam_device_.AddOutputStream(format_, width_, height_,
                                CAMERA3_STREAM_ROTATION_0);
    ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
        << "Configuring stream fails";

    // Get original pattern
    CameraMetadataUniquePtr metadata(clone_camera_metadata(
        cam_device_.ConstructDefaultRequestSettings(CAMERA3_TEMPLATE_PREVIEW)));
    UpdateMetadata(ANDROID_SENSOR_TEST_PATTERN_MODE, test_pattern_modes.data(),
                   1, &metadata);
    ASSERT_EQ(0, CreateCaptureRequestByMetadata(metadata, nullptr))
        << "Creating capture request fails";

    struct timespec timeout;
    GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
    WaitShutterAndCaptureResult(timeout);
    ASSERT_NE(nullptr, buffer_handle_) << "Failed to get original frame buffer";
    auto orig_i420_image =
        ConvertToImage(std::move(buffer_handle_), width_, height_,
                       ImageFormat::IMAGE_FORMAT_I420);
    ASSERT_NE(nullptr, orig_i420_image);
    auto SaveImage = [this](const Image& image, const std::string suffix) {
      std::stringstream ss;
      ss << "/tmp/portrait_test_0x" << std::hex << format_ << "_" << std::dec
         << width_ << "x" << height_ << "_" << rotation_degrees_ << suffix;
      EXPECT_EQ(0, image.SaveToFile(ss.str()));
    };
    if (save_images_) {
      SaveImage(*orig_i420_image, "_orig");
    }

    // Re-configure streams with rotation
    camera3_stream_rotation_t crop_rotate_scale_degrees =
        CAMERA3_STREAM_ROTATION_0;
    switch (rotation_degrees_) {
      case 90:
        crop_rotate_scale_degrees = CAMERA3_STREAM_ROTATION_90;
        break;
      case 270:
        crop_rotate_scale_degrees = CAMERA3_STREAM_ROTATION_270;
        break;
      default:
        FAIL() << "Invalid rotation degree: " << rotation_degrees_;
    }
    cam_device_.AddOutputStream(format_, width_, height_,
                                crop_rotate_scale_degrees);
    ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
        << "Configuring stream fails";
    ASSERT_EQ(0, CreateCaptureRequestByMetadata(metadata, nullptr))
        << "Creating capture request fails";

    // Verify the original pattern is asymmetric
    ImageUniquePtr orig_rotated_i420_image(
        new Image(width_, height_, ImageFormat::IMAGE_FORMAT_I420));
    ASSERT_EQ(0, Rotate180(*orig_i420_image, orig_rotated_i420_image.get()));
    ASSERT_LE(ComputeSsim(*orig_i420_image, *orig_rotated_i420_image),
              kPortraitTestSsimThreshold)
        << "Test pattern appears to be symmetric";

    // Generate software crop-rotate-scaled pattern
    ImageUniquePtr sw_portrait_i420_image(
        new Image(width_, height_, ImageFormat::IMAGE_FORMAT_I420));
    ASSERT_NE(nullptr, sw_portrait_i420_image);
    ASSERT_EQ(0,
              CropRotateScale(*orig_i420_image, sw_portrait_i420_image.get()));
    if (save_images_) {
      SaveImage(*sw_portrait_i420_image, "_swconv");
    }

    GetTimeOfTimeout(kDefaultTimeoutMs, &timeout);
    WaitShutterAndCaptureResult(timeout);
    ASSERT_NE(nullptr, buffer_handle_) << "Failed to get portrait frame buffer";
    auto portrait_i420_image =
        ConvertToImage(std::move(buffer_handle_), width_, height_,
                       ImageFormat::IMAGE_FORMAT_I420);
    ASSERT_NE(nullptr, portrait_i420_image);
    if (save_images_) {
      SaveImage(*portrait_i420_image, "_conv");
    }

    // Compare similarity of crop-rotate-scaled patterns
    ASSERT_GT(ComputeSsim(*sw_portrait_i420_image, *portrait_i420_image),
              kPortraitTestSsimThreshold)
        << "SSIM value is lower than threshold";
  }
}

INSTANTIATE_TEST_CASE_P(
    Camera3FrameTest,
    Camera3SingleFrameTest,
    ::testing::Combine(
        ::testing::ValuesIn(Camera3Module().GetCameraIds()),
        ::testing::Values(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                          HAL_PIXEL_FORMAT_YCbCr_420_888,
                          HAL_PIXEL_FORMAT_YCrCb_420_SP,
                          HAL_PIXEL_FORMAT_BLOB,
                          HAL_PIXEL_FORMAT_YV12,
                          HAL_PIXEL_FORMAT_Y8,
                          HAL_PIXEL_FORMAT_Y16,
                          HAL_PIXEL_FORMAT_RAW16),
        ::testing::Values(CAMERA3_TEMPLATE_PREVIEW,
                          CAMERA3_TEMPLATE_STILL_CAPTURE,
                          CAMERA3_TEMPLATE_VIDEO_RECORD,
                          CAMERA3_TEMPLATE_VIDEO_SNAPSHOT,
                          CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG,
                          CAMERA3_TEMPLATE_MANUAL),
        ::testing::Bool()));

INSTANTIATE_TEST_CASE_P(
    Camera3FrameTest,
    Camera3MultiFrameTest,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Values(CAMERA3_TEMPLATE_PREVIEW,
                                         CAMERA3_TEMPLATE_STILL_CAPTURE,
                                         CAMERA3_TEMPLATE_VIDEO_RECORD,
                                         CAMERA3_TEMPLATE_VIDEO_SNAPSHOT,
                                         CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG,
                                         CAMERA3_TEMPLATE_MANUAL),
                       ::testing::Range(1, 10)));

INSTANTIATE_TEST_CASE_P(Camera3FrameTest,
                        Camera3MixedTemplateMultiFrameTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

INSTANTIATE_TEST_CASE_P(
    Camera3FrameTest,
    Camera3FlushRequestsTest,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Values(CAMERA3_TEMPLATE_PREVIEW,
                                         CAMERA3_TEMPLATE_STILL_CAPTURE,
                                         CAMERA3_TEMPLATE_VIDEO_RECORD,
                                         CAMERA3_TEMPLATE_VIDEO_SNAPSHOT,
                                         CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG,
                                         CAMERA3_TEMPLATE_MANUAL),
                       ::testing::Values(10)));

INSTANTIATE_TEST_CASE_P(Camera3FrameTest,
                        Camera3MultiStreamFrameTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

INSTANTIATE_TEST_CASE_P(NullOrUnconfiguredRequest,
                        Camera3InvalidRequestTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

INSTANTIATE_TEST_CASE_P(
    Camera3FrameTest,
    Camera3SimpleCaptureFrames,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Values(10)));

INSTANTIATE_TEST_CASE_P(Camera3FrameTest,
                        Camera3ResultTimestampsTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

INSTANTIATE_TEST_CASE_P(Camera3FrameTest,
                        Camera3InvalidBufferTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

static std::vector<std::tuple<int32_t, int32_t, int32_t, int32_t>>
IterateCameraIdFormatResolution() {
  std::vector<std::tuple<int32_t, int32_t, int32_t, int32_t>> result;
  auto cam_ids = Camera3Module().GetCameraIds();
  auto formats =
      std::vector<int>({HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                        HAL_PIXEL_FORMAT_YCbCr_420_888, HAL_PIXEL_FORMAT_BLOB});
  for (const auto& cam_id : cam_ids) {
    for (const auto& format : formats) {
      auto resolutions =
          Camera3Module().GetSortedOutputResolutions(cam_id, format);
      for (const auto& resolution : resolutions) {
        result.emplace_back(cam_id, format, resolution.Width(),
                            resolution.Height());
      }
    }
  }
  return result;
}

INSTANTIATE_TEST_CASE_P(Camera3FrameTest,
                        Camera3FrameContentTest,
                        ::testing::ValuesIn(IterateCameraIdFormatResolution()));

INSTANTIATE_TEST_CASE_P(
    Camera3FrameTest,
    Camera3PortraitRotationTest,
    ::testing::Combine(::testing::ValuesIn(IterateCameraIdFormatResolution()),
                       ::testing::Values(90, 270)));

}  // namespace camera3_test
