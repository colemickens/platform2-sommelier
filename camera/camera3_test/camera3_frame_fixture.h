// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA_CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_
#define CAMERA_CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "camera3_test/camera3_stream_fixture.h"

namespace camera3_test {

class Camera3FrameFixture : public Camera3StreamFixture {
 public:
  const uint32_t kDefaultTimeoutMs = 1000;
  static const uint32_t kARGBPixelWidth = 4;

  explicit Camera3FrameFixture(int cam_id)
      : Camera3StreamFixture(cam_id),
        color_bars_test_patterns_(
            {{
                 // Android standard
                 // Color map:   R   , G   , B
                 std::make_tuple(0xFF, 0xFF, 0xFF),  // White
                 std::make_tuple(0xFF, 0xFF, 0x00),  // Yellow
                 std::make_tuple(0x00, 0xFF, 0xFF),  // Cyan
                 std::make_tuple(0x00, 0xFF, 0x00),  // Green
                 std::make_tuple(0xFF, 0x00, 0xFF),  // Magenta
                 std::make_tuple(0xFF, 0x00, 0x00),  // Red
                 std::make_tuple(0x00, 0x00, 0xFF),  // Blue
                 std::make_tuple(0x00, 0x00, 0x00),  // Black
             },
             {
                 // OV5670 color bars
                 std::make_tuple(0xFF, 0xFF, 0xFF),
                 std::make_tuple(0xC8, 0xC8, 0xC8),
                 std::make_tuple(0x96, 0x96, 0x96),
                 std::make_tuple(0x64, 0x64, 0x64),
                 std::make_tuple(0x32, 0x32, 0x32),
                 std::make_tuple(0x00, 0x00, 0x00),
                 std::make_tuple(0xFF, 0x00, 0x00),
                 std::make_tuple(0xFF, 0x32, 0x00),
                 std::make_tuple(0xFF, 0x00, 0xE6),
                 std::make_tuple(0x00, 0xFF, 0x00),
                 std::make_tuple(0x00, 0xFF, 0x00),
                 std::make_tuple(0x00, 0xFF, 0x00),
                 std::make_tuple(0x00, 0x00, 0xFF),
                 std::make_tuple(0xD2, 0x00, 0xFF),
                 std::make_tuple(0x00, 0xA0, 0xFF),
                 std::make_tuple(0xFF, 0xFF, 0xFF),
             },
             {
                 // IMX258 color bars
                 std::make_tuple(0xFF, 0xFF, 0xFF),  // White
                 std::make_tuple(0x00, 0xFF, 0xFF),  // Cyan
                 std::make_tuple(0xFF, 0xFF, 0x00),  // Yellow
                 std::make_tuple(0x00, 0xFF, 0x00),  // Green
                 std::make_tuple(0xFF, 0x00, 0xFF),  // Magenta
                 std::make_tuple(0x00, 0x00, 0xFF),  // Blue
                 std::make_tuple(0xFF, 0x00, 0x00),  // Red
                 std::make_tuple(0x00, 0x00, 0x00),  // Black
             }}),
        supported_color_bars_test_pattern_modes_(
            {ANDROID_SENSOR_TEST_PATTERN_MODE_COLOR_BARS_FADE_TO_GRAY,
             ANDROID_SENSOR_TEST_PATTERN_MODE_COLOR_BARS}) {}

 protected:
  // Create and process capture request of given metadata |metadata|. The frame
  // number of the created request is returned if |frame_number| is not null.
  int CreateCaptureRequestByMetadata(const CameraMetadataUniquePtr& metadata,
                                     uint32_t* frame_number);

  // Create and process capture request of given template |type|. The frame
  // number of the created request is returned if |frame_number| is not null.
  int CreateCaptureRequestByTemplate(int type, uint32_t* frame_number);

  // Wait for shutter and capture result with timeout
  void WaitShutterAndCaptureResult(const struct timespec& timeout);

  // Get available color bars test pattern modes
  std::vector<int32_t> GetAvailableColorBarsTestPatternModes();

  enum class ImageFormat {
    IMAGE_FORMAT_ARGB,
    IMAGE_FORMAT_I420,
    IMAGE_FORMAT_END
  };

  struct ImagePlane {
    ImagePlane(uint32_t stride, uint32_t size, uint8_t* addr);

    uint32_t stride;
    uint32_t size;
    uint8_t* addr;
  };

  struct Image {
    Image(uint32_t w, uint32_t h, ImageFormat f);
    int SaveToFile(const std::string filename) const;

    uint32_t width;
    uint32_t height;
    ImageFormat format;
    std::vector<uint8_t> data;
    uint32_t size;
    std::vector<ImagePlane> planes;
  };

  typedef std::unique_ptr<struct Image> ImageUniquePtr;

  // Convert the buffer to given format and return a new buffer in the Image
  // structure. The input buffer is freed.
  ImageUniquePtr ConvertToImage(BufferHandleUniquePtr buffer,
                                uint32_t width,
                                uint32_t height,
                                ImageFormat format);

  // Convert the buffer to given format, rotate the image by rotation and return
  // a new buffer in the Image structure. The input buffer is freed.
  ImageUniquePtr ConvertToImageAndRotate(BufferHandleUniquePtr buffer,
                                         uint32_t width,
                                         uint32_t height,
                                         ImageFormat format,
                                         int32_t rotation);

  ImageUniquePtr GenerateColorBarsPattern(
      uint32_t width,
      uint32_t height,
      ImageFormat format,
      const std::vector<std::tuple<uint8_t, uint8_t, uint8_t>>&
          color_bars_pattern,
      int32_t color_bars_pattern_mode);

  // Computes the structural similarity of given images. Given images must
  // be of the I420 format; otherwise, a value of 0.0 is returned. When given
  // images are very similar, it usually returns a score no less than 0.8.
  double ComputeSsim(const Image& buffer_a, const Image& buffer_b);

  std::vector<std::vector<std::tuple<uint8_t, uint8_t, uint8_t>>>
      color_bars_test_patterns_;

 private:
  // Create and process capture request of given metadata |metadata|. The frame
  // number of the created request is returned if |frame_number| is not null.
  int32_t CreateCaptureRequest(const camera_metadata_t& metadata,
                               uint32_t* frame_number);

  std::vector<int32_t> supported_color_bars_test_pattern_modes_;

  DISALLOW_COPY_AND_ASSIGN(Camera3FrameFixture);
};

void GetTimeOfTimeout(int32_t ms, struct timespec* ts);

}  // namespace camera3_test

#endif  // CAMERA_CAMERA3_TEST_CAMERA3_FRAME_FIXTURE_H_
