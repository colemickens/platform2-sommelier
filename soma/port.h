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
namespace parser {
namespace port {

using Number = int;

extern const char kListKey[];
extern const char kPortKey[];
extern const char kProtocolKey[];
extern const char kTcpProtocol[];
extern const char kUdpProtocol[];
extern const Number kWildcard;

void ParseList(const base::ListValue* listen_ports,
               std::set<Number>* tcp_ports, std::set<Number>* udp_ports);

}  // namespace port
}  // namespace parser
}  // namespace soma
#endif  // SOMA_PORT_H_
