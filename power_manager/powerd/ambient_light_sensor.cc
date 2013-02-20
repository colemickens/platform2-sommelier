// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/ambient_light_sensor.h"

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

namespace {

// Default path examined for backlight device directories.
const FilePath::CharType kDefaultDeviceListPath[] =
    FILE_PATH_LITERAL("/sys/bus/iio/devices");

// Default interval for polling the ambient light sensor.
const int kDefaultPollIntervalMs = 1000;

// Lux level <= kLuxLo should return 0% response.
const int kLuxLo = 12;

// Lux level >= kLuxHi should return 100% response.
const int kLuxHi = 1000;

// A positive kLuxOffset gives us a flatter curve, particularly at lower lux.
// Alternatively, we could use a higher kLuxLo.
const int kLuxOffset = 4;

// Max number of entries to have in the value histories.
size_t kHistorySizeMax = 10;

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
  // Initialize factors used for LuxToPercent calculation.
  // See comments in Tsl2563LuxToPercent() for a full description.
  double hi = kLuxHi + kLuxOffset;
  double lo = kLuxLo + kLuxOffset;
  log_multiply_factor_ = 100 / log(hi / lo);
  log_subtract_factor_ = log(lo) * log_multiply_factor_;

}

AmbientLightSensor::~AmbientLightSensor() {
  util::RemoveTimeout(&poll_timeout_id_);
}

void AmbientLightSensor::Init() {
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

  for (FilePath check_path = dir_enumerator.Next(); !check_path.empty();
       check_path = dir_enumerator.Next()) {
    for (unsigned int i = 0; i < arraysize(input_names); i++) {
      FilePath als_path = check_path.Append(input_names[i]);
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

void AmbientLightSensor::AddObserver(AmbientLightSensorObserver* observer) {
  observer_list_.AddObserver(observer);
}

void AmbientLightSensor::RemoveObserver(AmbientLightSensorObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

double AmbientLightSensor::GetAmbientLightPercent() const {
  return (lux_value_ != -1) ? Tsl2563LuxToPercent(lux_value_) : -1.0;
}

int AmbientLightSensor::GetAmbientLightLux() const {
  return lux_value_;
}

std::string AmbientLightSensor::DumpPercentHistory() const {
  std::string buffer;
  for (std::list<double>::const_reverse_iterator it = percent_history_.rbegin();
       it != percent_history_.rend();
       ++it) {
    if (!buffer.empty())
      buffer += ", ";
    buffer += StringPrintf("%.1f", *it);
  }
  return "[" + buffer + "]";
}

std::string AmbientLightSensor::DumpLuxHistory() const {
  std::string buffer;
  for (std::list<int>::const_reverse_iterator it = lux_history_.rbegin();
       it != lux_history_.rend();
       ++it) {
    if (!buffer.empty())
      buffer += ", ";
    buffer += base::IntToString(*it);
  }
  return "[" + buffer + "]";
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
    // Update the history logs.
    if (percent_history_.size() >= kHistorySizeMax)
      percent_history_.pop_front();
    percent_history_.push_back(Tsl2563LuxToPercent(lux_value_));
    if (lux_history_.size() >= kHistorySizeMax)
      lux_history_.pop_front();
    lux_history_.push_back(lux_value_);
    if (lux_value_ != previous_lux_value) {
      FOR_EACH_OBSERVER(AmbientLightSensorObserver, observer_list_,
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

double AmbientLightSensor::Tsl2563LuxToPercent(int luxval) const {
  // Notes on tsl2563 Ambient Light Response (_ALR) table:
  //
  // measurement location: lux file value, intended luma level
  //            dark room: 0,              0
  //               office: 75,             50
  //  outside, day, shade: 1000-3000,      100
  //  outside, day, direct sunlight: 10000, 100
  //
  // Give a natural logorithmic response of 0-100% for lux values 12-1000.
  // What's a log?  If value=e^exponent, then log(value)=exponent.
  //
  // Multiply the log by log_multiply_factor_ to provide the 100% range.
  // Calculated as: 100 / (log((kLuxHi + kLuxOffset) / (kLuxLo + kLuxOffset)))
  //    hi = kLuxHi + kLuxOffset
  //    lo = kLuxLo + kLuxOffset
  //    (log(hi) - log(lo)) * log_multiply_factor_ = 100
  //    So: log_multiply_factor_ = 100 / log(hi / lo)
  //
  // Subtract log_subtract_factor_ from the log product to normalize to 0.
  // Calculated as: log_subtract_factor_ = log(lo) * log_multiply_factor_
  //    lo = kLuxLo + kLuxOffset
  //    log(lo) * log_multiply_factor_ - log_subtract_factor_ = 0
  //    So: log_subtract_factor_ = log(lo) * log_multiply_factor_

  int value = luxval + kLuxOffset;
  double response = log(value) * log_multiply_factor_ - log_subtract_factor_;
  return std::max(0.0, std::min(100.0, response));
}

}  // namespace power_manager
