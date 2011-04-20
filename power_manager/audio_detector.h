// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_AUDIO_DETECTOR_H_
#define POWER_MANAGER_AUDIO_DETECTOR_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "power_manager/audio_detector_interface.h"

namespace power_manager {

class AudioDetector : public AudioDetectorInterface {
 public:
  AudioDetector();
  virtual ~AudioDetector() {}

  void Init();

  // Overridden from AudioDetectorInterface.
  virtual bool GetAudioStatus(bool* is_active);

 private:
  FilePath audio_status_path_;

  DISALLOW_COPY_AND_ASSIGN(AudioDetector);
};

}  // namespace power_manager

#endif // POWER_MANAGER_AUDIO_DETECTOR_H_
