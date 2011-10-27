// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/resolution_selector.h"

#include "base/logging.h"

namespace power_manager {

using std::vector;

const int ResolutionSelector::kMaxProjectorPixels = 1280 * 720;

ResolutionSelector::Mode::Mode(int width, int height, std::string name, int id)
    : width(width),
      height(height),
      name(name),
      id(id) {
}

ResolutionSelector::Mode::Mode()
    : width(0),
      height(0),
      name(""),
      id(0) {
}

ResolutionSelector::ResolutionSelector() {
}

ResolutionSelector::~ResolutionSelector() {
}

bool ResolutionSelector::FindBestResolutions(
    const vector<Mode>& lcd_modes,
    const vector<Mode>& external_modes,
    Mode* lcd_resolution,
    Mode* external_resolution,
    Mode* screen_resolution) {

  // On desktop variants, it's legit to have no display at all.
  if (lcd_modes.empty()) {
    LOG(INFO) << "We have no display at all";
    return true;
  }

  // If there's no external display, just use the highest resolution
  // available from the LCD.
  if (external_modes.empty()) {
    *lcd_resolution = *screen_resolution = lcd_modes[0];
    external_resolution->name.clear();
    LOG(INFO) << "We have no external display";
    return true;
  }

  const int max_lcd_size = lcd_modes[0].width * lcd_modes[0].height;
  const int max_external_size =
      external_modes[0].width * external_modes[0].height;

  if (max_lcd_size >= max_external_size) {
    return FindNearestResolutions(
        lcd_modes, external_modes,
        lcd_resolution, external_resolution, screen_resolution);
  } else {
    // If the external output is large enough that we think it's a monitor
    // (as opposed to a projector), then we just use its max resolution and
    // forget about trying to choose a screen size that'll fit on the
    // built-in display.
    if (max_external_size > kMaxProjectorPixels) {
      *external_resolution = *screen_resolution = external_modes[0];
      lcd_resolution->name.clear();
      return true;
    }
    return FindNearestResolutions(
        external_modes, lcd_modes,
        external_resolution, lcd_resolution, screen_resolution);
  }
}

bool ResolutionSelector::FindNearestResolutions(
    const vector<Mode>& larger_device_modes,
    const vector<Mode>& smaller_device_modes,
    Mode* larger_resolution,
    Mode* smaller_resolution,
    Mode* screen_resolution) {
  DCHECK(!larger_device_modes.empty());
  DCHECK(!smaller_device_modes.empty());
  DCHECK(larger_resolution);
  DCHECK(smaller_resolution);
  DCHECK(screen_resolution);

  // Start with the best that the smaller device has to offer.
  *smaller_resolution = smaller_device_modes[0];
  *screen_resolution = *smaller_resolution;
  int smaller_width = smaller_device_modes[0].width;
  int smaller_height = smaller_device_modes[0].height;

  for (vector<Mode>::const_reverse_iterator it =
           larger_device_modes.rbegin();
       it != larger_device_modes.rend(); ++it) {
    if (it->width >= smaller_width && it->height >= smaller_height) {
      *larger_resolution = *it;
      return true;
    }
  }

  LOG(WARNING) << "Failed to find a resolution from larger device "
               << "exceeding chosen resolution from smaller device ("
               << smaller_resolution->name << ")";
  return false;
}

}  // namespace power_manager
