// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/ambient_light_sensor.h"

#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/file_util.h"

namespace power_manager {

// Period in which to poll the ambient light sensor.
static const int kSensorPollPeriodMs = 1000;

// Lux level <= kLuxLo should return 0% response.
static const int kLuxLo = 12;

// Lux level >= kLuxHi should return 100% response.
static const int kLuxHi = 1000;

// A positive kLuxOffset gives us a flatter curve, particularly at lower lux.
// Alternatively, we could use a higher kLuxLo.
static const int kLuxOffset = 4;

AmbientLightSensor::AmbientLightSensor(BacklightController* controller,
                                       PowerPrefsInterface* prefs)
    : controller_(controller),
      prefs_(prefs),
      als_fd_(-1),
      is_polling_(false),
      disable_polling_(false),
      still_deferring_(false) {
  // Initialize factors used for LuxToPercent calculation.
  // See comments in Tsl2563LuxToPercent() for a full description.
  double hi = kLuxHi + kLuxOffset;
  double lo = kLuxLo + kLuxOffset;
  log_multiply_factor_ = 100 / log(hi / lo);
  log_subtract_factor_ = log(lo) * log_multiply_factor_;
}

AmbientLightSensor::~AmbientLightSensor() {
  if (als_fd_ >= 0)
    close(als_fd_);
}

bool AmbientLightSensor::DeferredInit() {
  // Search the iio/devices directory for a subdirectory (eg "device0" or
  // "iio:device0") that contains the "[in_]illuminance0_{input|raw}" file.
  file_util::FileEnumerator dir_enumerator(
      FilePath("/sys/bus/iio/devices"), false,
      file_util::FileEnumerator::DIRECTORIES);
  const char* input_names[] = {
      "in_illuminance0_input",
      "in_illuminance0_raw",
      "illuminance0_input",
  };

  for (FilePath check_path = dir_enumerator.Next(); !check_path.empty();
       check_path = dir_enumerator.Next()) {
    FilePath als_path;
    for (unsigned int i = 0; i < arraysize(input_names); i++) {
      als_path = check_path.Append(input_names[i]);
      if (file_util::PathExists(als_path))
        break;
    }
    als_fd_ = open(als_path.value().c_str(), O_RDONLY);
    if (als_fd_ >= 0)
      break;
  }

  // If the illuminance file is not immediately found, issue a deferral
  // message and try again later.
  if (als_fd_ == -1) {
    if (still_deferring_)
      return false;
    LOG(WARNING) << "Deferring lux: " << strerror(errno);
    still_deferring_ = true;
    return false;
  }
  if (still_deferring_)
    LOG(INFO) << "Finally found the lux file";
  return true;
}

bool AmbientLightSensor::Init() {
  int64 disable_als;
  // TODO: In addition to disable_als, we should add another prefs file
  // that allows polling ALS as usual but prevents backlight changes from
  // happening. This will be useful for power and system profiling.
  if (prefs_->GetInt64(kDisableALS, &disable_als) &&
      disable_als) {
    LOG(INFO) << "Not using ambient light sensor";
    return false;
  }
  if (controller_)
    controller_->SetAmbientLightSensor(this);
  return true;
}

void AmbientLightSensor::EnableOrDisableSensor(PowerState state) {
  if (state != BACKLIGHT_ACTIVE) {
    LOG(INFO) << "Disabling light sensor poll";
    disable_polling_ = true;
    return;
  }

  // We want to poll.
  // There is a possible race between setting disable_polling_ = true above
  // and now setting it false.  If BacklightController rapidly transitions
  // the backlight into and out of dim, we might try to turn on polling when
  // it is already on.
  // is_polling_ resolves the race.  No locking is needed in this single
  // threaded application.
  disable_polling_ = false;
  if (is_polling_)
    return;  // already polling.

  // Start polling.
  LOG(INFO) << "Enabling light sensor poll";
  is_polling_ = true;
  g_timeout_add(kSensorPollPeriodMs, ReadAlsThunk, this);
}

gboolean AmbientLightSensor::ReadAls() {
  if (disable_polling_) {
    is_polling_ = false;
    return false;  // Returning false removes the timeout.
  }

  // We really want to read the ambient light level.
  // Complete the deferred lux file open if necessary.
  if (als_fd_ < 0) {
    if (!DeferredInit())
      return true;  // Return true to try again later.
  }

  char buffer[10];
  int n;
  if (lseek(als_fd_, 0, SEEK_SET) != 0 ||
      (n = read(als_fd_, buffer, sizeof(buffer) - 1)) == -1) {
    LOG(WARNING) << "Unable to read light sensor file";
  }
  if (n > 0 && n < static_cast<int>(sizeof(buffer))) {
    buffer[n] = '\0';
    int luxval = atoi(buffer);
    if (controller_)
      controller_->SetAlsBrightnessOffsetPercent(Tsl2563LuxToPercent(luxval));
  }
  return true;
}

double AmbientLightSensor::Tsl2563LuxToPercent(int luxval) {
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
