// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/ambient_light_sensor.h"

#include <fcntl.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>

#include <base/bind.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

// Default path examined for backlight device directories.
const char kDefaultDeviceListPath[] = "/sys/bus/iio/devices";

// Default interval for polling the ambient light sensor.
const int kDefaultPollIntervalMs = 1000;

}  // namespace

const int AmbientLightSensor::kNumInitAttemptsBeforeLogging = 5;
const int AmbientLightSensor::kNumInitAttemptsBeforeGivingUp = 20;

AmbientLightSensor::AmbientLightSensor()
    : device_list_path_(kDefaultDeviceListPath),
      poll_interval_ms_(kDefaultPollIntervalMs),
      lux_value_(-1),
      num_init_attempts_(0) {}

AmbientLightSensor::~AmbientLightSensor() {}

void AmbientLightSensor::Init(bool read_immediately) {
  if (read_immediately)
    ReadAls();
  StartTimer();
}

base::FilePath AmbientLightSensor::GetIlluminancePath() const {
  return als_file_.HasOpenedFile() ? als_file_.path() : base::FilePath();
}

bool AmbientLightSensor::TriggerPollTimerForTesting() {
  if (!poll_timer_.IsRunning())
    return false;

  ReadAls();
  return true;
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

bool AmbientLightSensor::IsColorSensor() const {
  return false;
}

void AmbientLightSensor::StartTimer() {
  poll_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(poll_interval_ms_), this,
                    &AmbientLightSensor::ReadAls);
}

void AmbientLightSensor::ReadAls() {
  // We really want to read the ambient light level.
  // Complete the deferred lux file open if necessary.
  if (!als_file_.HasOpenedFile() && !InitAlsFile()) {
    if (num_init_attempts_ >= kNumInitAttemptsBeforeGivingUp) {
      LOG(ERROR) << "Giving up on reading from sensor";
      poll_timer_.Stop();
    }
    return;
  }

  // The timer will be restarted after the read finishes.
  poll_timer_.Stop();
  als_file_.StartRead(
      base::Bind(&AmbientLightSensor::ReadCallback, base::Unretained(this)),
      base::Bind(&AmbientLightSensor::ErrorCallback, base::Unretained(this)));
}

void AmbientLightSensor::ReadCallback(const std::string& data) {
  std::string trimmed_data;
  base::TrimWhitespaceASCII(data, base::TRIM_ALL, &trimmed_data);
  int value = 0;
  if (base::StringToInt(trimmed_data, &value)) {
    lux_value_ = value;
    VLOG(1) << "Read lux " << lux_value_;
    for (AmbientLightObserver& observer : observers_)
      observer.OnAmbientLightUpdated(this);
  } else {
    LOG(ERROR) << "Could not read lux value from ALS file contents: ["
               << trimmed_data << "]";
  }
  StartTimer();
}

void AmbientLightSensor::ErrorCallback() {
  LOG(ERROR) << "Error reading ALS file";
  StartTimer();
}

bool AmbientLightSensor::InitAlsFile() {
  CHECK(!als_file_.HasOpenedFile());

  // Search the iio/devices directory for a subdirectory (eg "device0" or
  // "iio:device0") that contains the "[in_]illuminance[0]_{input|raw}" file.
  base::FileEnumerator dir_enumerator(device_list_path_, false,
                                      base::FileEnumerator::DIRECTORIES);
  const char* input_names[] = {
      "in_illuminance0_input", "in_illuminance_input", "in_illuminance0_raw",
      "in_illuminance_raw",    "illuminance0_input",
  };

  num_init_attempts_++;
  for (base::FilePath check_path = dir_enumerator.Next(); !check_path.empty();
       check_path = dir_enumerator.Next()) {
    for (unsigned int i = 0; i < arraysize(input_names); i++) {
      base::FilePath als_path = check_path.Append(input_names[i]);
      if (!base::PathExists(als_path))
        continue;
      if (als_file_.Init(als_path)) {
        LOG(INFO) << "Using lux file " << als_path.value();
        return true;
      }
    }
  }

  // If the illuminance file is not immediately found, issue a deferral
  // message and try again later.
  if (num_init_attempts_ > kNumInitAttemptsBeforeLogging)
    PLOG(ERROR) << "lux file initialization failed";
  return false;
}

}  // namespace system
}  // namespace power_manager
