// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/audio_detector.h"

#include <cstring>

#include "base/file_util.h"
#include "base/logging.h"

namespace {

const char kAudioBasePath[] = "/proc/asound/card0/pcm0p";

}  // namespace

namespace power_manager {

AudioDetector::AudioDetector() {}

void AudioDetector::Init() {
  // TODO(sque): We can make this more flexible to accommodate different sysfs.
  FilePath base_path(kAudioBasePath);
  audio_status_path_ = base_path.Append("sub0/status");
}

bool AudioDetector::GetAudioStatus(bool* is_active) {
  CHECK(is_active != NULL);
  if (!file_util::PathExists(audio_status_path_))
    return false;

  // Read and parse the contents of the audio status file.
  std::string sound_buf;
  bool ok = file_util::ReadFileToString(audio_status_path_, &sound_buf);
  if (strstr(sound_buf.c_str(), "closed"))  // Audio is inactive.
    *is_active = false;
  else if (strstr(sound_buf.c_str(), "state: RUNNING"))  // Audio is active.
    *is_active = true;
  else  // Could not determine audio state.
    ok = false;

  return ok;
}

}  // namespace power_manager
