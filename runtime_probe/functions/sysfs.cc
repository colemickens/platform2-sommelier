/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string>
#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/values.h>

#include "runtime_probe/functions/sysfs.h"

namespace {

static constexpr int kReadFileMaxSize = 1024;

}  // namespace

namespace runtime_probe {

base::DictionaryValue SysfsFunction::ReadSysfs(
    base::FilePath sysfs_path) const {
  base::DictionaryValue dict_value;

  for (const auto key : keys_) {
    const auto file_path = sysfs_path.Append(key);
    std::string content;

    /* missing key */
    if (!base::PathExists(file_path))
      return {};

    /* key exists, but somehow we can't read it */
    if (!base::ReadFileToStringWithMaxSize(file_path, &content,
                                           kReadFileMaxSize)) {
      LOG(ERROR) << file_path.value() << " exists, but we can't read it";
      return {};
    }

    dict_value.SetString(key, content);
  }

  for (const auto key : optional_keys_) {
    const auto file_path = sysfs_path.Append(key);
    std::string content;

    if (!base::PathExists(file_path))
      continue;

    if (base::ReadFileToStringWithMaxSize(file_path, &content,
                                          kReadFileMaxSize))
      dict_value.SetString(key, content);
  }

  return dict_value;
}

SysfsFunction::DataType SysfsFunction::Eval() const {
  DataType result{};

  const base::FilePath glob_path{dir_path_};
  const auto glob_root = glob_path.DirName();
  const auto glob_pattern = glob_path.BaseName();

  if (!base::FilePath{"/sys/"}.IsParent(glob_root)) {
    if (sysfs_path_for_testing_.empty()) {
      LOG(ERROR) << glob_root.value() << " is not under /sys/";
      return {};
    }
    /* While testing, |sysfs_path_for_testing_| can be set to allow additional
     * path.
     */
    if (sysfs_path_for_testing_.IsParent(glob_root) ||
        sysfs_path_for_testing_ == glob_root) {
      LOG(WARNING) << glob_root.value() << " is allowed because "
                   << "sysfs_path_for_testing_ is set to "
                   << sysfs_path_for_testing_.value();
    } else {
      LOG(ERROR) << glob_root.value() << " is neither under under /sys/ nor "
                 << sysfs_path_for_testing_.value();
      return {};
    }
  }

  base::FileEnumerator sysfs_it(glob_root, false,
                                base::FileEnumerator::FileType::DIRECTORIES,
                                glob_pattern.value());
  while (true) {
    auto sysfs_path = sysfs_it.Next();
    if (sysfs_path.empty())
      break;

    auto dict_value = ReadSysfs(sysfs_path);
    if (!dict_value.empty())
      result.push_back(std::move(dict_value));
  }
  return result;
}

}  // namespace runtime_probe
