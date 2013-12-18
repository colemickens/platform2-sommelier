// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/action_recorder.h"

#include <cstdarg>

namespace power_manager {

std::string JoinActions(const char* action, ...) {
  std::string actions;

  va_list arg_list;
  va_start(arg_list, action);
  while (action) {
    if (!actions.empty())
      actions += ",";
    actions += action;
    action = va_arg(arg_list, const char*);
  }
  va_end(arg_list);
  return actions;
}

ActionRecorder::ActionRecorder() {}

ActionRecorder::~ActionRecorder() {}

std::string ActionRecorder::GetActions() {
  std::string actions = actions_;
  actions_.clear();
  return actions;
}

void ActionRecorder::AppendAction(const std::string& new_action) {
  if (!actions_.empty())
    actions_ += ",";
  actions_ += new_action;
}

}  // namespace power_manager
