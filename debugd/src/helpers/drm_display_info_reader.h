// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_HELPERS_DRM_DISPLAY_INFO_READER_H_
#define DEBUGD_SRC_HELPERS_DRM_DISPLAY_INFO_READER_H_

#include <memory>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/values.h>

namespace debugd {

// Returns info about the entire set of DRM devices, their connectors, and the
// displays connected to those connectors.
class DRMDisplayInfoReader {
 public:
  DRMDisplayInfoReader() = default;
  ~DRMDisplayInfoReader() = default;

  std::unique_ptr<base::DictionaryValue> GetDisplayInfo(
      const base::FilePath&) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(DRMDisplayInfoReader);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_HELPERS_DRM_DISPLAY_INFO_READER_H_
