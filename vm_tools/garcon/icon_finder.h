// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_GARCON_ICON_FINDER_H_
#define VM_TOOLS_GARCON_ICON_FINDER_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>

namespace vm_tools {
namespace garcon {

base::FilePath LocateIconFile(const std::string& desktop_file_id,
                              int icon_size,
                              int scale);

std::vector<base::FilePath> GetPathsForIcons(int icon_size, int scale);

}  // namespace garcon
}  // namespace vm_tools

#endif  // VM_TOOLS_GARCON_ICON_FINDER_H_
