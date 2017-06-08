// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <base/files/file_path.h>
#include <base/json/json_writer.h>

#include "debugd/src/helpers/drm_display_info_reader.h"

// A command line tool that looks up DRM-based display infomation from sysfs.
// Returns infomation about:
// - DRM devices
// - Connectors on each DRM device
// - The displays connected to each connector, if any.
//
// Usage: drm_display_info [sysfs_root]
//   [sysfs_root] is an optional argument that specifies the path of the DRM
//   sysfs directory if it is something other than the default.

namespace {

// By default, scan for DRM display status info in this directory.
const char kDefaultDRMSysfsPath[] = "/sys/class/drm";

}  // namespace

int main(int argc, char* argv[]) {
  base::FilePath drm_path(argc >= 2 ? argv[1] : kDefaultDRMSysfsPath);

  debugd::DRMDisplayInfoReader display_reader;
  std::unique_ptr<base::DictionaryValue> result =
      display_reader.GetDisplayInfo(drm_path);

  std::string json;
  base::JSONWriter::WriteWithOptions(
      *result, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  printf("%s\n", json.c_str());
  return 0;
}
