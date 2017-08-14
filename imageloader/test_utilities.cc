// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_utilities.h"

#include <dirent.h>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/logging.h>

#include "component.h"

namespace base {
void PrintTo(const base::FilePath& path, std::ostream* stream) {
  *stream << path.value();
}
}  // namespace base

namespace imageloader {

base::FilePath GetTestDataPath(const std::string& subdir) {
  const char* src_dir = getenv("CROS_WORKON_SRCROOT");
  CHECK(src_dir != nullptr);
  base::FilePath component_path(src_dir);
  component_path = component_path.Append("src")
                       .Append("platform")
                       .Append("imageloader")
                       .Append("testdata")
                       .Append(subdir);
  return component_path;
}

base::FilePath GetTestComponentPath() {
  return GetTestComponentPath(kTestDataVersion);
}

base::FilePath GetTestComponentPath(const std::string& version) {
  return GetTestDataPath(version + "_chromeos_intel64_PepperFlashPlayer");
}

base::FilePath GetTestOciComponentPath() {
  return GetTestDataPath(kTestOciComponentName);
}

void GetFilesInDir(const base::FilePath& dir, std::list<std::string>* files) {
  base::FileEnumerator file_enum(dir, false, base::FileEnumerator::FILES);
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    files->emplace_back(name.BaseName().value());
  }
}

}  // namespace imageloader
