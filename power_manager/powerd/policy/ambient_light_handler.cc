// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/ambient_light_handler.h"

#include <cmath>
#include <limits>

#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

#include "power_manager/powerd/system/ambient_light_sensor.h"

namespace power_manager {
namespace policy {

namespace {

// Number of light sensor responses required to overcome temporal hysteresis.
const int kHysteresisThreshold = 2;

}  // namespace

// static
BacklightBrightnessChange_Cause AmbientLightHandler::ToProtobufCause(
    BrightnessChangeCause als_cause) {
  switch (als_cause) {
    case BrightnessChangeCause::AMBIENT_LIGHT:
      return BacklightBrightnessChange_Cause_AMBIENT_LIGHT_CHANGED;
    case BrightnessChangeCause::EXTERNAL_POWER_CONNECTED:
      return BacklightBrightnessChange_Cause_EXTERNAL_POWER_CONNECTED;
    case BrightnessChangeCause::EXTERNAL_POWER_DISCONNECTED:
      return BacklightBrightnessChange_Cause_EXTERNAL_POWER_DISCONNECTED;
  }
  NOTREACHED() << "Invalid cause " << static_cast<int>(als_cause);
  return BacklightBrightnessChange_Cause_AMBIENT_LIGHT_CHANGED;
}

AmbientLightHandler::AmbientLightHandler(
    system::AmbientLightSensorInterface* sensor, Delegate* delegate)
    : sensor_(sensor),
      delegate_(delegate),
      power_source_(PowerSource::AC),
      lux_level_(0),
      hysteresis_state_(HysteresisState::IMMEDIATE),
      hysteresis_count_(0),
      step_index_(0),
      sent_initial_adjustment_(false) {
  DCHECK(sensor_);
  DCHECK(delegate_);
  sensor_->AddObserver(this);
}

AmbientLightHandler::~AmbientLightHandler() {
  sensor_->RemoveObserver(this);
}

void AmbientLightHandler::Init(const std::string& steps_pref_value,
                               double initial_brightness_percent) {
  std::vector<std::string> lines = base::SplitString(
      steps_pref_value, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (std::vector<std::string>::iterator iter = lines.begin();
       iter != lines.end(); ++iter) {
    std::vector<std::string> segments = base::SplitString(
        *iter, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    BrightnessStep new_step;
    if (segments.size() == 3 &&
        base::StringToDouble(segments[0], &new_step.ac_target_percent) &&
        base::StringToInt(segments[1], &new_step.decrease_lux_threshold) &&
        base::StringToInt(segments[2], &new_step.increase_lux_threshold)) {
      new_step.battery_target_percent = new_step.ac_target_percent;
    } else if (segments.size() == 4 &&
               base::StringToDouble(segments[0], &new_step.ac_target_percent) &&
               base::StringToDouble(segments[1],
                                    &new_step.battery_target_percent) &&
               base::StringToInt(segments[2],
                                 &new_step.decrease_lux_threshold) &&
               base::StringToInt(segments[3],
                                 &new_step.increase_lux_threshold)) {
      // Okay, we've read all the fields.
    } else {
      LOG(FATAL) << "Steps pref has invalid line \"" << *iter << "\"";
    }
    steps_.push_back(new_step);
  }

  // The bottom and top steps should have infinite ranges to ensure that we
  // don't fall off either end.
  CHECK(!steps_.empty()) << "No brightness steps defined in pref";
  CHECK_EQ(steps_.front().decrease_lux_threshold, -1);
  CHECK_EQ(steps_.back().increase_lux_threshold, -1);

  // Start at the step nearest to the initial backlight level.
  double percent_delta = std::numeric_limits<double>::max();
  for (size_t i = 0; i < steps_.size(); i++) {
    double temp_delta =
        fabs(initial_brightness_percent - steps_[i].ac_target_percent);
    if (temp_delta < percent_delta) {
      percent_delta = temp_delta;
      step_index_ = i;
    }
  }
  CHECK_LT(step_index_, steps_.size());

  // Create a synthetic lux value that is in line with |step_index_|.
  // If one or both of the thresholds are unbounded, just do the best we
  // can.
  if (steps_[step_index_].decrease_lux_threshold >= 0 &&
      steps_[step_index_].increase_lux_threshold >= 0) {
    lux_level_ = steps_[step_index_].decrease_lux_threshold +
                 (steps_[step_index_].increase_lux_threshold -
                  steps_[step_index_].decrease_lux_threshold) /
                     2;
  } else if (steps_[step_index_].decrease_lux_threshold >= 0) {
    lux_level_ = steps_[step_index_].decrease_lux_threshold;
  } else if (steps_[step_index_].increase_lux_threshold >= 0) {
    lux_level_ = steps_[step_index_].increase_lux_threshold;
  } else {
    lux_level_ = 0;
  }
}

void AmbientLightHandler::HandlePowerSourceChange(PowerSource source) {
  if (source == power_source_)
    return;

  double old_percent = GetTargetPercent();
  power_source_ = source;
  double new_percent = GetTargetPercent();
  if (new_percent != old_percent && sent_initial_adjustment_) {
    LOG(INFO) << "Going from " << old_percent << "% to " << new_percent
              << "% for power source change (" << name_ << ")";
    delegate_->SetBrightnessPercentForAmbientLight(
        new_percent, source == PowerSource::AC
                         ? BrightnessChangeCause::EXTERNAL_POWER_CONNECTED
                         : BrightnessChangeCause::EXTERNAL_POWER_DISCONNECTED);
  }
}

void AmbientLightHandler::OnAmbientLightUpdated(
    system::AmbientLightSensorInterface* sensor) {
  DCHECK_EQ(sensor, sensor_);

  int new_lux = sensor_->GetAmbientLightLux();
  if (new_lux < 0) {
    LOG(WARNING) << "Sensor doesn't have valid value";
    return;
  }

  if (hysteresis_state_ != HysteresisState::IMMEDIATE &&
      new_lux == lux_level_) {
    hysteresis_state_ = HysteresisState::STABLE;
    return;
  }

  int new_step_index = step_index_;
  int num_steps = steps_.size();
  if (new_lux > lux_level_) {
    if (hysteresis_state_ != HysteresisState::IMMEDIATE &&
        hysteresis_state_ != HysteresisState::INCREASING) {
      VLOG(1) << "ALS transitioned to brightness increasing (" << name_ << ")";
      hysteresis_state_ = HysteresisState::INCREASING;
      hysteresis_count_ = 0;
    }
    for (; new_step_index < num_steps; new_step_index++) {
      if (new_lux < steps_[new_step_index].increase_lux_threshold ||
          steps_[new_step_index].increase_lux_threshold == -1)
        break;
    }
  } else if (new_lux < lux_level_) {
    if (hysteresis_state_ != HysteresisState::IMMEDIATE &&
        hysteresis_state_ != HysteresisState::DECREASING) {
      VLOG(1) << "ALS transitioned to brightness decreasing (" << name_ << ")";
      hysteresis_state_ = HysteresisState::DECREASING;
      hysteresis_count_ = 0;
    }
    for (; new_step_index >= 0; new_step_index--) {
      if (new_lux > steps_[new_step_index].decrease_lux_threshold ||
          steps_[new_step_index].decrease_lux_threshold == -1)
        break;
    }
  }
  CHECK_GE(new_step_index, 0);
  CHECK_LT(new_step_index, num_steps);

  if (hysteresis_state_ == HysteresisState::IMMEDIATE) {
    step_index_ = new_step_index;
    double target_percent = GetTargetPercent();
    LOG(INFO) << "Immediately going to " << target_percent << "% (step "
              << step_index_ << ") for lux " << new_lux << " (" << name_ << ")";
    lux_level_ = new_lux;
    hysteresis_state_ = HysteresisState::STABLE;
    hysteresis_count_ = 0;
    delegate_->SetBrightnessPercentForAmbientLight(
        target_percent, BrightnessChangeCause::AMBIENT_LIGHT);
    sent_initial_adjustment_ = true;
    return;
  }

  if (static_cast<int>(step_index_) == new_step_index)
    return;

  hysteresis_count_++;
  VLOG(1) << "Incremented hysteresis count to " << hysteresis_count_
          << " (lux went from " << lux_level_ << " to " << new_lux << ") ("
          << name_ << ")";
  if (hysteresis_count_ >= kHysteresisThreshold) {
    step_index_ = new_step_index;
    double target_percent = GetTargetPercent();
    LOG(INFO) << "Hysteresis overcome; transitioning to " << target_percent
              << "% (step " << step_index_ << ") for lux " << new_lux << " ("
              << name_ << ")";
    lux_level_ = new_lux;
    hysteresis_count_ = 1;
    delegate_->SetBrightnessPercentForAmbientLight(
        target_percent, BrightnessChangeCause::AMBIENT_LIGHT);
    sent_initial_adjustment_ = true;
  }
}

double AmbientLightHandler::GetTargetPercent() const {
  CHECK_LT(step_index_, steps_.size());
  return power_source_ == PowerSource::AC
             ? steps_[step_index_].ac_target_percent
             : steps_[step_index_].battery_target_percent;
}

}  // namespace policy
}  // namespace power_manager
