// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/campfire_tool.h"

#include <string>

#include <base/files/file_util.h>
#include <base/strings/stringprintf.h>

namespace debugd {

namespace {

constexpr char kCampfireTagPath[] = "/var/lib/misc/campfire";

}

std::string CampfireTool::EnableAltOS(int size_gb) {
  return WriteTagFile(base::StringPrintf("-s %d prepare", size_gb));
}

std::string CampfireTool::DisableAltOS() {
  return WriteTagFile("restore");
}

std::string CampfireTool::WriteTagFile(const std::string& content) {
  base::FilePath campfire_tag_path(kCampfireTagPath);
  if (base::WriteFile(campfire_tag_path, content.c_str(),
                      content.length()) != content.length()) {
    return base::StringPrintf("Failed to write %s", kCampfireTagPath);
  }
  return "Success";
}

}  // namespace debugd
