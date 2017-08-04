/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/image_processor.h"

#include <sys/mman.h>

#include <base/at_exit.h>
#include <gtest/gtest.h>

#include "hal/usb/frame_buffer.h"

namespace arc {

namespace tests {

class ImageProcessorTest : public ::testing::Test {
 public:
  ImageProcessorTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageProcessorTest);
};

TEST_F(ImageProcessorTest, GetConvertedSize) {
  std::unique_ptr<AllocatedFrameBuffer> frame(new AllocatedFrameBuffer(0));
  // Size should be 0 if format, width, and height are not set up correctly.
  EXPECT_EQ(ImageProcessor::GetConvertedSize(*frame.get()), 0);
  frame->SetFourcc(V4L2_PIX_FMT_YUV420M);
  EXPECT_EQ(ImageProcessor::GetConvertedSize(*frame.get()), 0);
  frame->SetWidth(1280);
  EXPECT_EQ(ImageProcessor::GetConvertedSize(*frame.get()), 0);
  frame->SetHeight(720);
  EXPECT_EQ(ImageProcessor::GetConvertedSize(*frame.get()), 1280 * 720 * 1.5);
}

}  // namespace tests

}  // namespace arc

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
