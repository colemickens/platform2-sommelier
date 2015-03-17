// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "brdebug/device_property_watcher.h"

#include <base/bind.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/strings/string_util.h>

using std::map;
using std::string;

namespace brdebug {

namespace {

const unsigned int kDevicePropertyMaxLen = 128;

const char kDevicePropertyDir[] = "/var/lib/brillo-device";

const char kDevicePropertyAlias[] = "alias";

void ResetDeviceProperties(map<string, string>* device_properties) {
  *device_properties = {
      {kDevicePropertyAlias, ""},
  };
}

}  // namespace

DevicePropertyWatcher::DevicePropertyWatcher(const base::Closure& callback)
    : device_property_dir_path_(kDevicePropertyDir),
      device_property_change_callback_(callback) {}

bool DevicePropertyWatcher::Init() {
  if (!base::DirectoryExists(device_property_dir_path_) &&
      !base::CreateDirectory(device_property_dir_path_)) {
    LOG(ERROR) << "Unable to locate or create directory "
               << device_property_dir_path_.value();
    return false;
  }

  if (!device_property_dir_watcher_.Watch(
          device_property_dir_path_, false /* not recursive */,
          base::Bind(&DevicePropertyWatcher::HandleFileChange,
                     weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "Unable to watch " << device_property_dir_path_.value();
    return false;
  }

  ReadDeviceProperties();

  return true;
}

map<string, string> DevicePropertyWatcher::GetDeviceProperties() const {
  return device_properties_;
}

void DevicePropertyWatcher::HandleFileChange(const base::FilePath& path,
                                             bool error) {
  if (error) {
    LOG(ERROR) << "Error hearing about change to " << path.value();
    return;
  }

  ReadDeviceProperties();
  device_property_change_callback_.Run();
}

void DevicePropertyWatcher::ReadDeviceProperties() {
  ResetDeviceProperties(&device_properties_);

  base::FileEnumerator prop_file_enum(device_property_dir_path_,
                                      false /* not recursive */,
                                      base::FileEnumerator::FILES);
  while (true) {
    const base::FilePath prop_file_path = prop_file_enum.Next();
    if (prop_file_path.empty()) break;

    string prop_name = prop_file_path.BaseName().value();
    if (device_properties_.find(prop_name) == device_properties_.end()) {
      LOG(WARNING) << "Unknown property '" << prop_name << "'.";
      continue;
    }

    string prop_value;
    if (!base::ReadFileToString(prop_file_path, &prop_value,
                                kDevicePropertyMaxLen)) {
      LOG(WARNING) << "Error reading " << prop_file_path.value()
                   << " or the file has more than " << kDevicePropertyMaxLen
                   << " characters and will be ignored.";
      continue;
    }

    base::TrimWhitespaceASCII(prop_value, base::TRIM_ALL, &prop_value);
    device_properties_[prop_name] = prop_value;
  }
}

}  // namespace brdebug
