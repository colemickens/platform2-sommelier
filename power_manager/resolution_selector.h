// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_RESOLUTION_SELECTOR_H_
#define POWER_MANAGER_RESOLUTION_SELECTOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace power_manager {

// ResolutionSelector takes the sets of resolutions supported by the
// built-in and external displays as input and attempts to choose a shared
// resolution that will work well on both devices.
class ResolutionSelector {
 public:
  // A single mode supported by a device, equivalent to the XRRModeInfo
  // struct.
  struct Mode {
    Mode(int width, int height, std::string name, int id, bool preferred);
    Mode();

    // Mode's dimensions.
    int width;
    int height;

    // Mode's name from XRandR.  This uniquely describes the mode and can
    // be used to set the devices's resolution later.
    std::string name;

    // The mode id, used for setting this mode.
    unsigned id;

    // Whether this is a preferred mode.
    bool preferred;
  };

  // Maximum screen size for the external output at which we assume that
  // it's a projector (as opposed to a monitor) and try to find a size that
  // will also fit on the LCD display.  Above this, we just use the
  // external output's maximum resolution, even if it doesn't fit on the
  // LCD.
  static const int kMaxProjectorPixels;

  ResolutionSelector();
  ~ResolutionSelector();

  // Comparator used to sort Mode objects.
  // Returns true if |mode_a| has more pixels than |mode_b| and false if
  // |mode_a| has less pixels than |mode_b|.  In the case of a tie, will
  // return true if |mode_a| is preferred and |mode_b| is not.  Returns
  // false otherwise.  In other words, in the case of the same number of
  // pixels, preferred modes are considered "greater" than nonpreferred modes.
  class ModeResolutionComparator {
   public:
    bool operator()(const Mode& mode_a, const Mode& mode_b) const {
      if (mode_a.width * mode_a.height > mode_b.width * mode_b.height)
        return true;
      else if (mode_a.width * mode_a.height < mode_b.width * mode_b.height)
        return false;
      else
        return mode_a.preferred > mode_b.preferred;
    }
  };

  // Find the "best" resolutions for various outputs.
  // Returns the modes for both screens and the total screen resolution.
  bool FindBestResolutions(
      const std::vector<Mode>& lcd_modes,
      const std::vector<Mode>& external_modes,
      Mode* lcd_resolution,
      Mode* external_resolution,
      Mode* screen_resolution);

  // Find the best common resolutions for 2 outputs.
  // Returns the modes for both screens and the screen resolution.
  bool FindCommonResolutions(
      const std::vector<Mode>& lcd_modes,
      const std::vector<Mode>& external_modes,
      Mode* lcd_resolution,
      Mode* external_resolution,
      Mode* screen_resolution);

 private:
  // Find resolutions to use that are reasonably close together.
  // |larger_device_modes| and |smaller_device_modes| should be sorted by
  // descending resolution.  We choose the highest resolution from
  // |smaller_device_modes| and the lowest resolution from |larger_device_modes|
  // that's at least as high as the resolution from the smaller device.
  // |screen_resolution| gets set to |smaller_resolution| to avoid clipping.
  bool FindNearestResolutions(
      const std::vector<Mode>& larger_device_modes,
      const std::vector<Mode>& smaller_device_modes,
      Mode* larger_resolution,
      Mode* smaller_resolution,
      Mode* screen_resolution);

  DISALLOW_COPY_AND_ASSIGN(ResolutionSelector);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_RESOLUTION_SELECTOR_H_
