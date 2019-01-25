// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #define LOG_NDEBUG 0
#define LOG_TAG "ArcVideoEncoderE2ETest"

#include <getopt.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <utils/Log.h>

#include "arc/codec-test/common.h"
#include "arc/codec-test/mediacodec_encoder.h"

namespace android {

// Environment to store test stream data for all test cases.
class ArcVideoEncoderTestEnvironment;

namespace {
// Default initial bitrate.
const unsigned int kDefaultBitrate = 2000000;
// Default ratio of requested_subsequent_bitrate_ to initial_bitrate
// (see test parameters below) if one is not provided.
const double kDefaultSubsequentBitrateRatio = 2.0;
// Default initial framerate.
const unsigned int kDefaultFramerate = 30;
// Default ratio of requested_subsequent_framerate_ to initial_framerate
// (see test parameters below) if one is not provided.
const double kDefaultSubsequentFramerateRatio = 0.1;
// Tolerance factor for how encoded bitrate can differ from requested bitrate.
const double kBitrateTolerance = 0.1;
// The minimum number of encoded frames. If the frame number of the input stream
// is less than this value, then we circularly encode the input stream.
constexpr size_t kMinNumEncodedFrames = 300;

ArcVideoEncoderTestEnvironment* g_env;
}  // namespace

class ArcVideoEncoderTestEnvironment : public testing::Environment {
 public:
  explicit ArcVideoEncoderTestEnvironment(const std::string& data)
      : test_stream_data_(data) {}

  void SetUp() override { ParseTestStreamData(); }

  // The syntax of test stream is:
  // "input_file_path:width:height:profile:output_file_path:requested_bitrate
  //  :requested_framerate:requestedSubsequentBitrate
  //  :requestedSubsequentFramerate:pixelFormat"
  // - |input_file_path| is YUV raw stream. Its format must be |pixelFormat|
  //   (see http://www.fourcc.org/yuv.php#IYUV).
  // - |width| and |height| are in pixels.
  // - |profile| to encode into (values of VideoCodecProfile).
  //   NOTE: Only H264PROFILE_MAIN(1) is supported. Now we ignore this value.
  // - |output_file_path| filename to save the encoded stream to (optional).
  //   The format for H264 is Annex-B byte stream.
  // - |requested_bitrate| requested bitrate in bits per second.
  //   Bitrate is only forced for tests that test bitrate.
  // - |requested_framerate| requested initial framerate.
  // - |requestedSubsequentBitrate| bitrate to switch to in the middle of the
  //   stream. NOTE: This value is not supported yet.
  // - |requestedSubsequentFramerate| framerate to switch to in the middle of
  //   the stream. NOTE: This value is not supported yet.
  // - |pixelFormat| is the VideoPixelFormat of |input_file_path|.
  //   NOTE: Only PIXEL_FORMAT_I420 is supported. Now we just ignore this value.
  void ParseTestStreamData() {
    std::vector<std::string> fields = SplitString(test_stream_data_, ':');
    ALOG_ASSERT(fields.size() >= 3U,
                "The fields of test_stream_data is not enough: %s",
                test_stream_data_.c_str());
    ALOG_ASSERT(fields.size() <= 10U,
                "The fields of test_stream_data is too much: %s",
                test_stream_data_.c_str());

    input_file_path_ = fields[0];
    int width = std::stoi(fields[1]);
    int height = std::stoi(fields[2]);
    visible_size_ = Size(width, height);
    ASSERT_FALSE(visible_size_.IsEmpty());

    if (fields.size() >= 4 && !fields[3].empty()) {
      int profile = stoi(fields[3]);
      if (profile != 1)
        ALOGW("Only H264PROFILE_MAIN(1) is supported.");
    }

    if (fields.size() >= 5 && !fields[4].empty()) {
      output_file_path_ = fields[4];
    }

    if (fields.size() >= 6 && !fields[5].empty()) {
      requested_bitrate_ = stoi(fields[5]);
      ASSERT_GT(requested_bitrate_, 0);
    } else {
      requested_bitrate_ = kDefaultBitrate;
    }

    if (fields.size() >= 7 && !fields[6].empty()) {
      requested_framerate_ = std::stoi(fields[6]);
      ASSERT_GT(requested_framerate_, 0);
    } else {
      requested_framerate_ = kDefaultFramerate;
    }

    if (fields.size() >= 8 && !fields[7].empty()) {
      requested_subsequent_bitrate_ = std::stoi(fields[7]);
      ASSERT_GT(requested_subsequent_bitrate_, 0);
    } else {
      requested_subsequent_bitrate_ =
          requested_bitrate_ * kDefaultSubsequentBitrateRatio;
    }

    if (fields.size() >= 9 && !fields[8].empty()) {
      requested_subsequent_framerate_ = std::stoi(fields[8]);
      ASSERT_GT(requested_subsequent_framerate_, 0);
    } else {
      requested_subsequent_framerate_ =
          requested_framerate_ * kDefaultSubsequentFramerateRatio;
    }

    if (fields.size() >= 10 && !fields[9].empty()) {
      int format = std::stoi(fields[9]);
      if (format != 1 /* PIXEL_FORMAT_I420 */)
        ALOGW("Only I420 is suppported.");
    }
  }

  Size visible_size() const { return visible_size_; }
  std::string input_file_path() const { return input_file_path_; }
  std::string output_file_path() const { return output_file_path_; }
  int requested_bitrate() const { return requested_bitrate_; }
  int requested_framerate() const { return requested_framerate_; }
  int requested_subsequent_bitrate() const {
    return requested_subsequent_bitrate_;
  }
  int requested_subsequent_framerate() const {
    return requested_subsequent_framerate_;
  }

 private:
  std::string test_stream_data_;

  Size visible_size_;
  std::string input_file_path_;
  std::string output_file_path_;

  int requested_bitrate_;
  int requested_framerate_;
  int requested_subsequent_bitrate_;
  int requested_subsequent_framerate_;
};

class ArcVideoEncoderE2ETest : public testing::Test {
 public:
  // Callback functions of getting output buffers from encoder.
  void WriteOutputBufferToFile(const uint8_t* data, size_t size) {
    if (output_file_.is_open()) {
      output_file_.write(reinterpret_cast<const char*>(data), size);
      if (output_file_.fail()) {
        ALOGE("Failed to write encoded buffer into file.");
      }
    }
  }

  void AccumulateOutputBufferSize(const uint8_t* /* data */, size_t size) {
    total_output_buffer_size_ += size;
  }

 protected:
  void SetUp() override {
    encoder_ = MediaCodecEncoder::Create(g_env->input_file_path(),
                                         g_env->visible_size());
    ASSERT_TRUE(encoder_);
    encoder_->Rewind();
  }

  void TearDown() override {
    output_file_.close();
    encoder_.reset();
  }

  bool CreateOutputFile() {
    if (g_env->output_file_path().empty())
      return false;

    output_file_.open(g_env->output_file_path(), std::ofstream::binary);
    if (!output_file_.is_open()) {
      ALOGE("Failed to open file: %s", g_env->output_file_path().c_str());
      return false;
    }
    return true;
  }

  double CalculateAverageBitrate(size_t num_frames,
                                 unsigned int framerate) const {
    return 1.0f * total_output_buffer_size_ * 8 / num_frames * framerate;
  }

  // The wrapper of the mediacodec encoder.
  std::unique_ptr<MediaCodecEncoder> encoder_;

  // The output file to write the encoded video bitstream.
  std::ofstream output_file_;
  // Used to accumulate the output buffer size.
  size_t total_output_buffer_size_;
};

TEST_F(ArcVideoEncoderE2ETest, TestSimpleEncode) {
  // Write the output buffers to file.
  if (CreateOutputFile()) {
    encoder_->SetOutputBufferReadyCb(
        std::bind(&ArcVideoEncoderE2ETest::WriteOutputBufferToFile, this,
                  std::placeholders::_1, std::placeholders::_2));
  }

  ASSERT_TRUE(
      encoder_->Configure(static_cast<int32_t>(g_env->requested_bitrate()),
                          static_cast<int32_t>(g_env->requested_framerate())));
  ASSERT_TRUE(encoder_->Start());
  EXPECT_TRUE(encoder_->Encode());
  EXPECT_TRUE(encoder_->Stop());
}

TEST_F(ArcVideoEncoderE2ETest, TestBitrate) {
  // Ensure the number of encoded frames is enough for bitrate test case.
  encoder_->set_num_encoded_frames(
      std::max(encoder_->num_encoded_frames(), kMinNumEncodedFrames));

  // Accumulate the size of the output buffers.
  total_output_buffer_size_ = 0;
  encoder_->SetOutputBufferReadyCb(
      std::bind(&ArcVideoEncoderE2ETest::AccumulateOutputBufferSize, this,
                std::placeholders::_1, std::placeholders::_2));

  // TODO(akahuang): Verify bitrate switch at the middle of stream.
  ASSERT_TRUE(
      encoder_->Configure(static_cast<int32_t>(g_env->requested_bitrate()),
                          static_cast<int32_t>(g_env->requested_framerate())));
  ASSERT_TRUE(encoder_->Start());
  EXPECT_TRUE(encoder_->Encode());
  EXPECT_TRUE(encoder_->Stop());

  double measured_bitrate = CalculateAverageBitrate(
      encoder_->num_encoded_frames(), g_env->requested_framerate());
  EXPECT_NEAR(measured_bitrate, g_env->requested_bitrate(),
              kBitrateTolerance * g_env->requested_bitrate());
}

}  // namespace android

bool GetOption(int argc, char** argv, std::string* test_stream_data) {
  const char* const optstring = "t:";
  static const struct option opts[] = {
      {"test_stream_data", required_argument, nullptr, 't'},
      {nullptr, 0, nullptr, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, optstring, opts, nullptr)) != -1) {
    switch (opt) {
      case 't':
        *test_stream_data = optarg;
        break;
      default:
        ALOGW("Unknown option: getopt_long() returned code 0x%x.", opt);
        break;
    }
  }

  if (test_stream_data->empty()) {
    ALOGE("Please assign test stream data by --test_stream_data");
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  std::string test_stream_data;
  if (!GetOption(argc, argv, &test_stream_data))
    return EXIT_FAILURE;

  android::g_env = reinterpret_cast<android::ArcVideoEncoderTestEnvironment*>(
      testing::AddGlobalTestEnvironment(
          new android::ArcVideoEncoderTestEnvironment(test_stream_data)));
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
