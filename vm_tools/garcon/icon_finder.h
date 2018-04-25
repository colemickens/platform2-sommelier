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

// Returns a valid file path for reading in an icon file with the specified
// parameters. If |scale| is greater than 1 and it cannot find an icon at the
// requested scale, it will search with a scale factor of 1 as well.
base::FilePath LocateIconFile(const std::string& desktop_file_id,
                              int icon_size,
                              int scale);

// Returns a vector of directory paths that can be searched under for a
// specific icon meeting the passed in criteria.
std::vector<base::FilePath> GetPathsForIcons(int icon_size, int scale);

}  // namespace garcon
}  // namespace vm_tools

#endif  // VM_TOOLS_GARCON_ICON_FINDER_H_
