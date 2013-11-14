// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/test_util.h"

#include <cstdarg>

namespace power_manager {
namespace test {

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

void AppendAction(std::string* current_actions, const std::string& action) {
  if (!current_actions->empty())
    *current_actions += ",";
  *current_actions += action;
}

}  // namespace test
}  // namespace power_manager
