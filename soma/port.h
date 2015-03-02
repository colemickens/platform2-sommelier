// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_PORT_H_
#define SOMA_PORT_H_

#include <set>

namespace base {
class ListValue;
}

namespace soma {
namespace listen_port {

using Number = int;

extern const char kListKey[];
extern const Number kWildcard;

std::set<Number> ParseList(base::ListValue* listen_ports);

}  // namespace listen_port
}  // namespace soma
#endif  // SOMA_PORT_H_
