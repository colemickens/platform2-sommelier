// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HAMMERD_HAMMER_UPDATER_H_
#define HAMMERD_HAMMER_UPDATER_H_

#include <string>

#include <base/macros.h>

#include "hammerd/update_fw.h"

namespace hammerd {

class HammerUpdater {
 public:
  enum class RunStatus {
    kNoUpdate,
    kFatalError,
    kNeedReset,
    kNeedJump,
  };

  explicit HammerUpdater(const std::string& image);
  virtual ~HammerUpdater() = default;
  virtual bool Run();
  virtual RunStatus RunOnce(const bool post_rw_jump);
  virtual void PostRWProcess();

 protected:
  // Used in unittests to inject mock instance.
  HammerUpdater(const std::string& image,
                std::shared_ptr<FirmwareUpdaterInterface> fw_updater);

  friend class HammerUpdaterTest;
  FRIEND_TEST(HammerUpdaterTest, Run_LoadImageFailed);
  FRIEND_TEST(HammerUpdaterTest, Run_AlwaysReset);
  FRIEND_TEST(HammerUpdaterTest, Run_FatalError);
  FRIEND_TEST(HammerUpdaterTest, Run_Reset3Times);
  FRIEND_TEST(HammerUpdaterTest, Run_UpdateRWAfterJumpToRWFailed);
  FRIEND_TEST(HammerUpdaterTest, RunOnce_UpdateRW);
  FRIEND_TEST(HammerUpdaterTest, RunOnce_UnlockRW);
  FRIEND_TEST(HammerUpdaterTest, RunOnce_JumpToRW);
  FRIEND_TEST(HammerUpdaterTest, RunOnce_CompleteRWJump);
  FRIEND_TEST(HammerUpdaterTest, RunOnce_KeepInRW);
  FRIEND_TEST(HammerUpdaterTest, RunOnce_ResetToRO);

 private:
  DISALLOW_COPY_AND_ASSIGN(HammerUpdater);

  // The image data to be updated.
  std::string image_;
  // The main firmware updater.
  std::shared_ptr<FirmwareUpdaterInterface> fw_updater_;
};

}  // namespace hammerd
#endif  // HAMMERD_HAMMER_UPDATER_H_
