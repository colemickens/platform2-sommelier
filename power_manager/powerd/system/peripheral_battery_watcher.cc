// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/peripheral_battery_watcher.h"

#include <fcntl.h>
#include <errno.h>

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

// Default path examined for peripheral battery directories.
const base::FilePath::CharType kDefaultPeripheralBatteryPath[] =
    FILE_PATH_LITERAL("/sys/class/power_supply/");

// Default interval for polling the device battery info.
const int kDefaultPollIntervalMs = 10000;

}  // namespace

PeripheralBatteryWatcher::PeripheralBatteryWatcher()
    : peripheral_battery_path_(kDefaultPeripheralBatteryPath),
      poll_timeout_id_(0),
      poll_interval_ms_(kDefaultPollIntervalMs) {
}

PeripheralBatteryWatcher::~PeripheralBatteryWatcher() {
  util::RemoveTimeout(&poll_timeout_id_);
}

void PeripheralBatteryWatcher::Init() {
  ReadBatteryStatuses();
}

void PeripheralBatteryWatcher::AddObserver(
    PeripheralBatteryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PeripheralBatteryWatcher::RemoveObserver(
    PeripheralBatteryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void PeripheralBatteryWatcher::GetBatteryList(
    std::vector<base::FilePath>* battery_list) {
  battery_list->clear();
  file_util::FileEnumerator dir_enumerator(
      peripheral_battery_path_, false, file_util::FileEnumerator::DIRECTORIES);

  // Peripheral battery has a sysfs entry with name "scope" containing value
  // "Device".
  for (base::FilePath check_path = dir_enumerator.Next(); !check_path.empty();
       check_path = dir_enumerator.Next()) {
    base::FilePath scope_path = check_path.Append("scope");
    if (!file_util::PathExists(scope_path))
      continue;

    std::string buf;
    file_util::ReadFileToString(scope_path, &buf);
    TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
    if (buf != "Device")
      continue;

    battery_list->push_back(check_path);
  }
}

gboolean PeripheralBatteryWatcher::ReadBatteryStatuses() {
  battery_readers_.clear();
  read_callbacks_.clear();
  error_callbacks_.clear();

  std::vector<base::FilePath> new_battery_list;
  GetBatteryList(&new_battery_list);

  for (size_t i = 0; i < new_battery_list.size(); i++) {
    base::FilePath path = new_battery_list[i];

    // sysfs entry "capacity" has the current battery level.
    base::FilePath capacity_path = path.Append("capacity");
    if (!file_util::PathExists(capacity_path))
      continue;

    base::FilePath model_name_path = path.Append("model_name");
    if (!file_util::PathExists(model_name_path))
      continue;

    std::string model_name;
    file_util::ReadFileToString(model_name_path, &model_name);
    TrimWhitespaceASCII(model_name, TRIM_TRAILING, &model_name);

    AsyncFileReader* reader = new AsyncFileReader;
    ReadCallbackType* read_cb = new ReadCallbackType(
        base::Bind(&PeripheralBatteryWatcher::ReadCallback,
                   base::Unretained(this),
                   path.value(), model_name));
    ErrorCallbackType* error_cb = new ErrorCallbackType(
        base::Bind(&PeripheralBatteryWatcher::ErrorCallback,
                   base::Unretained(this),
                   path.value(), model_name));

    battery_readers_.push_back(reader);
    read_callbacks_.push_back(read_cb);
    error_callbacks_.push_back(error_cb);

    if (reader->Init(capacity_path.value())) {
      reader->StartRead(read_cb, error_cb);
    } else {
      LOG(ERROR) << "Can't read battery capacity " << capacity_path.value();
    }
  }

  poll_timeout_id_ = g_timeout_add(poll_interval_ms_, ReadBatteryStatusesThunk,
                                   this);
  return FALSE;
}

void PeripheralBatteryWatcher::ReadCallback(const std::string& path,
                                            const std::string& model_name,
                                            const std::string& data) {
  std::string trimmed_data;
  TrimWhitespaceASCII(data, TRIM_ALL, &trimmed_data);
  int level = -1;
  if (base::StringToInt(trimmed_data, &level)) {
    FOR_EACH_OBSERVER(PeripheralBatteryObserver, observer_list_,
                      OnPeripheralBatteryUpdate(path, model_name, level));
  } else {
    LOG(ERROR) << "Invalid battery level reading : [" << data << "]"
               << " from " << path;
  }
}

void PeripheralBatteryWatcher::ErrorCallback(const std::string& path,
                                             const std::string& model_name) {
  FOR_EACH_OBSERVER(PeripheralBatteryObserver, observer_list_,
                    OnPeripheralBatteryError(path, model_name));
}

}  // namespace system
}  // namespace power_manager
