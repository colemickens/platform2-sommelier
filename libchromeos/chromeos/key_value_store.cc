// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/key_value_store.h"

#include <map>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/important_file_writer.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

using std::map;
using std::string;
using std::vector;

namespace chromeos {

bool KeyValueStore::Load(const base::FilePath& path) {
  string file_data;
  if (!base::ReadFileToString(path, &file_data))
    return false;

  // Split along '\n', then along '='
  vector<string> lines;
  base::SplitStringDontTrim(file_data, '\n', &lines);
  for (const auto& line : lines) {
    if (line.empty() || line.front() == '#')
      continue;
    string::size_type pos = line.find('=');
    if (pos == string::npos)
      continue;
    store_[line.substr(0, pos)] = line.substr(pos + 1);
  }
  return true;
}

bool KeyValueStore::Save(const base::FilePath& path) const {
  string data;
  for (const auto& key_value : store_)
    data += key_value.first + "=" + key_value.second + "\n";

  return base::ImportantFileWriter::WriteFileAtomically(path, data);
}

bool KeyValueStore::GetString(const string& key, string* value) const {
  const auto key_value = store_.find(key);
  if (key_value == store_.end())
    return false;
  *value = key_value->second;
  return true;
}

void KeyValueStore::SetString(const string& key, const string& value) {
  store_[key] = value;
}

bool KeyValueStore::GetBoolean(const string& key, bool* value) const {
  const auto key_value = store_.find(key);
  if (key_value == store_.end())
    return false;
  if (key_value->second == "true") {
    *value = true;
    return true;
  } else if (key_value-> second == "false") {
    *value = false;
    return true;
  }
  return false;
}

void KeyValueStore::SetBoolean(const string& key, bool value) {
  store_[key] = value ? "true" : "false";
}

}  // namespace chromeos
