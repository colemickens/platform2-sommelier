// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_instance.h"

namespace buffet {

CommandInstance::CommandInstance(const std::string& name,
                                 const std::string& category,
                                 const native_types::Object& parameters)
    : name_(name), category_(category), parameters_(parameters) {
}

std::shared_ptr<const PropValue> CommandInstance::FindParameter(
    const std::string& name) const {
  std::shared_ptr<const PropValue> value;
  auto p = parameters_.find(name);
  if (p != parameters_.end())
    value = p->second;
  return value;
}


}  // namespace buffet
