// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <set>

#include "base/logging.h"

#include "permission_broker/firewall.h"

namespace permission_broker {

class FakeFirewall : public Firewall {
 public:
  FakeFirewall() = default;
  ~FakeFirewall() = default;

 private:
  // The fake's implementation always succeeds.
  int RunInMinijail(const std::vector<std::string>& argv) override {
    return 0;
  }

  DISALLOW_COPY_AND_ASSIGN(FakeFirewall);
};

}  // namespace permission_broker

struct Environment {
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  permission_broker::FakeFirewall fake_firewall;
  FuzzedDataProvider data_provider(data, size);

  std::set<uint16_t> tcp_ports;
  std::set<uint16_t> udp_ports;

  // How many ports should we try?
  uint8_t num_ports = data_provider.ConsumeIntegral<uint8_t>();
  for (size_t i = 0; i < num_ports; i++) {
    bool is_tcp = data_provider.ConsumeBool();
    uint16_t port = data_provider.ConsumeIntegral<uint16_t>();

    if (!is_tcp && port == 0) {
      // Did we run out of data? Consume another bool to check.
      if (!data_provider.ConsumeBool())
        break;
    }

    bool do_add = true;

    if ((is_tcp && tcp_ports.count(port) == 0) ||
        (!is_tcp && udp_ports.count(port) == 0)) {
      // Port does not exist.
      // With small probability, hit the error case: delete a port that doesn't
      // exist.
      do_add = data_provider.ConsumeIntegral<uint8_t>() < 0xFF;
    } else {
      // Port exists.
      // With small probability, hit the error case: add a port that already
      // exists.
      do_add = data_provider.ConsumeIntegral<uint8_t>() == 0xFF;
    }

    if (do_add) {
      fake_firewall.AddAcceptRules(is_tcp ? permission_broker::kProtocolTcp
                                          : permission_broker::kProtocolUdp,
                                   port, "iface");
    } else {
      fake_firewall.DeleteAcceptRules(is_tcp ? permission_broker::kProtocolTcp
                                             : permission_broker::kProtocolUdp,
                                      port, "iface");
    }
  }
  return 0;
}
