// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>

#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "arc/codec-test/common.h"
#include "arc/codec-test/mediacodec_decoder.h"

namespace android {

// Environment to store test video data for all test cases.
class ArcVideoDecoderTestEnvironment;

namespace {
ArcVideoDecoderTestEnvironment* g_env;
}  // namespace

class ArcVideoDecoderTestEnvironment : public testing::Environment {
 public:
  explicit ArcVideoDecoderTestEnvironment(const std::string& data,
                                          const std::string& output_frames_path)
      : test_video_data_(data), output_frames_path_(output_frames_path) {}

  void SetUp() override { ParseTestVideoData(); }

  // The syntax of test video data is:
  // "input_file_path:width:height:num_frames:num_fragments:min_fps_render:
  //  min_fps_no_render:video_codec_profile[:output_file_path]"
  // - |input_file_path| is compressed video stream in H264 Annex B (NAL) format
  //   (H264) or IVF (VP8/9).
  // - |width| and |height| are visible frame size in pixels.
  // - |num_frames| is the number of picture frames for the input stream.
  // - |num_fragments| is the number of AU (H264) or frame (VP8/9) in the input
  //   stream. (Unused. Test will automatically parse the number.)
  // - |min_fps_render| and |min_fps_no_render| are minimum frames/second speeds
  //   expected to be achieved with and without rendering respective.
  //   (The former is unused because no rendering case here.)
  //   (The latter is Optional.)
  // - |video_codec_profile| is the VideoCodecProfile set during Initialization.
  void ParseTestVideoData() {
    std::vector<std::string> fields = SplitString(test_video_data_, ':');
    ASSERT_EQ(fields.size(), 8U)
        << "The number of fields of test_video_data is not 8: "
        << test_video_data_;

    input_file_path_ = fields[0];
    int width = std::stoi(fields[1]);
    int height = std::stoi(fields[2]);
    visible_size_ = Size(width, height);
    ASSERT_FALSE(visible_size_.IsEmpty());

    num_frames_ = std::stoi(fields[3]);
    ASSERT_GT(num_frames_, 0);

    // Unused fields[4] --> num_fragments
    // Unused fields[5] --> min_fps_render

    if (!fields[6].empty()) {
      min_fps_no_render_ = std::stoi(fields[6]);
    }

    video_codec_profile_ = static_cast<VideoCodecProfile>(std::stoi(fields[7]));
    ASSERT_NE(VideoCodecProfileToType(video_codec_profile_),
              VideoCodecType::UNKNOWN);
  }

  std::string output_frames_path() const { return output_frames_path_; }

  std::string input_file_path() const { return input_file_path_; }
  Size visible_size() const { return visible_size_; }
  int num_frames() const { return num_frames_; }
  int min_fps_no_render() const { return min_fps_no_render_; }
  VideoCodecProfile video_codec_profile() const { return video_codec_profile_; }

 protected:
  std::string test_video_data_;
  std::string output_frames_path_;

  std::string input_file_path_;
  Size visible_size_;
  int num_frames_ = 0;
  int min_fps_no_render_ = 0;
  VideoCodecProfile video_codec_profile_;
};

class ArcVideoDecoderE2ETest : public testing::Test {
 public:
  //  Callback function of output buffer ready to count frame.
  void CountFrame(const uint8_t* /* data */, size_t /* buffer_size */) {
    decoded_frames_++;
  }

  // Callback function of output buffer ready to write buffer into file, as well
  // as count frame.
  void WriteOutputToFile(const uint8_t* data, size_t buffer_size) {
    CountFrame(data, buffer_size);

    // TODO(johnylin): only write pixels in visible size to file and check
    //                 frame-wise md5sum. b/112741393
    output_file_.write(reinterpret_cast<const char*>(data), buffer_size);
    if (output_file_.fail()) {
      printf("[ERR] Failed to write output buffer into file.\n");
    }
  }

  // Callback function of output format changed to verify output format.
  void VerifyOutputFormat(const Size& coded_size,
                          const Size& visible_size,
                          int32_t color_format) {
    ASSERT_FALSE(coded_size.IsEmpty());
    ASSERT_FALSE(visible_size.IsEmpty());
    ASSERT_LE(visible_size.width, coded_size.width);
    ASSERT_LE(visible_size.height, coded_size.height);
    printf(
        "[LOG] Got format changed { coded_size: %dx%d, visible_size: %dx%d, "
        "color_format: 0x%x\n",
        coded_size.width, coded_size.height, visible_size.width,
        visible_size.height, color_format);
    visible_size_ = visible_size;
  }

 protected:
  void SetUp() override {
    decoder_ = MediaCodecDecoder::Create(g_env->input_file_path(),
                                         g_env->video_codec_profile(),
                                         g_env->visible_size());

    ASSERT_TRUE(decoder_);
    decoder_->Rewind();

    ASSERT_TRUE(decoder_->Configure());
    ASSERT_TRUE(decoder_->Start());
  }

  void TearDown() override {
    EXPECT_TRUE(decoder_->Stop());

    EXPECT_EQ(g_env->visible_size().width, visible_size_.width);
    EXPECT_EQ(g_env->visible_size().height, visible_size_.height);
    EXPECT_EQ(g_env->num_frames(), decoded_frames_);

    output_file_.close();
    decoder_.reset();
  }

  bool CreateOutputFile() {
    if (g_env->output_frames_path().empty())
      return false;

    output_file_.open(g_env->output_frames_path(), std::ofstream::binary);
    if (!output_file_.is_open()) {
      printf("[ERR] Failed to open file: %s\n",
             g_env->output_frames_path().c_str());
      return false;
    }
    printf("[LOG] Decode output to file: %s\n",
           g_env->output_frames_path().c_str());
    return true;
  }

  // The wrapper of the mediacodec decoder.
  std::unique_ptr<MediaCodecDecoder> decoder_;

  // The output file to write the decoded raw video.
  std::ofstream output_file_;
  // The counter of obtained decoded output frames.
  int decoded_frames_ = 0;
  // This records visible size from output format change.
  Size visible_size_;
};

TEST_F(ArcVideoDecoderE2ETest, TestSimpleDecode) {
  if (CreateOutputFile()) {
    decoder_->SetOutputBufferReadyCb(
        std::bind(&ArcVideoDecoderE2ETest::WriteOutputToFile, this,
                  std::placeholders::_1, std::placeholders::_2));
  } else {
    decoder_->SetOutputBufferReadyCb(
        std::bind(&ArcVideoDecoderE2ETest::CountFrame, this,
                  std::placeholders::_1, std::placeholders::_2));
  }
  decoder_->SetOutputFormatChangedCb(std::bind(
      &ArcVideoDecoderE2ETest::VerifyOutputFormat, this, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3));

  EXPECT_TRUE(decoder_->Decode());
}

TEST_F(ArcVideoDecoderE2ETest, TestFPS) {
  decoder_->SetOutputBufferReadyCb(
      std::bind(&ArcVideoDecoderE2ETest::CountFrame, this,
                std::placeholders::_1, std::placeholders::_2));
  decoder_->SetOutputFormatChangedCb(std::bind(
      &ArcVideoDecoderE2ETest::VerifyOutputFormat, this, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3));

  int64_t time_before_decode_us = GetNowUs();
  EXPECT_TRUE(decoder_->Decode());
  int64_t total_decode_time_us = GetNowUs() - time_before_decode_us;

  double fps = decoded_frames_ * 1E6 / total_decode_time_us;
  printf("[LOG] Measured decoder FPS: %.4f\n", fps);
  // TODO(johnylin): improve FPS calculation by CTS method and then enable the
  //                 following check.
  // EXPECT_GE(fps, static_cast<double>(g_env->min_fps_no_render()));
}

}  // namespace android

bool GetOption(int argc,
               char** argv,
               std::string* test_video_data,
               std::string* output_frames_path) {
  const char* const optstring = "to:";
  static const struct option opts[] = {
      {"test_video_data", required_argument, nullptr, 't'},
      {"output_frames_path", required_argument, nullptr, 'o'},
      {nullptr, 0, nullptr, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, optstring, opts, nullptr)) != -1) {
    switch (opt) {
      case 't':
        *test_video_data = optarg;
        break;
      case 'o':
        *output_frames_path = optarg;
        break;
      default:
        printf("[WARN] Unknown option: getopt_long() returned code 0x%x.\n",
               opt);
        break;
    }
  }

  if (test_video_data->empty()) {
    printf("[ERR] Please assign test video data by --test_video_data\n");
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  std::string test_video_data;
  std::string output_frames_path;
  if (!GetOption(argc, argv, &test_video_data, &output_frames_path))
    return EXIT_FAILURE;

  android::g_env = reinterpret_cast<android::ArcVideoDecoderTestEnvironment*>(
      testing::AddGlobalTestEnvironment(
          new android::ArcVideoDecoderTestEnvironment(test_video_data,
                                                      output_frames_path)));
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
