// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "screenshot/crtc.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>

namespace screenshot {

namespace {

constexpr const char kDrmDeviceDir[] = "/dev/dri";
constexpr const char kDrmDeviceGlob[] = "card?";

}  // namespace

Crtc::Crtc(base::File file, ScopedDrmModeCrtcPtr crtc)
    : file_(std::move(file)), crtc_(std::move(crtc)) {}

// static
std::unique_ptr<Crtc> CrtcFinder::FindAnyDisplay() {
  std::vector<base::FilePath> paths;
  {
    base::FileEnumerator lister(base::FilePath(kDrmDeviceDir),
                                false,
                                base::FileEnumerator::FILES,
                                kDrmDeviceGlob);
    for (base::FilePath name = lister.Next();
         !name.empty();
         name = lister.Next()) {
      paths.push_back(name);
    }
  }
  std::sort(paths.begin(), paths.end());

  for (base::FilePath path : paths) {
    base::File file(
        path,
        base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
    if (!file.IsValid())
      continue;

    ScopedDrmModeResPtr resources(drmModeGetResources(file.GetPlatformFile()));
    if (!resources)
      continue;

    for (int index_crtc = 0;
         index_crtc < resources->count_crtcs;
         ++index_crtc) {
      uint32_t crtc_id = resources->crtcs[index_crtc];

      ScopedDrmModeCrtcPtr crtc(
          drmModeGetCrtc(file.GetPlatformFile(), crtc_id));
      if (!crtc || !crtc->mode_valid || crtc->buffer_id == 0)
        continue;

      return std::make_unique<Crtc>(std::move(file), std::move(crtc));
    }
  }

  return nullptr;
}

}  // namespace screenshot
