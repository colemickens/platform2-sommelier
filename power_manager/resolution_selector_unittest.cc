// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "power_manager/resolution_selector.h"

namespace power_manager {

using std::vector;

class ResolutionSelectorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    lcd_resolution_ = new ResolutionSelector::Mode;
    external_resolution_ = new ResolutionSelector::Mode;
    screen_resolution_ = new ResolutionSelector::Mode;
  }

  // Add a mode to |lcd_modes_|.
  void AddLcdMode(int width, int height, int id) {
    lcd_modes_.push_back(
        ResolutionSelector::Mode(
            width, height, StringPrintf("%dx%d", width, height), id));
  }

  // Add a mode to |external_modes_|.
  void AddExternalMode(int width, int height, int id) {
    external_modes_.push_back(
        ResolutionSelector::Mode(
            width, height, StringPrintf("%dx%d", width, height), id));
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
  AddLcdMode(1024, 768, 50);
  AddLcdMode(800, 600, 51);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("1024x768", lcd_resolution_->name);
  EXPECT_EQ("", external_resolution_->name);
  EXPECT_EQ("1024x768", screen_resolution_->name);
}

// When both outputs have the same max resolution, we should use it.
TEST_F(ResolutionSelectorTest, MatchingTopResolutions) {
  AddLcdMode(1024, 768, 50);
  AddLcdMode(800, 600, 51);
  AddExternalMode(1024, 768, 60);
  AddExternalMode(800, 600, 61);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("1024x768", lcd_resolution_->name);
  EXPECT_EQ("1024x768", external_resolution_->name);
  EXPECT_EQ("1024x768", screen_resolution_->name);
}

// We should use the highest shared resolution when the LCD has the higher
// max resolution.
TEST_F(ResolutionSelectorTest, LcdHasHigherResolution) {
  AddLcdMode(1024, 768, 50);
  AddLcdMode(800, 600, 51);
  AddLcdMode(640, 480, 52);
  AddExternalMode(800, 600, 60);
  AddExternalMode(640, 480, 61);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("800x600", lcd_resolution_->name);
  EXPECT_EQ("800x600", external_resolution_->name);
  EXPECT_EQ("800x600", screen_resolution_->name);
}

// We should use the highest shared resolution when the external output has
// the higher max resolution.
TEST_F(ResolutionSelectorTest, ExternalHasHigherResolution) {
  AddLcdMode(800, 600, 50);
  AddLcdMode(640, 480, 51);
  AddExternalMode(1024, 768, 60);
  AddExternalMode(800, 600, 61);
  AddExternalMode(640, 480, 62);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("800x600", lcd_resolution_->name);
  EXPECT_EQ("800x600", external_resolution_->name);
  EXPECT_EQ("800x600", screen_resolution_->name);
}

// When the maximum resolution offered by the two outputs is different, we
// should use the max resolution from the lower-res output.
TEST_F(ResolutionSelectorTest, MismatchedMaxResolution) {
  AddLcdMode(1024, 600, 50);
  AddLcdMode(800, 600, 51);
  AddExternalMode(1280, 720, 60);
  AddExternalMode(1024, 768, 61);
  AddExternalMode(800, 600, 62);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("1024x600", lcd_resolution_->name);
  EXPECT_EQ("1024x768", external_resolution_->name);
  EXPECT_EQ("1024x600", screen_resolution_->name);
}

// When the external output is large enough that we think it's a monitor,
// we should just use its maximum resolution instead of trying to find a
// size that'll also work for the LCD output.
TEST_F(ResolutionSelectorTest, ExternalOutputIsMonitor) {
  AddLcdMode(1024, 768, 50);
  AddLcdMode(800, 600, 51);
  AddExternalMode(1600, 1200, 60);
  AddExternalMode(1280, 960, 61);
  AddExternalMode(1024, 768, 62);
  ASSERT_GE(external_modes_[0].width * external_modes_[0].height,
            ResolutionSelector::kMaxProjectorPixels);
  ASSERT_TRUE(GetResolutions());
  EXPECT_EQ("", lcd_resolution_->name);
  EXPECT_EQ("1600x1200", external_resolution_->name);
  EXPECT_EQ("1600x1200", screen_resolution_->name);
}

// We should just fail if there's no common resolution between the two
// outputs.
TEST_F(ResolutionSelectorTest, FailIfNoCommonResolution) {
  AddLcdMode(1024, 768, 50);
  AddExternalMode(1280, 600, 60);
  EXPECT_FALSE(GetResolutions());
}

}  // namespace power_manager
