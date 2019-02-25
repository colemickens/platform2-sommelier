// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_CODEC_TEST_COMMON_H_
#define ARC_CODEC_TEST_COMMON_H_

#include <fstream>
#include <string>
#include <vector>

namespace android {

// The enumeration of video codec profile. This would be better to align with
// VideoCodecProfile enum in Chromium so we could use the identical test stream
// data arguments for both ARC end-to-end and Chromium tests.
enum VideoCodecProfile {
  VIDEO_CODEC_PROFILE_UNKNOWN = -1,
  VIDEO_CODEC_PROFILE_MIN = VIDEO_CODEC_PROFILE_UNKNOWN,
  H264PROFILE_MIN = 0,
  H264PROFILE_BASELINE = H264PROFILE_MIN,
  H264PROFILE_MAIN = 1,
  H264PROFILE_EXTENDED = 2,
  H264PROFILE_HIGH = 3,
  H264PROFILE_HIGH10PROFILE = 4,
  H264PROFILE_HIGH422PROFILE = 5,
  H264PROFILE_HIGH444PREDICTIVEPROFILE = 6,
  H264PROFILE_SCALABLEBASELINE = 7,
  H264PROFILE_SCALABLEHIGH = 8,
  H264PROFILE_STEREOHIGH = 9,
  H264PROFILE_MULTIVIEWHIGH = 10,
  H264PROFILE_MAX = H264PROFILE_MULTIVIEWHIGH,
  VP8PROFILE_MIN = 11,
  VP8PROFILE_ANY = VP8PROFILE_MIN,
  VP8PROFILE_MAX = VP8PROFILE_ANY,
  VP9PROFILE_MIN = 12,
  VP9PROFILE_PROFILE0 = VP9PROFILE_MIN,
  VP9PROFILE_PROFILE1 = 13,
  VP9PROFILE_PROFILE2 = 14,
  VP9PROFILE_PROFILE3 = 15,
  VP9PROFILE_MAX = VP9PROFILE_PROFILE3,
};

// The enum class of video codec type.
enum class VideoCodecType {
  UNKNOWN,
  H264,
  VP8,
  VP9,
};

// Structure to store resolution.
struct Size {
  Size() : width(0), height(0) {}
  Size(int w, int h) : width(w), height(h) {}
  bool IsEmpty() const { return width <= 0 || height <= 0; }

  int width;
  int height;
};

// Wrapper of std::ifstream.
class InputFileStream {
 public:
  explicit InputFileStream(std::string file_path);

  // Check if the file is valid.
  bool IsValid() const;
  // Get the size of the file.
  size_t GetLength();
  // Set position to the beginning of the file.
  void Rewind();
  // Read the given number of bytes to the buffer. Return the number of bytes
  // read or -1 on error.
  size_t Read(char* buffer, size_t size);

 private:
  std::ifstream file_;
};

// Helper function to get VideoCodecType from |profile|.
VideoCodecType VideoCodecProfileToType(VideoCodecProfile profile);

// Split the string |src| by the delimiter |delim|.
std::vector<std::string> SplitString(const std::string& src, char delim);

// Get monotonic timestamp for now in microseconds.
int64_t GetNowUs();

// Get Mime type name from video codec type.
const char* GetMimeType(VideoCodecType type);

}  // namespace android
#endif  // ARC_CODEC_TEST_COMMON_H_
