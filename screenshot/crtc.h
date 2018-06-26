// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SCREENSHOT_CRTC_H_
#define SCREENSHOT_CRTC_H_

#include <memory>

#include <base/files/file.h>
#include <base/macros.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "screenshot/ptr_util.h"

namespace screenshot {

class Crtc {
 public:
  Crtc(base::File file, ScopedDrmModeCrtcPtr crtc);

  const base::File& file() const { return file_; }
  drmModeCrtc* crtc() const { return crtc_.get(); }

 private:
  base::File file_;
  ScopedDrmModeCrtcPtr crtc_;

  DISALLOW_COPY_AND_ASSIGN(Crtc);
};

class CrtcFinder {
 public:
  static std::unique_ptr<Crtc> FindAnyDisplay();

 private:
  CrtcFinder() = delete;
};

}  // namespace screenshot

#endif  // SCREENSHOT_CRTC_H_
