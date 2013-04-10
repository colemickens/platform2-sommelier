// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/ambient_light_sensor.h"

#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <algorithm>
#include <cmath>

#include "base/bind.h"
#include "base/logging.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

// Default path examined for backlight device directories.
const base::FilePath::CharType kDefaultDeviceListPath[] =
    FILE_PATH_LITERAL("/sys/bus/iio/devices");

// Default interval for polling the ambient light sensor.
const int kDefaultPollIntervalMs = 1000;

}  // namespace

AmbientLightSensor::AmbientLightSensor()
    : device_list_path_(kDefaultDeviceListPath),
      poll_timeout_id_(0),
      poll_interval_ms_(kDefaultPollIntervalMs),
      lux_value_(-1),
      still_deferring_(false),
      als_found_(false),
      read_cb_(base::Bind(&AmbientLightSensor::ReadCallback,
                          base::Unretained(this))),
      error_cb_(base::Bind(&AmbientLightSensor::ErrorCallback,
                           base::Unretained(this))) {
}

AmbientLightSensor::~AmbientLightSensor() {
  util::RemoveTimeout(&poll_timeout_id_);
}

void AmbientLightSensor::Init() {
  poll_timeout_id_ = g_timeout_add(poll_interval_ms_, ReadAlsThunk, this);
}

void AmbientLightSensor::AddObserver(AmbientLightObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void AmbientLightSensor::RemoveObserver(AmbientLightObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

int AmbientLightSensor::GetAmbientLightLux() {
  return lux_value_;
}

gboolean AmbientLightSensor::ReadAls() {
  // We really want to read the ambient light level.
  // Complete the deferred lux file open if necessary.
  if (!als_file_.HasOpenedFile() && !DeferredInit())
    return TRUE;  // Keep the timeout alive.

  // StartRead() can call |error_cb_| synchronously, so clear |poll_timeout_id_|
  // first to make sure that we don't leak a newer timeout set by the callback.
  poll_timeout_id_ = 0;
  als_file_.StartRead(&read_cb_, &error_cb_);
  return FALSE;
}

void AmbientLightSensor::ReadCallback(const std::string& data) {
  int previous_lux_value = lux_value_;

  std::string trimmed_data;
  TrimWhitespaceASCII(data, TRIM_ALL, &trimmed_data);
  lux_value_ = -1;
  if (base::StringToInt(trimmed_data, &lux_value_)) {
    VLOG(1) << "Read lux " << lux_value_;
    if (lux_value_ != previous_lux_value) {
      FOR_EACH_OBSERVER(AmbientLightObserver, observers_,
                        OnAmbientLightChanged(this));
    }
  } else {
    LOG(ERROR) << "Could not read lux value from ALS file contents: ["
               << trimmed_data << "]";
  }
  // Schedule next poll.
  poll_timeout_id_ = g_timeout_add(poll_interval_ms_, ReadAlsThunk, this);
}

void AmbientLightSensor::ErrorCallback() {
  LOG(ERROR) << "Error reading ALS file.";
  poll_timeout_id_ = g_timeout_add(poll_interval_ms_, ReadAlsThunk, this);
}

bool AmbientLightSensor::DeferredInit() {
  CHECK(!als_file_.HasOpenedFile());
  // Search the iio/devices directory for a subdirectory (eg "device0" or
  // "iio:device0") that contains the "[in_]illuminance0_{input|raw}" file.
  file_util::FileEnumerator dir_enumerator(
      device_list_path_, false, file_util::FileEnumerator::DIRECTORIES);
  const char* input_names[] = {
      "in_illuminance0_input",
      "in_illuminance0_raw",
      "illuminance0_input",
  };

  for (base::FilePath check_path = dir_enumerator.Next(); !check_path.empty();
       check_path = dir_enumerator.Next()) {
    for (unsigned int i = 0; i < arraysize(input_names); i++) {
      base::FilePath als_path = check_path.Append(input_names[i]);
      if (als_file_.Init(als_path.value())) {
        if (still_deferring_)
          LOG(INFO) << "Finally found the lux file";
        return true;
      }
    }
  }

  // If the illuminance file is not immediately found, issue a deferral
  // message and try again later.
  if (still_deferring_)
    return false;
  LOG(WARNING) << "Deferring lux: " << strerror(errno);
  still_deferring_ = true;
  return false;
}

}  // namespace system
}  // namespace power_manager
