// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_INIT_PROCESS_REAPER_H_
#define GERM_INIT_PROCESS_REAPER_H_

#include <sys/wait.h>

#include <base/callback.h>
#include <base/macros.h>

#include "germ/process_reaper.h"

namespace germ {

// Process reaper which exits when its last child terminates.
class InitProcessReaper : public ProcessReaper {
 public:
  explicit InitProcessReaper(base::Closure quit_closure);
  ~InitProcessReaper();

 private:
  void HandleReapedChild(const siginfo_t& info) override;
  bool NoUnwaitedForChildren();

  base::Closure quit_closure_;
  DISALLOW_COPY_AND_ASSIGN(InitProcessReaper);
};

}  // namespace germ

#endif  // GERM_INIT_PROCESS_REAPER_H_
