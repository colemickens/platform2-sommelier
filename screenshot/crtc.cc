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

std::vector<std::unique_ptr<Crtc>> GetConnectedCrtcs() {
  std::vector<std::unique_ptr<Crtc>> crtcs;

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

    for (int index_connector = 0;
         index_connector < resources->count_connectors;
         ++index_connector) {
      ScopedDrmModeConnectorPtr connector(
          drmModeGetConnector(file.GetPlatformFile(),
                              resources->connectors[index_connector]));
      if (!connector || connector->encoder_id == 0)
        continue;

      ScopedDrmModeEncoderPtr encoder(
          drmModeGetEncoder(file.GetPlatformFile(), connector->encoder_id));
      if (!encoder || encoder->crtc_id == 0)
        continue;

      ScopedDrmModeCrtcPtr crtc(
          drmModeGetCrtc(file.GetPlatformFile(), encoder->crtc_id));
      if (!crtc || !crtc->mode_valid || crtc->buffer_id == 0)
        continue;

      ScopedDrmModeFBPtr fb(
          drmModeGetFB(file.GetPlatformFile(), crtc->buffer_id));
      if (!fb)
        continue;

      crtcs.emplace_back(std::make_unique<Crtc>(
          file.Duplicate(), std::move(connector), std::move(encoder),
          std::move(crtc), std::move(fb)));
    }
  }

  return crtcs;
}

}  // namespace

Crtc::Crtc(base::File file, ScopedDrmModeConnectorPtr connector,
           ScopedDrmModeEncoderPtr encoder, ScopedDrmModeCrtcPtr crtc,
           ScopedDrmModeFBPtr fb)
    : file_(std::move(file)), connector_(std::move(connector)),
      encoder_(std::move(encoder)), crtc_(std::move(crtc)),
      fb_(std::move(fb)) {}

bool Crtc::IsInternalDisplay() const {
  switch (connector_->connector_type) {
    case DRM_MODE_CONNECTOR_eDP:
    case DRM_MODE_CONNECTOR_LVDS:
    case DRM_MODE_CONNECTOR_DSI:
    case DRM_MODE_CONNECTOR_VIRTUAL:
      return true;
    default:
      return false;
  }
}

// static
std::unique_ptr<Crtc> CrtcFinder::FindAnyDisplay() {
  auto crtcs = GetConnectedCrtcs();
  if (crtcs.empty())
    return nullptr;
  return std::move(crtcs[0]);
}

// static
std::unique_ptr<Crtc> CrtcFinder::FindInternalDisplay() {
  auto crtcs = GetConnectedCrtcs();
  for (auto& crtc : crtcs)
    if (crtc->IsInternalDisplay())
      return std::move(crtc);
  return nullptr;
}

// static
std::unique_ptr<Crtc> CrtcFinder::FindExternalDisplay() {
  auto crtcs = GetConnectedCrtcs();
  for (auto& crtc : crtcs)
    if (!crtc->IsInternalDisplay())
      return std::move(crtc);
  return nullptr;
}

// static
std::unique_ptr<Crtc> CrtcFinder::FindById(uint32_t crtc_id) {
  auto crtcs = GetConnectedCrtcs();
  for (auto& crtc : crtcs)
    if (crtc->crtc()->crtc_id == crtc_id)
      return std::move(crtc);
  return nullptr;
}

}  // namespace screenshot
