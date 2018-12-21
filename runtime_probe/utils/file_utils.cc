/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <base/files/file_util.h>
#include <base/strings/string_util.h>

#include "runtime_probe/utils/file_utils.h"

using base::DictionaryValue;
using base::FilePath;
using base::PathExists;
using base::ReadFileToStringWithMaxSize;
using base::TrimWhitespaceASCII;
using base::TrimPositions::TRIM_ALL;
using std::string;
using std::vector;

namespace {

constexpr int kReadFileMaxSize = 1024;

}  // namespace

namespace runtime_probe {

/* Maps files listed in |keys| and |optional_keys| under |dir_path| into key
 * value pairs.
 *
 * |keys| represents the set of must have, if any |keys| is missed in the
 * |dir_path|, an empty dictionary will be returned.
 */
DictionaryValue MapFilesToDict(const FilePath& dir_path,
                               const vector<string> keys,
                               const vector<string> optional_keys) {
  DictionaryValue ret;

  for (const auto key : keys) {
    const auto file_path = dir_path.Append(key);
    string content;

    /* missing key */
    if (!PathExists(file_path))
      return {};

    /* key exists, but somehow we can't read it */
    if (!ReadFileToStringWithMaxSize(file_path, &content, kReadFileMaxSize)) {
      LOG(ERROR) << file_path.value() << " exists, but we can't read it";
      return {};
    }

    ret.SetString(key, TrimWhitespaceASCII(content, TRIM_ALL));
  }

  for (const auto key : optional_keys) {
    const auto file_path = dir_path.Append(key);
    string content;

    if (!base::PathExists(file_path))
      continue;

    if (ReadFileToStringWithMaxSize(file_path, &content, kReadFileMaxSize))
      ret.SetString(key, TrimWhitespaceASCII(content, TRIM_ALL));
  }

  return ret;
}

}  // namespace runtime_probe
