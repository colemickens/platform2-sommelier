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
  Crtc(base::File file, ScopedDrmModeConnectorPtr connector,
       ScopedDrmModeEncoderPtr encoder, ScopedDrmModeCrtcPtr crtc,
       ScopedDrmModeFBPtr fb);

  const base::File& file() const { return file_; }
  drmModeConnector* connector() const { return connector_.get(); }
  drmModeEncoder* encoder() const { return encoder_.get(); }
  drmModeCrtc* crtc() const { return crtc_.get(); }
  drmModeFB* fb() const { return fb_.get(); }

  bool IsInternalDisplay() const;

 private:
  base::File file_;
  ScopedDrmModeConnectorPtr connector_;
  ScopedDrmModeEncoderPtr encoder_;
  ScopedDrmModeCrtcPtr crtc_;
  ScopedDrmModeFBPtr fb_;

  DISALLOW_COPY_AND_ASSIGN(Crtc);
};

class CrtcFinder {
 public:
  static std::unique_ptr<Crtc> FindAnyDisplay();
  static std::unique_ptr<Crtc> FindInternalDisplay();
  static std::unique_ptr<Crtc> FindExternalDisplay();
  static std::unique_ptr<Crtc> FindById(uint32_t crtc_id);

 private:
  CrtcFinder() = delete;
};

}  // namespace screenshot

#endif  // SCREENSHOT_CRTC_H_
