// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "power_manager/resolution_selector.h"

namespace power_manager {

using std::sort;
using std::vector;

class ResolutionSelectorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    lcd_resolution_ = new ResolutionSelector::Mode;
    external_resolution_ = new ResolutionSelector::Mode;
    screen_resolution_ = new ResolutionSelector::Mode;
  }

  // Add a mode to |lcd_modes_|.  The new mode will be inserted in sorted
  // order from greatest to least according to
  // ResolutionSelector::ModeResolutionComparator(), which simulates the
  // sorting done by MonitorReconfigure before choosing the resolution.
  void AddLcdMode(int width, int height, int id, bool preferred) {
    lcd_modes_.push_back(
        ResolutionSelector::Mode(
            width, height,
            StringPrintf("%dx%d", width, height),
            id, preferred));

    sort(lcd_modes_.begin(), lcd_modes_.end(),
         ResolutionSelector::ModeResolutionComparator());
  }

  // Add a mode to |external_modes_|.  The new mode will be inserted in sorted
  // order from greatest to least according to
  // ResolutionSelector::ModeResolutionComparator(), which simulates the
  // sorting done by MonitorReconfigure before choosing the resolution.
  void AddExternalMode(int width, int height, int id, bool preferred) {
    external_modes_.push_back(
        ResolutionSelector::Mode(
            width, height,
            StringPrintf("%dx%d", width, height),
            id, preferred));

    sort(external_modes_.begin(), external_modes_.end(),
         ResolutionSelector::ModeResolutionComparator());
  }

  // Ask |selector_| for the best resolutions and store them in
  // |lcd_resolution_|, |external_resolution_|, and |screen_resolution_|.
  // The return value from |FindBestResolutions()| is returned.
  bool GetResolutions() {
    return selector_.FindBestResolutions(lcd_modes_,
                                         external_modes_,
                                         lcd_resolution_,
                                         external_resolution_,
                                         screen_resolution_);
  }

  ResolutionSelector selector_;

  vector<ResolutionSelector::Mode> lcd_modes_;
  vector<ResolutionSelector::Mode> external_modes_;

  ResolutionSelector::Mode* lcd_resolution_;
  ResolutionSelector::Mode* external_resolution_;
  ResolutionSelector::Mode* screen_resolution_;
};

// We should use the LCD's max resolution when there's no external output
// connected.
TEST_F(ResolutionSelectorTest, NoExternalOutput) {
  AddLcdMode(1024, 768, 50, false);
  AddLcdMode(800, 600, 51, false);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("1024x768", lcd_resolution_->name);
  EXPECT_EQ("", external_resolution_->name);
  EXPECT_EQ("1024x768", screen_resolution_->name);
}

// When both outputs have the same max resolution, we should use it.
TEST_F(ResolutionSelectorTest, MatchingTopResolutions) {
  AddLcdMode(1024, 768, 50, false);
  AddLcdMode(800, 600, 51, false);
  AddExternalMode(1024, 768, 60, false);
  AddExternalMode(800, 600, 61, false);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("1024x768", lcd_resolution_->name);
  EXPECT_EQ("1024x768", external_resolution_->name);
  EXPECT_EQ("1024x768", screen_resolution_->name);
}

// We should use the highest shared resolution when the LCD has the higher
// max resolution.
TEST_F(ResolutionSelectorTest, LcdHasHigherResolution) {
  AddLcdMode(1024, 768, 50, false);
  AddLcdMode(800, 600, 51, false);
  AddLcdMode(640, 480, 52, false);
  AddExternalMode(800, 600, 60, false);
  AddExternalMode(640, 480, 61, false);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("800x600", lcd_resolution_->name);
  EXPECT_EQ("800x600", external_resolution_->name);
  EXPECT_EQ("800x600", screen_resolution_->name);
}

// We should use the highest shared resolution when the external output has
// the higher max resolution.
TEST_F(ResolutionSelectorTest, ExternalHasHigherResolution) {
  AddLcdMode(800, 600, 50, false);
  AddLcdMode(640, 480, 51, false);
  AddExternalMode(1024, 768, 60, false);
  AddExternalMode(800, 600, 61, false);
  AddExternalMode(640, 480, 62, false);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("800x600", lcd_resolution_->name);
  EXPECT_EQ("800x600", external_resolution_->name);
  EXPECT_EQ("800x600", screen_resolution_->name);
}

// When the maximum resolution offered by the two outputs is different, we
// should use the max resolution from the lower-res output.
TEST_F(ResolutionSelectorTest, MismatchedMaxResolution) {
  AddLcdMode(1024, 600, 50, false);
  AddLcdMode(800, 600, 51, false);
  AddExternalMode(1280, 720, 60, false);
  AddExternalMode(1024, 768, 61, false);
  AddExternalMode(800, 600, 62, false);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("1024x600", lcd_resolution_->name);
  EXPECT_EQ("1024x768", external_resolution_->name);
  EXPECT_EQ("1024x600", screen_resolution_->name);
}

// When the external output is large enough that we think it's a monitor,
// we should just use its maximum resolution instead of trying to find a
// size that'll also work for the LCD output.
TEST_F(ResolutionSelectorTest, ExternalOutputIsMonitor) {
  AddLcdMode(1024, 768, 50, false);
  AddLcdMode(800, 600, 51, false);
  AddExternalMode(1600, 1200, 60, false);
  AddExternalMode(1280, 960, 61, false);
  AddExternalMode(1024, 768, 62, false);
  ASSERT_GE(external_modes_[0].width * external_modes_[0].height,
            ResolutionSelector::kMaxProjectorPixels);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("", lcd_resolution_->name);
  EXPECT_EQ("1600x1200", external_resolution_->name);
  EXPECT_EQ("1600x1200", screen_resolution_->name);
}

// When multiple modes have the same number of pixels, if one or more of them
// are preferred modes, make sure a preferred mode ends up being selected.
TEST_F(ResolutionSelectorTest, ExternalOutputPreferredMode0) {
  AddLcdMode(1024, 768, 50, false);
  AddLcdMode(800, 600, 51, false);
  AddExternalMode(1600, 1200, 60, true);
  AddExternalMode(1600, 1200, 61, false);
  AddExternalMode(1600, 1200, 62, false);
  ASSERT_GE(external_modes_[0].width * external_modes_[0].height,
            ResolutionSelector::kMaxProjectorPixels);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("", lcd_resolution_->name);
  EXPECT_EQ(60, external_resolution_->id);
  EXPECT_EQ(60, screen_resolution_->id);
}
TEST_F(ResolutionSelectorTest, ExternalOutputPreferredMode1) {
  AddLcdMode(1024, 768, 50, false);
  AddLcdMode(800, 600, 51, false);
  AddExternalMode(1600, 1200, 60, false);
  AddExternalMode(1600, 1200, 61, true);
  AddExternalMode(1600, 1200, 62, false);
  ASSERT_GE(external_modes_[0].width * external_modes_[0].height,
            ResolutionSelector::kMaxProjectorPixels);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("", lcd_resolution_->name);
  EXPECT_EQ(61, external_resolution_->id);
  EXPECT_EQ(61, screen_resolution_->id);
}
TEST_F(ResolutionSelectorTest, ExternalOutputPreferredMode2) {
  AddLcdMode(1024, 768, 50, false);
  AddLcdMode(800, 600, 51, false);
  AddExternalMode(1600, 1200, 60, false);
  AddExternalMode(1600, 1200, 61, false);
  AddExternalMode(1600, 1200, 62, true);
  ASSERT_GE(external_modes_[0].width * external_modes_[0].height,
            ResolutionSelector::kMaxProjectorPixels);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("", lcd_resolution_->name);
  EXPECT_EQ(62, external_resolution_->id);
  EXPECT_EQ(62, screen_resolution_->id);
}

// We should just fail if there's no common resolution between the two
// outputs.
TEST_F(ResolutionSelectorTest, FailIfNoCommonResolution) {
  AddLcdMode(1024, 768, 50, false);
  AddExternalMode(1280, 600, 60, false);
  EXPECT_FALSE(GetResolutions());
}

}  // namespace power_manager
