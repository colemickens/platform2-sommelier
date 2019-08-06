// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <gtest/gtest.h>

#include "v4l2_test/media_v4l2_device.h"

namespace cros {
namespace tests {

class V4L2TestEnvironment;

namespace {

V4L2TestEnvironment* g_env;

constexpr char kDefaultTestList[] = "default";
constexpr char kHalv3TestList[] = "halv3";
constexpr char kCertificationTestList[] = "certification";

bool ExerciseControl(V4L2Device* v4l2_dev, uint32_t id, const char* control) {
  v4l2_queryctrl query_ctrl;
  if (v4l2_dev->QueryControl(id, &query_ctrl)) {
    if (!v4l2_dev->SetControl(id, query_ctrl.maximum)) {
      LOG(WARNING) << "Cannot set " << control << " to maximum value";
    }
    if (!v4l2_dev->SetControl(id, query_ctrl.minimum)) {
      LOG(WARNING) << "Cannot set " << control << " to minimum value";
    }
    if (!v4l2_dev->SetControl(id, query_ctrl.default_value)) {
      LOG(WARNING) << "Cannot set " << control << " to default value";
    }
  } else {
    LOG(WARNING) << "Cannot query control name: " << control;
    return false;
  }
  return true;
}

}  // namespace

class V4L2TestEnvironment : public ::testing::Environment {
 public:
  V4L2TestEnvironment(const std::string& test_list,
                      const std::string& device_path)
      : test_list_(test_list),
        device_path_(device_path),
        test_constant_framerate_(test_list != kDefaultTestList) {}

  void SetUp() {
    LOG(INFO) << "Test list: " << test_list_;
    LOG(INFO) << "Device path: " << device_path_;
    LOG(INFO) << "Test constant framerate: " << std::boolalpha
              << test_constant_framerate_;
    ASSERT_TRUE(test_list_ == kDefaultTestList ||
                test_list_ == kHalv3TestList ||
                test_list_ == kCertificationTestList);
    ASSERT_TRUE(base::PathExists(base::FilePath(device_path_)));
  }

  std::string test_list_;
  std::string device_path_;
  bool test_constant_framerate_;
};

class V4L2Test : public ::testing::Test {
 protected:
  V4L2Test() : dev_(g_env->device_path_.c_str(), 4) {}
  void SetUp() override { ASSERT_TRUE(dev_.OpenDevice()); }
  void TearDown() override { dev_.CloseDevice(); }
  V4L2Device dev_;
};

class V4L2TestWithIO
    : public V4L2Test,
      public ::testing::WithParamInterface<V4L2Device::IOMethod> {};

TEST_F(V4L2Test, MultipleOpen) {
  V4L2Device dev2(g_env->device_path_.c_str(), 4);
  ASSERT_TRUE(dev2.OpenDevice()) << "Cannot open device for the second time";
  dev2.CloseDevice();
}

TEST_P(V4L2TestWithIO, MultipleInit) {
  V4L2Device::IOMethod io = GetParam();
  V4L2Device::ConstantFramerate constant_framerate =
      V4L2Device::DEFAULT_FRAMERATE_SETTING;
  V4L2Device& dev1 = dev_;
  V4L2Device dev2(g_env->device_path_.c_str(), 4);
  ASSERT_TRUE(dev2.OpenDevice()) << "Cannot open device for the second time";

  ASSERT_TRUE(dev1.InitDevice(io, 640, 480, V4L2_PIX_FMT_YUYV, 30,
                              constant_framerate, 0))
      << "Cannot init device for the first time";

  ASSERT_FALSE(dev2.InitDevice(io, 640, 480, V4L2_PIX_FMT_YUYV, 30,
                               constant_framerate, 0))
      << "Multiple init device should fail";

  dev1.UninitDevice();
  dev2.UninitDevice();
  dev2.CloseDevice();
}

// EnumInput and EnumStandard are optional.
TEST_F(V4L2Test, EnumInputAndStandard) {
  dev_.EnumInput();
  dev_.EnumStandard();
}

// EnumControl is optional, but the output is useful. For example, we could
// know whether constant framerate is supported or not.
TEST_F(V4L2Test, EnumControl) {
  dev_.EnumControl();
}

TEST_F(V4L2Test, SetControl) {
  // Test mandatory controls.
  if (g_env->test_constant_framerate_) {
    ASSERT_TRUE(ExerciseControl(&dev_, V4L2_CID_EXPOSURE_AUTO_PRIORITY,
                                "exposure_auto_priority"));
  }

  // Test optional controls.
  ExerciseControl(&dev_, V4L2_CID_BRIGHTNESS, "brightness");
  ExerciseControl(&dev_, V4L2_CID_CONTRAST, "contrast");
  ExerciseControl(&dev_, V4L2_CID_SATURATION, "saturation");
  ExerciseControl(&dev_, V4L2_CID_GAMMA, "gamma");
  ExerciseControl(&dev_, V4L2_CID_HUE, "hue");
  ExerciseControl(&dev_, V4L2_CID_GAIN, "gain");
  ExerciseControl(&dev_, V4L2_CID_SHARPNESS, "sharpness");
}

// SetCrop is optional.
TEST_F(V4L2Test, SetCrop) {
  v4l2_cropcap cropcap = {};
  if (dev_.GetCropCap(&cropcap)) {
    v4l2_crop crop = {};
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect;
    dev_.SetCrop(&crop);
  }
}

// GetCrop is optional.
TEST_F(V4L2Test, GetCrop) {
  v4l2_crop crop;
  memset(&crop, 0, sizeof(crop));
  crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  dev_.GetCrop(&crop);
}

TEST_F(V4L2Test, ProbeCaps) {
  v4l2_capability caps;
  ASSERT_TRUE(dev_.ProbeCaps(&caps, true));
  uint32_t dev_caps = (caps.capabilities & V4L2_CAP_DEVICE_CAPS)
                          ? caps.device_caps
                          : caps.capabilities;
  ASSERT_TRUE(dev_caps & V4L2_CAP_VIDEO_CAPTURE)
      << "Should support video capture interface";
}

TEST_F(V4L2Test, EnumFormats) {
  ASSERT_TRUE(dev_.EnumFormat(NULL));
}

TEST_F(V4L2Test, EnumFrameSize) {
  uint32_t format_count = 0;
  ASSERT_TRUE(dev_.EnumFormat(&format_count));
  for (uint32_t i = 0; i < format_count; i++) {
    uint32_t pixfmt;
    ASSERT_TRUE(dev_.GetPixelFormat(i, &pixfmt));
    ASSERT_TRUE(dev_.EnumFrameSize(pixfmt, NULL));
  }
}

TEST_F(V4L2Test, EnumFrameInterval) {
  uint32_t format_count = 0;
  ASSERT_TRUE(dev_.EnumFormat(&format_count));
  for (uint32_t i = 0; i < format_count; i++) {
    uint32_t pixfmt;
    ASSERT_TRUE(dev_.GetPixelFormat(i, &pixfmt));
    uint32_t size_count;
    ASSERT_TRUE(dev_.EnumFrameSize(pixfmt, &size_count));

    for (uint32_t j = 0; j < size_count; j++) {
      uint32_t width, height;
      ASSERT_TRUE(dev_.GetFrameSize(j, pixfmt, &width, &height));
      ASSERT_TRUE(dev_.EnumFrameInterval(pixfmt, width, height, NULL));
    }
  }
}

TEST_F(V4L2Test, FrameRate) {
  v4l2_streamparm param;
  ASSERT_TRUE(dev_.GetParam(&param));
  // we only try to adjust frame rate when it claims can.
  if (param.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
    ASSERT_TRUE(dev_.SetParam(&param));
  } else {
    LOG(INFO) << "Does not support TIMEPERFRAME";
  }
}

INSTANTIATE_TEST_CASE_P(V4L2Test,
                        V4L2TestWithIO,
                        ::testing::Values(V4L2Device::IO_METHOD_MMAP,
                                          V4L2Device::IO_METHOD_USERPTR));
}  // namespace tests
}  // namespace cros

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  DEFINE_string(test_list, "default", "Select different test list");
  DEFINE_string(device_path, "/dev/video0", "Path to the video device");

  // Add a newline at the beginning of the usage text to separate the help
  // message from gtest.
  brillo::FlagHelper::Init(argc, argv, "\nTest V4L2 camera functionalities.");

  cros::tests::g_env = reinterpret_cast<cros::tests::V4L2TestEnvironment*>(
      testing::AddGlobalTestEnvironment(new cros::tests::V4L2TestEnvironment(
          FLAGS_test_list, FLAGS_device_path)));

  return RUN_ALL_TESTS();
}
