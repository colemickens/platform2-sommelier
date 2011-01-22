// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/ambient_light_sensor.h"

#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "base/logging.h"

namespace power_manager {

// Period in which to poll the ambient light sensor.
static const int kSensorPollPeriodMs = 1000;

AmbientLightSensor::AmbientLightSensor(BacklightController* controller)
    : controller_(controller),
      als_fd_(-1),
      last_level_(0),
      is_polling_(false),
      disable_polling_(false),
      still_deferring_(false) {}

AmbientLightSensor::~AmbientLightSensor() {
  if (als_fd_ >= 0)
    close(als_fd_);
}

bool AmbientLightSensor::DeferredInit() {
  // tsl2561 is currently the only supported light sensor.
  // If the lux file is not immediately found, issue a deferral
  // message and try again later.
  als_fd_ = open("/sys/class/iio/device0/lux", O_RDONLY);
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
  if (controller_)
    controller_->set_light_sensor(this);
  return true;
}

void AmbientLightSensor::EnableOrDisableSensor(PowerState power, DimState dim) {
  if (power == BACKLIGHT_OFF || dim == BACKLIGHT_DIM) {
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
    return; // already polling.

  // Start polling.
  LOG(INFO) << "Enabling light sensor poll";
  is_polling_ = true;
  g_timeout_add(kSensorPollPeriodMs, ReadAlsThunk, this);
}

gboolean AmbientLightSensor::ReadAls() {
  if (disable_polling_) {
    is_polling_ = false;
    return false; // Returning false removes the timeout.
  }

  // We really want to read the ambient light level.
  // Complete the deferred lux file open if necessary.
  if (als_fd_ < 0) {
    if (!DeferredInit())
      return true; // Return true to try again later.
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
    int64 level = Tsl2563LuxToLevel(luxval);
    if (level != last_level_ && controller_)
      controller_->SetAlsBrightnessLevel(level);
    last_level_ = level;
  }
  return true;
}

int64 AmbientLightSensor::Tsl2563LuxToLevel(int luxval) {
  // Notes on tsl2563 Ambient Light Response (_ALR) table:
  //
  // measurement location: lux file value, intended luma level
  //            dark room: 0,              0
  //               office: 75,             50
  //  outside, day, shade: 1000-3000,      100
  //  outside, day, direct sunlight: 10000, 100
  //
  // Table data comes from 'bc -l':
  //   x=7
  //   while(x>2) {
  //     e(x)
  //     x-=.1
  //   }
  int lux_table[] = {
    8, 9, 10, 11, 12, 13, 14, 16, 18, 20,
    22, 24, 27, 29, 33, 36, 40, 44, 49, 54,
    60, 66, 73, 81, 90, 99, 109, 121, 134, 148,
    164, 181, 200, 221, 244, 270, 298, 330, 365, 403,
    445, 492, 544, 601, 665, 735, 812, 897, 992, 1096,
  };

  // std::lower_bound() gives a pointer to the next highest value (unless
  // there is an exact match), or just off the end of the table if there is no
  // next highest value.
  int* bound = std::lower_bound(lux_table, lux_table + arraysize(lux_table),
                                luxval);

  // Normalize table pointer to 100.
  return ((bound - lux_table) * 100 / arraysize(lux_table));
}

}  // namespace power_manager
