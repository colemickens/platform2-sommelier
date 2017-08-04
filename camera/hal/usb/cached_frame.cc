/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/cached_frame.h"

#include <errno.h>

#include "arc/common.h"
#include "hal/usb/common_types.h"

namespace arc {

CachedFrame::CachedFrame()
    : source_frame_(nullptr),
      rotated_frame_(new AllocatedFrameBuffer(0)),
      yu12_frame_(new AllocatedFrameBuffer(0)) {}

CachedFrame::~CachedFrame() {
  UnsetSource();
}

int CachedFrame::SetSource(const FrameBuffer* frame,
                           int rotate_degree,
                           bool test_pattern) {
  source_frame_ = frame;
  int res = ConvertToYU12(test_pattern);
  if (res != 0) {
    return res;
  }

  if (rotate_degree > 0) {
    res = CropRotateScale(rotate_degree);
  }
  return res;
}

void CachedFrame::UnsetSource() {
  source_frame_ = nullptr;
}

uint8_t* CachedFrame::GetSourceBuffer() const {
  return source_frame_->GetData();
}

size_t CachedFrame::GetSourceDataSize() const {
  return source_frame_->GetDataSize();
}

uint32_t CachedFrame::GetSourceFourCC() const {
  return source_frame_->GetFourcc();
}

uint8_t* CachedFrame::GetCachedBuffer() const {
  return yu12_frame_->GetData();
}

uint32_t CachedFrame::GetCachedFourCC() const {
  return yu12_frame_->GetFourcc();
}

int CachedFrame::GetWidth() const {
  return yu12_frame_->GetWidth();
}

int CachedFrame::GetHeight() const {
  return yu12_frame_->GetHeight();
}

int CachedFrame::Convert(const android::CameraMetadata& metadata,
                         FrameBuffer* out_frame,
                         bool video_hack) {
  if (video_hack && out_frame->GetFourcc() == V4L2_PIX_FMT_YVU420) {
    out_frame->SetFourcc(V4L2_PIX_FMT_YUV420);
  }
  return ImageProcessor::ConvertFormat(metadata, *yu12_frame_.get(), out_frame);
}

int CachedFrame::ConvertToYU12(bool test_pattern) {
  yu12_frame_->SetFourcc(V4L2_PIX_FMT_YUV420M);
  yu12_frame_->SetWidth(source_frame_->GetWidth());
  yu12_frame_->SetHeight(source_frame_->GetHeight());

  if (test_pattern == false) {
    int ret = ImageProcessor::ConvertFormat(android::CameraMetadata(),
                                            *source_frame_, yu12_frame_.get());
    if (ret) {
      LOGF(ERROR) << "Convert from "
                  << FormatToString(source_frame_->GetFourcc())
                  << " to YU12 failed.";
      return ret;
    }
  } else {
    yu12_frame_->SetDataSize(source_frame_->GetDataSize());
    memcpy(yu12_frame_->GetData(), source_frame_->GetData(),
           source_frame_->GetDataSize());
  }
  return 0;
}

int CachedFrame::CropRotateScale(int rotate_degree) {
  if (yu12_frame_->GetHeight() % 2 != 0 || yu12_frame_->GetWidth() % 2 != 0) {
    LOGF(ERROR) << "yu12_frame_ has odd dimension: " << yu12_frame_->GetWidth()
                << "x" << yu12_frame_->GetHeight();
    return -EINVAL;
  }

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
  rotated_frame_->SetWidth(cropped_height);
  rotated_frame_->SetHeight(cropped_width);

  int ret = ImageProcessor::CropAndRotate(*yu12_frame_.get(),
                                          rotated_frame_.get(), rotate_degree);
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

  ret = ImageProcessor::Scale(*rotated_frame_.get(), yu12_frame_.get());
  LOGF_IF(ERROR, ret) << "Scale failed: " << ret;
  return ret;
}

}  // namespace arc
