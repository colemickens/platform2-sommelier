// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_AUDIO_DETECTOR_INTERFACE_H_
#define POWER_MANAGER_AUDIO_DETECTOR_INTERFACE_H_

namespace power_manager {

// Interface for detecting the presence of audio during user idle periods.
class AudioDetectorInterface {
 public:
  // Sets |is_active| to true if audio activity has been detected, otherwise
  // sets |is_active| to false.
  //
  // On success, returns true; Returns false if the audio state could not be
  // determined or the audio file could not be read.
  virtual bool GetAudioStatus(bool* is_active) = 0;

 protected:
  virtual ~AudioDetectorInterface() {}
};

}  // namespace power_manager

#endif // POWER_MANAGER_AUDIO_DETECTOR_INTERFACE_H_
