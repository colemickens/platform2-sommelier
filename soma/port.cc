// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/port.h"

#include <limits>
#include <set>

#include <base/logging.h>
#include <base/values.h>

namespace soma {
namespace parser {
namespace port {
const char kListKey[] = "listen ports";
const Number kWildcard = -1;

namespace {
bool IsValid(Number port) {
  return port == kWildcard ||
      (port >= 0 && port <= std::numeric_limits<uint16_t>::max());
}
}  // namespace

std::set<Number> ParseList(base::ListValue* listen_ports) {
  std::set<Number> to_return;
  for (base::Value* port_value : *listen_ports) {
    Number port = 0;
    if (!port_value->GetAsInteger(&port) || !IsValid(port)) {
      LOG(ERROR) << "Listen ports must be uint16 or -1.";
      continue;
    }
    // If kWildcard gets added, anything else is redundant.
    if (port == kWildcard) {
      to_return = {kWildcard};
      break;
    }
    to_return.insert(port);
  }
  return to_return;
}
}  // namespace port
}  // namespace parser
}  // namespace soma
