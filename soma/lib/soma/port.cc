// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/port.h"

#include <limits>
#include <set>
#include <string>

#include <base/logging.h>
#include <base/values.h>

namespace soma {
namespace parser {
namespace port {
const char kListKey[] = "ports";
const char kPortKey[] = "port";
const char kProtocolKey[] = "protocol";
const char kTcpProtocol[] = "tcp";
const char kUdpProtocol[] = "udp";
const Number kWildcard = -1;

namespace {
bool IsValid(Number port) {
  return port == kWildcard ||
      (port >= 0 && port <= std::numeric_limits<uint16_t>::max());
}


}  // namespace

bool ParseList(const base::ListValue& listen_ports,
               std::set<Number>* tcp_ports, std::set<Number>* udp_ports) {
  DCHECK(tcp_ports);
  DCHECK(udp_ports);
  for (const base::Value* port_value : listen_ports) {
    const base::DictionaryValue* port_spec = nullptr;
    if (!port_value->GetAsDictionary(&port_spec)) {
      LOG(ERROR) << "Ports must be specified in a dictionary, not "
                 << port_value;
      return false;
    }
    std::string protocol;
    if (!port_spec->GetString(kProtocolKey, &protocol)) {
      LOG(ERROR) << "Port protocol must be a string, not " << port_spec;
      return false;
    }
    std::set<Number>* to_update = nullptr;
    if (protocol == kTcpProtocol) {
      to_update = tcp_ports;
    } else if (protocol == kUdpProtocol) {
      to_update = udp_ports;
    } else {
      LOG(ERROR) << "Port protocol must be 'tcp' or 'udp', not " << protocol;
      return false;
    }
    Number port = 0;
    if (!port_spec->GetInteger(kPortKey, &port) || !IsValid(port)) {
      LOG(ERROR) << "Listen ports must be uint16 or -1, not " << port;
      return false;
    }
    // If kWildcard gets added, anything else is redundant.
    if (port == kWildcard) {
      *to_update = {kWildcard};
      break;
    }
    to_update->insert(port);
  }
  return true;
}
}  // namespace port
}  // namespace parser
}  // namespace soma
