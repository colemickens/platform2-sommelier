// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/keyboard_backlight_controller.h"
#include "power_manager/powerd/system/ambient_light_sensor.h"
#include "power_manager/powerd/system/backlight_interface.h"

namespace {

// Default control values for the als target percent.
const double kAlsTargetPercentDim = 10.0;
const double kAlsTargetPercentMax = 60.0;
const double kAlsTargetPercentMin = 0.0;

// Default control values for the user target percent.
const double kUserTargetPercentDim = 10.0;
const double kUserTargetPercentMax = 100.0;
const double kUserTargetPercentMin = 0.0;

// Number of light sensor responses required to overcome temporal hysteresis.
const int kAlsHystResponse = 2;

// This is how long after a video playing message is received we should wait
// until reverting to the not playing state. If another message is received in
// this interval the timeout is reset. The browser should be sending these
// messages ~5 seconds when video is playing.
const int64 kVideoTimeoutIntervalMs = 7000;

// Interval between brightness level updates during animated transitions, in
// milliseconds.
const int64 kTransitionUpdateMs = 30;

}  // namespace

namespace power_manager {

const int64 KeyboardBacklightController::kFastTransitionMs = 200;
const int64 KeyboardBacklightController::kSlowTransitionMs = 2000;

KeyboardBacklightController::KeyboardBacklightController(
    system::BacklightInterface* backlight,
    PrefsInterface* prefs,
    system::AmbientLightSensorInterface* sensor)
    : is_initialized_ (false),
      backlight_(backlight),
      prefs_(prefs),
      light_sensor_(sensor),
      observer_(NULL),
      state_(BACKLIGHT_UNINITIALIZED),
      is_video_playing_(false),
      is_fullscreen_(false),
      video_enabled_(true),
      max_level_(0),
      current_level_(0),
      als_target_percent_(0.0),
      user_target_percent_(0.0),
      als_target_percent_dim_(kAlsTargetPercentDim),
      als_target_percent_max_(kAlsTargetPercentMax),
      als_target_percent_min_(kAlsTargetPercentMin),
      user_target_percent_dim_(kUserTargetPercentDim),
      user_target_percent_max_(kUserTargetPercentMax),
      user_target_percent_min_(kUserTargetPercentMin),
      hysteresis_state_(ALS_HYST_IDLE),
      hysteresis_count_(0),
      lux_level_(0),
      als_step_index_(0),
      user_step_index_(-1),
      ignore_ambient_light_(false),
      video_timeout_timer_id_(0),
      num_als_adjustments_(0),
      num_user_adjustments_(0) {
  DCHECK(backlight);
  if (light_sensor_)
    light_sensor_->AddObserver(this);
}

KeyboardBacklightController::~KeyboardBacklightController() {
  if (light_sensor_) {
    light_sensor_->RemoveObserver(this);
    light_sensor_ = NULL;
  }
  HaltVideoTimeout();
}

void KeyboardBacklightController::HandleVideoActivity(
    base::TimeTicks last_activity_time,
    bool is_fullscreen) {
  LOG(INFO) << "Received video event with fullscreen bit "
            << (is_fullscreen ? "" : "not ") << "set";
  int64 timeout_interval_ms = kVideoTimeoutIntervalMs -
      (GetCurrentTime() - last_activity_time).InMilliseconds();
  if (timeout_interval_ms <= 0) {
    LOG(WARNING) << "Didn't get notification about video event before timeout "
                 << "interval was over!";
    is_video_playing_ = false;
    is_fullscreen_ = false;
    return;
  }
  HaltVideoTimeout();
  is_fullscreen_ = is_fullscreen;
  is_video_playing_ = true;
  UpdateBacklightEnabled();
  video_timeout_timer_id_ = g_timeout_add(timeout_interval_ms,
                                          VideoTimeoutThunk,
                                          this);
}

bool KeyboardBacklightController::Init() {
  if (!backlight_->GetMaxBrightnessLevel(&max_level_)) {
    LOG(ERROR) << "Querying backlight during initialization failed";
    is_initialized_ = false;
    return false;
  }

  ReadPrefs();

  if (!ResetAls()) {
    is_initialized_ = false;
    return false;
  }

  is_initialized_ = true;
  return true;
}

void KeyboardBacklightController::SetObserver(
    BacklightControllerObserver* observer) {
  observer_ = observer;
}

double KeyboardBacklightController::GetTargetBrightnessPercent() {
  return (user_step_index_ != -1) ? user_target_percent_ : als_target_percent_;
}

bool KeyboardBacklightController::GetCurrentBrightnessPercent(double* percent) {
  DCHECK(percent);
  *percent = LevelToPercent(current_level_);
  return *percent >= 0.0;
}

bool KeyboardBacklightController::SetCurrentBrightnessPercent(
    double percent,
    BrightnessChangeCause cause,
    TransitionStyle style) {
  if (cause == BRIGHTNESS_CHANGE_AUTOMATED) {
    als_target_percent_ = std::max(std::min(percent, als_target_percent_max_),
                                   als_target_percent_min_);
    LOG(INFO) << "als_target_percent_ set to " << als_target_percent_;
  } else {
    if (user_step_index_ == -1) {
      LOG(ERROR) << "Attempting to set user controller brightness without first"
                 << " initializing user_step_index!";
      return false;
    }
    user_target_percent_ = std::max(std::min(percent, user_target_percent_max_),
                                    user_target_percent_min_);
    LOG(INFO) << "user_target_percent_ set to " << user_target_percent_;
  }
  int64 new_level = GetNewLevel();
  if (new_level == current_level_) {
    LOG(INFO) << "No change in light level (" << current_level_
              << ") , so no transition";
    return false;
  }
  LOG(INFO) << "Changing Brightness, state = " << PowerStateToString(state_)
            << ", new level = " << new_level << ", transition style = "
            << TransitionStyleToString(style);
  current_level_ = new_level;

  backlight_->SetBrightnessLevel(new_level, GetTransitionDuration(style));
  if (observer_)
    observer_->OnBrightnessChanged(LevelToPercent(new_level), cause, this);
  return true;
}

bool KeyboardBacklightController::IncreaseBrightness(
    BrightnessChangeCause cause) {
  if (cause == BRIGHTNESS_CHANGE_USER_INITIATED) {
    num_user_adjustments_++;
    if (user_step_index_ == -1)
      InitializeUserStepIndex();
    const ssize_t num_steps = user_steps_.size();
    if (user_step_index_ < num_steps - 1)
      user_step_index_++;
    SetCurrentBrightnessPercent(user_steps_[user_step_index_],
                                BRIGHTNESS_CHANGE_USER_INITIATED,
                                TRANSITION_FAST);
    return true;
  }
  LOG(ERROR) << "Received IncreaseBrightness request from non-user, ignoring!";
  return false;
}

bool KeyboardBacklightController::DecreaseBrightness(
    bool allow_off,
    BrightnessChangeCause cause) {
  if (cause == BRIGHTNESS_CHANGE_USER_INITIATED) {
    num_user_adjustments_++;
    if (user_step_index_ == -1)
      InitializeUserStepIndex();
    if (user_step_index_ > 0)
        user_step_index_--;
    SetCurrentBrightnessPercent(user_steps_[user_step_index_],
                                BRIGHTNESS_CHANGE_USER_INITIATED,
                                TRANSITION_FAST);
    return true;
  }
  LOG(ERROR) << "Received DecreaseBrightness request from non-user, ignoring!";
  return false;
}

bool KeyboardBacklightController::SetPowerState(PowerState new_state) {
  if (new_state == state_ || !is_initialized_)
    return false;

  CHECK(new_state != BACKLIGHT_UNINITIALIZED);
  LOG(INFO) << "Changing state: " << PowerStateToString(state_) << " -> "
            << PowerStateToString(new_state);
  PowerState old_state = state_;
  state_ = new_state;

  // Early return, let the EC deal with the backlight.
  if (old_state == BACKLIGHT_SUSPENDED || state_ == BACKLIGHT_SUSPENDED)
    return true;

  if (state_ == BACKLIGHT_SHUTTING_DOWN) {
    backlight_->SetBrightnessLevel(0, base::TimeDelta());
    return true;
  }

  // When returning to active we need to restart the internal ALS values since
  // we might have been ignoring changes from the ALS.
  if (old_state != BACKLIGHT_ACTIVE && state_ == BACKLIGHT_ACTIVE) {
    if (!ResetAls())
      return false;
  }

  SetCurrentBrightnessPercent(PercentToLevel(GetNewLevel()),
                              user_step_index_ != -1 ?
                              BRIGHTNESS_CHANGE_USER_INITIATED :
                              BRIGHTNESS_CHANGE_AUTOMATED,
                              TRANSITION_SLOW);
  return true;
}

PowerState KeyboardBacklightController::GetPowerState() const {
  return state_;
}

bool KeyboardBacklightController::IsBacklightActiveOff() {
  return (video_enabled_ && current_level_ == 0);
}

int KeyboardBacklightController::GetNumAmbientLightSensorAdjustments() const {
  return num_als_adjustments_;
}

int KeyboardBacklightController::GetNumUserAdjustments() const {
  return num_user_adjustments_;
}

void KeyboardBacklightController::OnBacklightDeviceChanged() {
  LOG(INFO) << "Backlight device changed; reinitializing controller";
  bool use_user = user_step_index_ != -1;
  double target_percent = use_user ? user_target_percent_ : als_target_percent_;
  if (Init()) {
    SetCurrentBrightnessPercent(target_percent,
                                use_user ?
                                BRIGHTNESS_CHANGE_USER_INITIATED :
                                BRIGHTNESS_CHANGE_AUTOMATED,
                                TRANSITION_INSTANT);
  }
}

void KeyboardBacklightController::OnAmbientLightChanged(
    system::AmbientLightSensorInterface* sensor) {
#ifndef HAS_ALS
  LOG(WARNING) << "Got ALS reading from platform supposed to have no ALS. "
               << "Please check the platform ALS configuration.";
#endif

  DCHECK_EQ(sensor, light_sensor_);

  if (ignore_ambient_light_)
    return;

  int new_lux = light_sensor_->GetAmbientLightLux();
  if (new_lux < 0) {
    LOG(WARNING) << "ALS doesn't have valid value after sending "
                 << "OnAmbientLightChanged";
    return;
  }

  if (new_lux == lux_level_) {
    hysteresis_state_ = ALS_HYST_IDLE;
    return;
  }

  ssize_t i = -1;
  const ssize_t num_steps = als_steps_.size();
  if (new_lux > lux_level_) {  // Brightness Increasing
    if (hysteresis_state_ != ALS_HYST_UP) {
      LOG(INFO) << "ALS transitioned to brightness increasing";
      hysteresis_state_ = ALS_HYST_UP;
      hysteresis_count_ = 0;
    }
    for (i = als_step_index_; i < num_steps; i++) {
      if (new_lux < als_steps_[i].increase_threshold ||
          als_steps_[i].increase_threshold == -1)
        break;
    }
  } else {  // Brightness decreasing
    if (hysteresis_state_ != ALS_HYST_DOWN) {
      LOG(INFO) << "ALS transitioned to brightness decreasing";
      hysteresis_state_ = ALS_HYST_DOWN;
      hysteresis_count_ = 0;
    }
    for (i = als_step_index_; i >= 0; i--) {
      if (new_lux > als_steps_[i].decrease_threshold ||
          als_steps_[i].decrease_threshold == -1)
        break;
    }
  }

  if (i >= num_steps || i < 0) {
    LOG(ERROR) << "When trying to find new brightness step for lux value of "
               << new_lux << ", index ended up out of range with a value of "
               << i;
    return;
  }

  if (als_step_index_ == i)
    return;

  hysteresis_count_++;
  LOG(INFO) << "lux_level_ = " << lux_level_ << " and new_lux = " << new_lux;
  LOG(INFO) << "Incremented hysteresis count to " << hysteresis_count_;
  if (hysteresis_count_ >= kAlsHystResponse) {
    LOG(INFO) << "Hystersis overcome, transitioning step "
              << als_step_index_ << " -> step " << i;
    LOG(INFO) << "ALS history (most recent first): "
              << light_sensor_->DumpLuxHistory();
    als_step_index_ = i;
    lux_level_ = new_lux;
    hysteresis_count_ = 1;
    SetCurrentBrightnessPercent(
        als_steps_[als_step_index_].target_percent,
        BRIGHTNESS_CHANGE_AUTOMATED,
        TRANSITION_SLOW);
  }
}

// static
base::TimeDelta KeyboardBacklightController::GetTransitionDuration(
    TransitionStyle style) {
  switch (style) {
    case TRANSITION_INSTANT:
      return base::TimeDelta();
    case TRANSITION_FAST:
      return base::TimeDelta::FromMilliseconds(kFastTransitionMs);
    case TRANSITION_SLOW:
      return base::TimeDelta::FromMilliseconds(kSlowTransitionMs);
    default:
      NOTREACHED() << "Unhandled transition style " << style;
      return base::TimeDelta();
  }
}

base::TimeTicks KeyboardBacklightController::GetCurrentTime() const {
  return current_time_for_testing_.is_null() ?
         base::TimeTicks::Now() :
         current_time_for_testing_;
}

void KeyboardBacklightController::ReadPrefs() {
  ReadLimitsPrefs(kKeyboardBacklightAlsLimitsPref, &als_target_percent_min_,
                  &als_target_percent_dim_, &als_target_percent_max_);
  ReadLimitsPrefs(kKeyboardBacklightUserLimitsPref, &user_target_percent_min_,
                  &user_target_percent_dim_, &user_target_percent_max_);
  ReadAlsStepsPref(kKeyboardBacklightAlsStepsPref);
  ReadUserStepsPref(kKeyboardBacklightUserStepsPref);
  prefs_->GetBool(kDisableALSPref, &ignore_ambient_light_);
}

void KeyboardBacklightController::ReadLimitsPrefs(const char* prefs_file,
                                                  double* min,
                                                  double* dim,
                                                  double* max) {
  double temp_min = *min;
  double temp_dim = *dim;
  double temp_max = *max;
  std::string input_str;
  if (prefs_->GetString(prefs_file, &input_str)) {
    std::vector<std::string> inputs;
    base::SplitString(input_str, '\n', &inputs);
    if ((inputs.size() != 3) ||
        !base::StringToDouble(inputs[0], min) ||
        !base::StringToDouble(inputs[1], dim) ||
        !base::StringToDouble(inputs[2], max)) {
      *min = temp_min;
      *dim = temp_dim;
      *max = temp_max;
      LOG(ERROR) << "Failed to parse limits prefs file (" << prefs_file
                 << "), with contents:\n\t" << input_str;
    }
  } else {
    LOG(ERROR) << "Failed to read limits prefs file!";
  }
}

void KeyboardBacklightController::ReadAlsStepsPref(const char* prefs_file) {
  als_steps_.clear();
  std::string input_str;
  if (prefs_->GetString(prefs_file, &input_str)) {
    std::vector<std::string> lines;
    base::SplitString(input_str, '\n', &lines);
    for (std::vector<std::string>::iterator iter = lines.begin();
         iter < lines.end(); ++iter) {
      std::vector<std::string> segments;
      base::SplitString(*iter, ' ', &segments);
      if (segments.size() != 3) {
        LOG(ERROR) << "Skipping line in keyboard brightness als steps file:"
                   << *iter;
        continue;
      }
      struct BrightnessStep new_step;
      if (!base::StringToDouble(segments[0], &new_step.target_percent) ||
          !base::StringToInt(segments[1], &new_step.decrease_threshold) ||
          !base::StringToInt(segments[2],  &new_step.increase_threshold)) {
        LOG(ERROR) << "Failure in parse string: " << *iter << " -> ("
                   << new_step.target_percent << ", "
                   << new_step.decrease_threshold << ", "
                   << new_step.increase_threshold << ")";
        continue;
      } else {
        als_steps_.push_back(new_step);
      }
    }
  } else {
    LOG(ERROR) << "Failed to read ALS steps file!";
  }

  // If don't have any values in |als_steps_| insert a default value so
  // that we can effectively ignore the ALS, but still operate in a reasonable
  // manner.
  if (als_steps_.empty()) {
    struct BrightnessStep default_step;
    default_step.target_percent = als_target_percent_max_;
    default_step.decrease_threshold = -1;
    default_step.increase_threshold = -1;
    als_steps_.push_back(default_step);
    LOG(INFO) << "No brightness steps read; inserted default step = ("
              << default_step.target_percent << ", "
              << default_step.decrease_threshold << ", "
              << default_step.increase_threshold << ")";
  }
}

void KeyboardBacklightController::ReadUserStepsPref(const char* prefs_file) {
  std::string input_str;
  user_steps_.clear();
  if (prefs_->GetString(prefs_file, &input_str)) {
    std::vector<std::string> lines;
    base::SplitString(input_str, '\n', &lines);
    for (std::vector<std::string>::iterator iter = lines.begin();
         iter < lines.end(); ++iter) {
      double new_step;
      if (!base::StringToDouble(*iter, &new_step)) {
        LOG(ERROR) << "Failure in parse string: " << *iter << " -> "
                   << new_step;
      } else {
        user_steps_.push_back(new_step);
      }
    }
  } else {
    LOG(ERROR) << "Failed to read user steps file!";
  }

  if (user_steps_.empty()) {
    LOG(INFO) << "No user steps read in, seeding user steps using "
              << "user_target_percent_[min,dim,max]";
    user_steps_.push_back(user_target_percent_min_);
    user_steps_.push_back(user_target_percent_dim_);
    user_steps_.push_back(user_target_percent_max_);
  }
}

void KeyboardBacklightController::UpdateBacklightEnabled() {
  bool new_video_enabled = (!is_video_playing_ || !is_fullscreen_);
  if (new_video_enabled == video_enabled_) {
    LOG(INFO) << "video_enabled_ stays at " << video_enabled_;
    return;
  }
  LOG(INFO) << "video_enabled_ transitions " << video_enabled_ << " -> "
            << new_video_enabled;
  video_enabled_ = new_video_enabled;
  SetCurrentBrightnessPercent(als_target_percent_,
                              BRIGHTNESS_CHANGE_AUTOMATED,
                              TRANSITION_SLOW);
}

void KeyboardBacklightController::HaltVideoTimeout() {
  util::RemoveTimeout(&video_timeout_timer_id_ );
}

gboolean KeyboardBacklightController::VideoTimeout() {
  is_video_playing_ = false;
  UpdateBacklightEnabled();
  video_timeout_timer_id_ = 0;
  return FALSE;
}

int64 KeyboardBacklightController::PercentToLevel(double percent) const {
  if (max_level_ == 0)
    return -1;
  percent = std::max(std::min(percent, 100.0), 0.0);
  return lround(static_cast<double>(max_level_) * percent / 100.0);
}

double KeyboardBacklightController::LevelToPercent(int64 level) const {
  if (max_level_ == 0)
    return -1.0;
  level = std::max(std::min(level, max_level_), static_cast<int64>(0));
  return static_cast<double>(level) * 100.0 / max_level_;
}

void KeyboardBacklightController::InitializeUserStepIndex() {
  if (user_step_index_ != -1) {
    LOG(ERROR) << "Attempted to initialize user index, but already intialized!";
    return;
  }
  // Capping in case we are still using the firmware set value, which can be
  // larger then our expected range.
  double percent = std::min(LevelToPercent(current_level_),
                            user_target_percent_max_);
  // Select the nearest step to the current backlight level and adjust the
  // target percent in line with it. This follows the same basic procedure that
  // is used for initializing |als_target_percent_|.
  double percent_delta = 2 * user_target_percent_max_;
  size_t i;
  for (i = 0; i < user_steps_.size(); i++) {
    double temp_delta = abs(percent - user_steps_[i]);
    if (temp_delta < percent_delta) {
      percent_delta = temp_delta;
      user_step_index_ = i;
    }
  }
  CHECK(percent_delta < 2 * user_target_percent_max_);
  user_target_percent_ = user_steps_[user_step_index_];
}

int64 KeyboardBacklightController::GetNewLevel() const {
  bool use_user = user_step_index_ != -1;
  if (!use_user && !video_enabled_) {
    LOG(INFO) << "Backlight disabled, minimizing backlight";
    return PercentToLevel(als_target_percent_min_);
  }

  switch (state_) {
    case BACKLIGHT_ACTIVE:
      return PercentToLevel(use_user ? user_target_percent_ :
                            als_target_percent_);
      break;
    case BACKLIGHT_DIM:
      return std::min(current_level_,
                      PercentToLevel(use_user ?
                                     user_target_percent_dim_ :
                                     als_target_percent_dim_));
      break;
    case BACKLIGHT_IDLE_OFF:
    case BACKLIGHT_SUSPENDED:
      return PercentToLevel(use_user ?
                            user_target_percent_min_ : als_target_percent_min_);
      break;
    default:
      return current_level_;
      break;
  };
}

bool KeyboardBacklightController::ResetAls() {
  if (!backlight_->GetCurrentBrightnessLevel(&current_level_)) {
    LOG(ERROR) << "Querying backlight during ALS reset failed";
    return false;
  }

  // This needs to be clamped since the brightness steps that are defined might
  // not use the whole range of the backlight, so the EC set level might be out
  // of range.
  double percent = std::min(LevelToPercent(current_level_),
                            als_target_percent_max_);
  // Select the nearest step to the current backlight level and adjust the
  // target percent in line with it. Set |percent_delta| to an arbitrary too
  // large delta and go through |als_steps_| minimizing the difference
  // between the step's |als_target_percent_| and the percent calculated above.
  double percent_delta = 2 * als_target_percent_max_;
  for (size_t i = 0; i < als_steps_.size(); i++) {
    double temp_delta = abs(percent - als_steps_[i].target_percent);
    if (temp_delta < percent_delta) {
      percent_delta = temp_delta;
      als_step_index_ = i;
      als_target_percent_ = als_steps_[i].target_percent;
    }
  }
  CHECK(percent_delta < 2 * als_target_percent_max_);

  hysteresis_state_ = ALS_HYST_IDLE;
  hysteresis_count_ = 0;
  // Create a synthetic lux value that is inline with |als_step_index_|.
  lux_level_ = als_steps_[als_step_index_].decrease_threshold +
      (als_steps_[als_step_index_].increase_threshold -
       als_steps_[als_step_index_].decrease_threshold) / 2;
  LOG(INFO) << "Created synthetic lux value of " << lux_level_;

  return true;
}

}  // namespace power_manager
