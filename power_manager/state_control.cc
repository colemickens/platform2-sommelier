// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/time.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/file_tagger.h"
#include "power_manager/power_constants.h"
#include "power_manager/power_prefs.h"
#include "power_manager/powerd.h"
#include "power_manager/state_control.h"
#include "power_manager/util.h"
#include "power_state_control.pb.h"

using std::min;

namespace {
// Max duration to disable for in seconds.
const unsigned int kMaxDurationSec = 30 * 60;
// Number of times to retry to find an available id.
const unsigned int kMaxRetries = 20;
}  // namespace

namespace power_manager {

StateControl::StateControl(Daemon* daemon)
    : last_id_(0),
      next_check_(0),
      max_duration_(kMaxDurationSec),
      disable_idle_dim_(false),
      disable_idle_blank_(false),
      disable_idle_suspend_(false),
      disable_lid_suspend_(false),
      daemon_(daemon) {}

StateControl::~StateControl() {
  StateControlList::iterator iter = state_override_list_.begin();
  while (state_override_list_.end() != iter) {
    delete iter->second;
    ++iter;
  }
}

void StateControl::DumpInfoRec(const StateControlInfo* info) {
  LOG(INFO) << "Dumping info record: ";
  LOG(INFO) << "  request_id: " << info->request_id;
  LOG(INFO) << "  expires: " << info->expires;
  LOG(INFO) << "  duration: " << info->duration;
  LOG(INFO) << "  disable_idle_dim: " << info->disable_idle_dim;
  LOG(INFO) << "  disable_idle_blank: " << info->disable_idle_blank;
  LOG(INFO) << "  disable_idle_suspend: " << info->disable_idle_suspend;
  LOG(INFO) << "  disable_lid_suspend: " << info->disable_lid_suspend;
}

void StateControl::RescanState(time_t cur_time) {
  next_check_ = 0;
  disable_idle_dim_ = false;
  disable_idle_blank_ = false;
  disable_idle_suspend_ = false;
  disable_lid_suspend_ = false;

  // Allow a passed in value for testing purposes.
  if (cur_time == 0)
    cur_time = time(NULL);

  StateControlList::iterator iter = state_override_list_.begin();
  while (state_override_list_.end() != iter) {
    if (iter->second->expires <= cur_time) {
      LOG(INFO) << "Request_id " << iter->second->request_id
                << " removed due to expiry.";
      delete iter->second;
      state_override_list_.erase(iter++);
      continue;
    }
    disable_idle_dim_|= iter->second->disable_idle_dim;
    disable_idle_blank_ |= iter->second->disable_idle_blank;
    disable_idle_suspend_ |= iter->second->disable_idle_suspend;
    disable_lid_suspend_ |= iter->second->disable_lid_suspend;
    if (next_check_ == 0) {
      next_check_ = iter->second->expires;
    } else {
      next_check_ = min(next_check_, iter->second->expires);
    }
    ++iter;
  }
  LOG(INFO) << "Rescanned states:"
            << " disable_idle_dim = "     << disable_idle_dim_
            << " disable_idle_blank = "   << disable_idle_blank_
            << " disable_idle_suspend = " << disable_idle_suspend_
            << " disable_lid_suspend = "  << disable_lid_suspend_;
  if (next_check_  > 0)
    g_timeout_add_seconds(next_check_ - time(NULL), RecordExpiredThunk, this);
}

void StateControl::RemoveOverride(int request_id) {
  StateControlList::iterator iter = state_override_list_.find(request_id);
  if (state_override_list_.end() == iter) {
    LOG(WARNING) << "RemoveOverride id " << request_id << " not found";
    return;
  }
  delete iter->second;
  state_override_list_.erase(iter);
  RescanState();
}

void StateControl::RemoveOverrideAndUpdate(int request_id) {
  bool old_idle_dim = disable_idle_dim_;
  bool old_idle_blank = disable_idle_blank_;
  bool old_idle_suspend = disable_idle_suspend_;
  bool old_lid_suspend = disable_lid_suspend_;

  RemoveOverride(request_id);
  CHECK(daemon_);
  if ((old_idle_dim && !disable_idle_dim_) ||
      (old_idle_blank && !disable_idle_blank_) ||
      (old_idle_suspend && !disable_idle_suspend_)) {
    daemon_->UpdateIdleStates();
  }
  if (old_lid_suspend && !disable_lid_suspend_)
    util::SendSignalToPowerM(kCheckLidStateSignal);
}

gboolean StateControl::RecordExpired() {
  LOG(INFO) << "Expiring StateControl entries";
  RescanState();
  CHECK(daemon_);
  daemon_->UpdateIdleStates();
  return false;
}

bool StateControl::StateOverrideRequest(const char* data, const int size,
                                        int* return_value) {
  // first decode the protobuff
  PowerStateControl protobuf;
  StateControlInfo info;

  if (!protobuf.ParseFromArray(data, size)) {
    LOG(ERROR) << "Failed to parse protobuf";
    return false;
  }

  info.request_id = protobuf.request_id();
  info.duration = protobuf.duration();
  info.disable_idle_dim = protobuf.disable_idle_dim();
  info.disable_idle_blank = protobuf.disable_idle_blank();
  info.disable_idle_suspend = protobuf.disable_idle_suspend();
  info.disable_lid_suspend = protobuf.disable_lid_suspend();

  return StateOverrideRequestStruct(&info, return_value);
}

bool StateControl::StateOverrideRequestStruct(const StateControlInfo* request,
                                              int* return_value) {
  // We only allow disabling states that include previous states.
  // This way we won't go from full on to suspend and confuse users.
  if (request->disable_idle_dim &&
      (!request->disable_idle_blank || !request->disable_idle_suspend)) {
    // Only allow disabling idle dim if both blank and suspend are disabled.
    // This way a user will get a warning before those events happen.
    LOG(ERROR) << "Illegal request to disable dim but not blank and suspend";
    return false;
  }
  if (request->disable_idle_blank && !request->disable_idle_suspend) {
    // Only disable blanking if suspend is disabled.
    // This way a user will have a warning that the system will soon suspend.
    LOG(ERROR) << "Illegal request to disable blank but not suspend";
    return false;
  }
  if (request->duration == 0) {
    LOG(ERROR) << "Duration must be greater than 0";
    return false;
  }
  if (request->duration > max_duration_) {
    LOG(ERROR) << "Duration " << request->duration << " passed but max is "
               << max_duration_;
    return false;
  }

  StateControlInfo *new_entry = NULL;

  if (request->request_id != 0) {
    StateControlList::iterator iter =
      state_override_list_.find(request->request_id);
    if (state_override_list_.end() != iter) {
      new_entry = iter->second;
    } else {
      LOG(WARNING) << "StateOverrideRequest had request_id "
                   << request->request_id << " but no entry exists for it";
      return false;
    }
  }
  if (!new_entry) {
    new_entry = new StateControlInfo();
    // Try kMaxRetries (20) times to find unused ID. Should be rare we collide
    for (unsigned int i = 0; i < kMaxRetries; i++) {
      new_entry->request_id = ++last_id_;
      LOG(INFO) << "Checking " << new_entry->request_id;
      if (new_entry->request_id != 0 &&
          state_override_list_.count(new_entry->request_id) == 0) {
        break;
      }
    }
    if (new_entry->request_id == 0 ||
        state_override_list_.count(new_entry->request_id) != 0) {
      LOG(ERROR) << "Could not get unused index to store request";
      LOG(ERROR) << "Map size is " << state_override_list_.size();
      delete new_entry;
      return(false);
    }
  }
  new_entry->duration             = request->duration;
  new_entry->expires              = time(NULL) + request->duration;
  new_entry->disable_idle_dim     = request->disable_idle_dim;
  new_entry->disable_idle_blank   = request->disable_idle_blank;
  new_entry->disable_idle_suspend = request->disable_idle_suspend;
  new_entry->disable_lid_suspend  = request->disable_lid_suspend;

  state_override_list_[new_entry->request_id] = new_entry;

  // now we update the globals
  disable_idle_dim_     |= new_entry->disable_idle_dim;
  disable_idle_blank_   |= new_entry->disable_idle_blank;
  disable_idle_suspend_ |= new_entry->disable_idle_suspend;
  disable_lid_suspend_  |= new_entry->disable_lid_suspend;

  LOG(INFO) << "New override added: "
            << " request_id = "           << new_entry->request_id
            << " duration = "             << new_entry->duration
            << " disable_idle_dim = "     << new_entry->disable_idle_dim
            << " disable_idle_blank = "   << new_entry->disable_idle_blank
            << " disable_idle_suspend = " << new_entry->disable_idle_suspend
            << " disable_lid_suspend = "  << new_entry->disable_lid_suspend;

  if (next_check_ == 0 || new_entry->expires < next_check_) {
    next_check_ = new_entry->expires;
    g_timeout_add_seconds(next_check_ - time(NULL), RecordExpiredThunk, this);
  }

  *return_value = new_entry->request_id;
  return true;
}

bool StateControl::IsStateDisabled(StateControlStates state) {
  if (next_check_ > 0 && time(NULL) >= next_check_) {
    RescanState();
  }

  switch (state) {
  case kIdleDimDisabled:
    LOG(INFO) << "Checking disable_idle_dim. Value = " << disable_idle_dim_;
    return(disable_idle_dim_);
  case kIdleBlankDisabled:
    LOG(INFO) << "Checking disable_idle_blank. Value = "
              << disable_idle_blank_;
    return(disable_idle_blank_);
  case kIdleSuspendDisabled:
    LOG(INFO) << "Checking disable_idle_suspend. Value = "
              << disable_idle_suspend_;
    return(disable_idle_suspend_);
  case kLidSuspendDisabled:
    LOG(INFO) << "Checking disable_lid_suspend. Value = "
              << disable_lid_suspend_;
    return(disable_lid_suspend_);
  default:
    LOG(ERROR) << "Invalid state " << state << " passed to IsStateDisabled";
    return false;
  }
}

void StateControl::ReadSettings(PowerPrefs* prefs) {
  int64 value;

  if (prefs->GetInt64(kStateMaxDisabledDurationSec, &value)) {
    LOG(INFO) << "Read in " << value;
    max_duration_ = static_cast<unsigned int>(value);
  }
}

}  // namespace power_manager
