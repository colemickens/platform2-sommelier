// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/ambient_light_handler.h"

#include <limits>

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "power_manager/common/prefs.h"
#include "power_manager/powerd/system/ambient_light_sensor.h"

namespace power_manager {
namespace policy {

namespace {

// Number of light sensor responses required to overcome temporal hysteresis.
const int kHysteresisThreshold = 2;

}  // namespace

AmbientLightHandler::AmbientLightHandler(
    system::AmbientLightSensorInterface* sensor,
    Delegate* delegate)
    : sensor_(sensor),
      delegate_(delegate),
      power_source_(POWER_AC),
      min_brightness_percent_(0.0),
      dimmed_brightness_percent_(10.0),
      max_brightness_percent_(60.0),
      lux_level_(0),
      hysteresis_state_(HYSTERESIS_IMMEDIATE),
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

void AmbientLightHandler::Init(PrefsInterface* prefs,
                               const std::string& limits_pref_name,
                               const std::string& steps_pref_name,
                               double initial_brightness_percent) {
  DCHECK(prefs);

  std::string input_str;
  if (prefs->GetString(limits_pref_name, &input_str)) {
    std::vector<std::string> inputs;
    base::SplitString(input_str, '\n', &inputs);
    double temp_min, temp_dim, temp_max;
    if (inputs.size() == 3 &&
        base::StringToDouble(inputs[0], &temp_min) &&
        base::StringToDouble(inputs[1], &temp_dim) &&
        base::StringToDouble(inputs[2], &temp_max)) {
      min_brightness_percent_ = temp_min;
      dimmed_brightness_percent_ = temp_dim;
      max_brightness_percent_ = temp_max;
    } else {
      ReplaceSubstringsAfterOffset(&input_str, 0, "\n", "\\n");
      LOG(ERROR) << "Failed to parse limits pref " << limits_pref_name
                 << " with contents: \"" << input_str << "\"";
    }
  } else {
    LOG(ERROR) << "Failed to read limits pref " << limits_pref_name;
  }

  if (prefs->GetString(steps_pref_name, &input_str)) {
    std::vector<std::string> lines;
    base::SplitString(input_str, '\n', &lines);
    for (std::vector<std::string>::iterator iter = lines.begin();
         iter != lines.end(); ++iter) {
      std::vector<std::string> segments;
      base::SplitString(*iter, ' ', &segments);
      BrightnessStep new_step;
      if (segments.size() == 3 &&
          base::StringToDouble(segments[0], &new_step.ac_target_percent) &&
          base::StringToInt(segments[1], &new_step.decrease_lux_threshold) &&
          base::StringToInt(segments[2], &new_step.increase_lux_threshold)) {
        new_step.battery_target_percent = new_step.ac_target_percent;
      } else if (
          segments.size() == 4 &&
          base::StringToDouble(segments[0], &new_step.ac_target_percent) &&
          base::StringToDouble(segments[1], &new_step.battery_target_percent) &&
          base::StringToInt(segments[2], &new_step.decrease_lux_threshold) &&
          base::StringToInt(segments[3], &new_step.increase_lux_threshold)) {
        // Okay, we've read all the fields.
      } else {
        LOG(ERROR) << "Skipping line in steps pref " << steps_pref_name
                   << ": \"" << *iter << "\"";
        continue;
      }
      steps_.push_back(new_step);
    }
  } else {
    LOG(ERROR) << "Failed to read steps pref " << steps_pref_name;
  }

  // If we don't have any values in |steps_|, insert a default value.
  if (steps_.empty()) {
    BrightnessStep default_step;
    default_step.ac_target_percent = max_brightness_percent_;
    default_step.battery_target_percent = max_brightness_percent_;
    default_step.decrease_lux_threshold = -1;
    default_step.increase_lux_threshold = -1;
    steps_.push_back(default_step);
  }

  // Start at the step nearest to the initial backlight level.
  double percent_delta = std::numeric_limits<double>::max();
  for (size_t i = 0; i < steps_.size(); i++) {
    double temp_delta =
        abs(initial_brightness_percent - steps_[i].ac_target_percent);
    if (temp_delta < percent_delta) {
      percent_delta = temp_delta;
      step_index_ = i;
    }
  }
  CHECK(step_index_ >= 0);
  CHECK(step_index_ < steps_.size());

  // Create a synthetic lux value that is in line with |step_index_|.
  // If one or both of the thresholds are unbounded, just do the best we
  // can.
  if (steps_[step_index_].decrease_lux_threshold >= 0 &&
      steps_[step_index_].increase_lux_threshold >= 0) {
    lux_level_ = steps_[step_index_].decrease_lux_threshold +
        (steps_[step_index_].increase_lux_threshold -
         steps_[step_index_].decrease_lux_threshold) / 2;
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
    VLOG(1) << "Going from " << old_percent << "% to " << new_percent
            << "% for power source change (" << name_ << ")";
    delegate_->SetBrightnessPercentForAmbientLight(
        new_percent, CAUSED_BY_POWER_SOURCE);
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

  if (hysteresis_state_ != HYSTERESIS_IMMEDIATE && new_lux == lux_level_) {
    hysteresis_state_ = HYSTERESIS_STABLE;
    return;
  }

  int new_step_index = step_index_;
  int num_steps = steps_.size();
  if (new_lux > lux_level_) {
    if (hysteresis_state_ != HYSTERESIS_IMMEDIATE &&
        hysteresis_state_ != HYSTERESIS_INCREASING) {
      VLOG(2) << "ALS transitioned to brightness increasing (" << name_ << ")";
      hysteresis_state_ = HYSTERESIS_INCREASING;
      hysteresis_count_ = 0;
    }
    for (; new_step_index < num_steps; new_step_index++) {
      if (new_lux < steps_[new_step_index].increase_lux_threshold ||
          steps_[new_step_index].increase_lux_threshold == -1)
        break;
    }
  } else if (new_lux < lux_level_) {
    if (hysteresis_state_ != HYSTERESIS_IMMEDIATE &&
        hysteresis_state_ != HYSTERESIS_DECREASING) {
      VLOG(2) << "ALS transitioned to brightness decreasing (" << name_ << ")";
      hysteresis_state_ = HYSTERESIS_DECREASING;
      hysteresis_count_ = 0;
    }
    for (; new_step_index >= 0; new_step_index--) {
      if (new_lux > steps_[new_step_index].decrease_lux_threshold ||
          steps_[new_step_index].decrease_lux_threshold == -1)
        break;
    }
  }
  DCHECK_GE(new_step_index, 0);
  DCHECK_LT(new_step_index, num_steps);

  if (hysteresis_state_ == HYSTERESIS_IMMEDIATE) {
    step_index_ = new_step_index;
    double target_percent = GetTargetPercent();
    VLOG(1) << "Immediately going to " << target_percent << "% (step "
            << step_index_ << ") for lux " << new_lux << " (" << name_ << ")";
    lux_level_ = new_lux;
    hysteresis_state_ = HYSTERESIS_STABLE;
    hysteresis_count_ = 0;
    delegate_->SetBrightnessPercentForAmbientLight(
        target_percent, CAUSED_BY_AMBIENT_LIGHT);
    sent_initial_adjustment_ = true;
    return;
  }

  if (static_cast<int>(step_index_) == new_step_index)
    return;

  hysteresis_count_++;
  VLOG(2) << "Incremented hysteresis count to " << hysteresis_count_
          << " (lux went from " << lux_level_ << " to " << new_lux << ") ("
          << name_ << ")";
  if (hysteresis_count_ >= kHysteresisThreshold) {
    step_index_ = new_step_index;
    double target_percent = GetTargetPercent();
    VLOG(1) << "Hysteresis overcome; transitioning to " << target_percent
            << "% (step " << step_index_ << ") for lux " << new_lux
            << " (" << name_ << ")";
    lux_level_ = new_lux;
    hysteresis_count_ = 1;
    delegate_->SetBrightnessPercentForAmbientLight(
        target_percent, CAUSED_BY_AMBIENT_LIGHT);
    sent_initial_adjustment_ = true;
  }
}

double AmbientLightHandler::GetTargetPercent() const {
  DCHECK_LT(step_index_, steps_.size());
  return power_source_ == POWER_AC ?
      steps_[step_index_].ac_target_percent :
      steps_[step_index_].battery_target_percent;
}

}  // namespace policy
}  // namespace power_manager
