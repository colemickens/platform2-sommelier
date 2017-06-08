// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_TESTS_TEST_HELPER_H_
#define MIDIS_TESTS_TEST_HELPER_H_

#include <string>

#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <gmock/gmock.h>

#include "midis/device_tracker.h"

namespace midis {

MATCHER_P2(DeviceMatcher, id, name, "") {
  return (id == UdevHandler::GenerateDeviceId(arg->GetCard(),
                                              arg->GetDeviceNum()) &&
          base::EqualsCaseInsensitiveASCII(arg->GetName(), name));
}

base::FilePath CreateFakeTempSubDir(base::FilePath temp_path,
                                    const std::string& subdir_path);

base::FilePath CreateDevNodeFileName(base::FilePath dev_path_base,
                                     uint32_t sys_num, uint32_t dev_num);

}  // namespace midis

#endif  // MIDIS_TESTS_TEST_HELPER_H_
